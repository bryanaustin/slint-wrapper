#include <slint-interpreter.h>
#include <slint_timer.h>
#include <json/json.h>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <map>
#include <fcntl.h>
#include <unistd.h>

// Helper function to convert Slint Value to Json::Value
Json::Value slint_value_to_json(const slint::interpreter::Value& value) {
    if (auto str = value.to_string()) {
        return Json::Value(std::string(*str));
    } else if (auto num = value.to_number()) {
        return Json::Value(*num);
    } else if (auto boolean = value.to_bool()) {
        return Json::Value(*boolean);
    } else if (auto struct_val = value.to_struct()) {
        Json::Value obj(Json::objectValue);
        // Iterate through all struct fields
        for (auto it = struct_val->begin(); it != struct_val->end(); ++it) {
            auto [field_name, field_value] = *it;
            // Recursively convert nested values
            obj[std::string(field_name)] = slint_value_to_json(field_value);
        }
        return obj;
    } else if (value.to_image()) {
        Json::Value obj(Json::objectValue);
        obj["_type"] = "image";
        return obj;
    } else if (value.to_brush()) {
        Json::Value obj(Json::objectValue);
        obj["_type"] = "brush";
        return obj;
    } else {
        return Json::Value(Json::nullValue);
    }
}

// Helper function to convert Json::Value to Slint Value
slint::interpreter::Value json_to_slint_value(const Json::Value& json) {
    if (json.isNull()) {
        return slint::interpreter::Value();
    } else if (json.isBool()) {
        return slint::interpreter::Value(json.asBool());
    } else if (json.isInt()) {
        return slint::interpreter::Value(static_cast<double>(json.asInt()));
    } else if (json.isUInt()) {
        return slint::interpreter::Value(static_cast<double>(json.asUInt()));
    } else if (json.isDouble()) {
        return slint::interpreter::Value(json.asDouble());
    } else if (json.isString()) {
        return slint::interpreter::Value(slint::SharedString(json.asString()));
    } else if (json.isArray()) {
        slint::SharedVector<slint::interpreter::Value> vec;
        for (const auto& item : json) {
            vec.push_back(json_to_slint_value(item));
        }
        return slint::interpreter::Value(vec);
    } else if (json.isObject()) {
        // For objects, we'll create a struct
        slint::interpreter::Struct s;
        for (const auto& member : json.getMemberNames()) {
            s.set_field(member, json_to_slint_value(json[member]));
        }
        return slint::interpreter::Value(s);
    }
    return slint::interpreter::Value();
}

// Generic callback handler that outputs JSON
auto create_callback_handler(const std::string& callback_name, const std::string& context = "") {
    return [callback_name, context](std::span<const slint::interpreter::Value> args) -> slint::interpreter::Value {
        Json::Value root;
        root["action"] = "callback";
        
        // Add context prefix if it's a global callback
        if (!context.empty()) {
            root["name"] = context + "::" + callback_name;
        } else {
            root["name"] = callback_name;
        }
        
        // Convert arguments to JSON array
        Json::Value args_array(Json::arrayValue);
        for (const auto& arg : args) {
            args_array.append(slint_value_to_json(arg));
        }
        root["args"] = args_array;
        
        // Use FastWriter for single-line output
        Json::FastWriter writer;
        std::cout << writer.write(root);
        std::cout.flush();
        
        // Return void value
        return slint::interpreter::Value();
    };
}

// Thread-safe queue for JSON commands
std::queue<Json::Value> command_queue;
std::mutex queue_mutex;
std::atomic<bool> should_exit(false);

// Cached property values for change detection
std::map<std::string, Json::Value> cached_properties;

// Function to read stdin in a separate thread
void stdin_reader_thread() {
    // Set stdin to non-blocking mode
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    
    std::string buffer;
    char ch;
    
    while (!should_exit.load()) {
        // Try to read a character
        ssize_t bytes = read(STDIN_FILENO, &ch, 1);
        
        if (bytes > 0) {
            if (ch == '\n') {
                // Parse the line as JSON
                if (!buffer.empty()) {
                    Json::CharReaderBuilder reader_builder;
                    Json::Value json_cmd;
                    std::string errors;
                    std::istringstream stream(buffer);
                    
                    if (Json::parseFromStream(reader_builder, stream, &json_cmd, &errors)) {
                        // Add to queue
                        std::lock_guard<std::mutex> lock(queue_mutex);
                        command_queue.push(json_cmd);
                    }
                    buffer.clear();
                }
            } else {
                buffer += ch;
            }
        } else {
            // No data available, sleep briefly
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

// Function to process JSON commands
void process_json_command(const Json::Value& cmd, 
                          const slint::ComponentHandle<slint::interpreter::ComponentInstance>& instance) {
    if (!cmd.isObject()) return;
    
    std::string action = cmd.get("action", "").asString();
    
    if (action == "set") {
        std::string key = cmd.get("key", "").asString();
        Json::Value value = cmd.get("value", Json::nullValue);
        
        if (!key.empty() && !value.isNull()) {
            // Check if it's a global property (contains ::)
            size_t separator_pos = key.find("::");
            if (separator_pos != std::string::npos) {
                // Global property
                std::string global_name = key.substr(0, separator_pos);
                std::string prop_name = key.substr(separator_pos + 2);
                instance->set_global_property(global_name, prop_name, json_to_slint_value(value));
            } else {
                // Regular property
                instance->set_property(key, json_to_slint_value(value));
            }
        }
    } else if (action == "invoke") {
        std::string name = cmd.get("name", "").asString();
        Json::Value args = cmd.get("args", Json::arrayValue);
        
        if (!name.empty()) {
            slint::SharedVector<slint::interpreter::Value> slint_args;
            for (const auto& arg : args) {
                slint_args.push_back(json_to_slint_value(arg));
            }
            
            // Check if it's a global function (contains ::)
            size_t separator_pos = name.find("::");
            if (separator_pos != std::string::npos) {
                // Global function
                std::string global_name = name.substr(0, separator_pos);
                std::string func_name = name.substr(separator_pos + 2);
                auto result = instance->invoke_global(global_name, func_name, 
                    std::span<const slint::interpreter::Value>(slint_args.begin(), slint_args.end()));
                
                if (result) {
                    // Output the result as JSON
                    Json::Value response;
                    response["action"] = "result";
                    response["name"] = name;
                    response["value"] = slint_value_to_json(*result);
                    Json::FastWriter writer;
                    std::cout << writer.write(response);
                    std::cout.flush();
                }
            } else {
                // Regular function/callback
                auto result = instance->invoke(name, 
                    std::span<const slint::interpreter::Value>(slint_args.begin(), slint_args.end()));
                
                if (result) {
                    // Output the result as JSON
                    Json::Value response;
                    response["action"] = "result";
                    response["name"] = name;
                    response["value"] = slint_value_to_json(*result);
                    Json::FastWriter writer;
                    std::cout << writer.write(response);
                    std::cout.flush();
                }
            }
        }
    }
}

// Check all properties for changes and emit "set" actions for any that changed
void poll_property_changes(
    const slint::interpreter::ComponentDefinition& definition,
    const slint::ComponentHandle<slint::interpreter::ComponentInstance>& instance) {

    Json::FastWriter writer;

    // Check main component properties
    auto properties = definition.properties();
    for (const auto& prop : properties) {
        std::string name(prop.property_name);
        auto value = instance->get_property(name);
        if (value) {
            Json::Value json_val = slint_value_to_json(*value);
            auto it = cached_properties.find(name);
            if (it == cached_properties.end() || it->second != json_val) {
                cached_properties[name] = json_val;
                // Only emit if we had a previous value (skip initial population)
                if (it != cached_properties.end()) {
                    Json::Value root;
                    root["action"] = "set";
                    root["key"] = name;
                    root["value"] = json_val;
                    std::cout << writer.write(root);
                    std::cout.flush();
                }
            }
        }
    }

    // Check global properties
    auto globals = definition.globals();
    for (const auto& global_name : globals) {
        std::string global_str(global_name);
        auto global_props = definition.global_properties(global_str);
        if (global_props) {
            for (const auto& prop : *global_props) {
                std::string prop_name(prop.property_name);
                std::string full_key = global_str + "::" + prop_name;
                auto value = instance->get_global_property(global_str, prop_name);
                if (value) {
                    Json::Value json_val = slint_value_to_json(*value);
                    auto it = cached_properties.find(full_key);
                    if (it == cached_properties.end() || it->second != json_val) {
                        cached_properties[full_key] = json_val;
                        if (it != cached_properties.end()) {
                            Json::Value root;
                            root["action"] = "set";
                            root["key"] = full_key;
                            root["value"] = json_val;
                            std::cout << writer.write(root);
                            std::cout.flush();
                        }
                    }
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    // Check command line arguments
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <path-to-slint-file>" << std::endl;
        return 1;
    }
    
    const std::string slint_file_path = argv[1];
    
    // Create component compiler
    slint::interpreter::ComponentCompiler compiler;
    
    // Add include path for any imports
    std::string dir_path = slint_file_path;
    auto last_slash = dir_path.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        dir_path = dir_path.substr(0, last_slash);
        slint::SharedVector<slint::SharedString> paths;
        paths.push_back(slint::SharedString(dir_path));
        compiler.set_include_paths(paths);
    }
    
    // Build component from file
    auto definition = compiler.build_from_path(slint_file_path);
    
    if (!definition) {
        // Print diagnostics
        auto diagnostics = compiler.diagnostics();
        for (const auto& diag : diagnostics) {
            std::cerr << diag.message << std::endl;
        }
        return 1;
    }
    
    // Create component instance
    auto instance = definition->create();
    
    // Set up callbacks for the main component
    auto callbacks = definition->callbacks();
    
    for (const auto& callback_name : callbacks) {
        std::string name_str(callback_name);
        instance->set_callback(name_str, create_callback_handler(name_str));
    }
    
    // Set up callbacks for global singletons
    auto globals = definition->globals();
    
    for (const auto& global_name : globals) {
        std::string global_str(global_name);
        auto global_callbacks = definition->global_callbacks(global_str);
        
        if (global_callbacks && !global_callbacks->empty()) {
            for (const auto& callback_name : *global_callbacks) {
                std::string callback_str(callback_name);
                instance->set_global_callback(
                    global_str, 
                    callback_str, 
                    create_callback_handler(callback_str, global_str)
                );
            }
        }
    }
    
    // Populate initial property cache
    poll_property_changes(*definition, instance);

    // Show the window
    instance->show();

    // Start stdin reader thread
    std::thread stdin_thread(stdin_reader_thread);

    // Create a timer to process commands from the queue and poll for property changes
    slint::Timer command_processor;
    auto def_copy = *definition;
    command_processor.start(slint::TimerMode::Repeated, std::chrono::milliseconds(50),
        [instance, def_copy]() {
            // Process all pending commands
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                while (!command_queue.empty()) {
                    Json::Value cmd = command_queue.front();
                    command_queue.pop();
                    process_json_command(cmd, instance);
                }
            }
            // Check for property changes
            poll_property_changes(def_copy, instance);
        });
    
    // Run the event loop - this will block until the window is closed
    slint::run_event_loop();
    
    // Clean up
    should_exit.store(true);
    stdin_thread.join();
    
    return 0;
}