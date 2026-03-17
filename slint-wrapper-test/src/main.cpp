#include <slint-interpreter.h>
#include <slint-platform.h>
#include <slint_timer.h>
#include <json/json.h>
#include <png.h>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <vector>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

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
        slint::interpreter::Struct s;
        for (const auto& member : json.getMemberNames()) {
            s.set_field(member, json_to_slint_value(json[member]));
        }
        return slint::interpreter::Value(s);
    }
    return slint::interpreter::Value();
}

// Thread-safe queue for JSON commands
std::queue<Json::Value> command_queue;
std::mutex queue_mutex;
std::atomic<bool> should_exit(false);

// Function to read stdin in a separate thread
void stdin_reader_thread() {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    std::string buffer;
    char ch;

    while (!should_exit.load()) {
        ssize_t bytes = read(STDIN_FILENO, &ch, 1);

        if (bytes > 0) {
            if (ch == '\n') {
                if (!buffer.empty()) {
                    Json::CharReaderBuilder reader_builder;
                    Json::Value json_cmd;
                    std::string errors;
                    std::istringstream stream(buffer);

                    if (Json::parseFromStream(reader_builder, stream, &json_cmd, &errors)) {
                        std::lock_guard<std::mutex> lock(queue_mutex);
                        command_queue.push(json_cmd);
                    }
                    buffer.clear();
                }
            } else {
                buffer += ch;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

// Function to process JSON commands (set and invoke only, no stdout output)
void process_json_command(const Json::Value& cmd,
                          const slint::ComponentHandle<slint::interpreter::ComponentInstance>& instance) {
    if (!cmd.isObject()) return;

    std::string action = cmd.get("action", "").asString();

    if (action == "set") {
        std::string key = cmd.get("key", "").asString();
        Json::Value value = cmd.get("value", Json::nullValue);

        if (!key.empty() && !value.isNull()) {
            size_t separator_pos = key.find("::");
            if (separator_pos != std::string::npos) {
                std::string global_name = key.substr(0, separator_pos);
                std::string prop_name = key.substr(separator_pos + 2);
                instance->set_global_property(global_name, prop_name, json_to_slint_value(value));
            } else {
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

            size_t separator_pos = name.find("::");
            if (separator_pos != std::string::npos) {
                std::string global_name = name.substr(0, separator_pos);
                std::string func_name = name.substr(separator_pos + 2);
                instance->invoke_global(global_name, func_name,
                    std::span<const slint::interpreter::Value>(slint_args.begin(), slint_args.end()));
            } else {
                instance->invoke(name,
                    std::span<const slint::interpreter::Value>(slint_args.begin(), slint_args.end()));
            }
        }
    }
}

// Write RGB8 pixel buffer to PNG file
bool write_png(const std::string& filename, const slint::Rgb8Pixel* pixels,
               uint32_t width, uint32_t height, uint32_t stride) {
    FILE* fp = fopen(filename.c_str(), "wb");
    if (!fp) {
        std::cerr << "Error: cannot open " << filename << " for writing" << std::endl;
        return false;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        fclose(fp);
        return false;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, nullptr);
        fclose(fp);
        return false;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return false;
    }

    png_init_io(png, fp);
    png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    for (uint32_t y = 0; y < height; ++y) {
        png_write_row(png, reinterpret_cast<const png_byte*>(pixels + y * stride));
    }

    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    fclose(fp);
    return true;
}

// Headless window adapter using SoftwareRenderer
class HeadlessWindowAdapter : public slint::platform::WindowAdapter {
    slint::platform::SoftwareRenderer m_renderer;
    slint::PhysicalSize m_size;
    bool m_visible = false;

public:
    HeadlessWindowAdapter(uint32_t width, uint32_t height)
        : m_renderer(slint::platform::SoftwareRenderer::RepaintBufferType::NewBuffer)
        , m_size({ width, height })
    {
    }

    slint::platform::AbstractRenderer& renderer() override { return m_renderer; }
    slint::PhysicalSize size() override { return m_size; }

    void set_visible(bool v) override {
        m_visible = v;
        if (v) {
            window().dispatch_resize_event(slint::LogicalSize({ static_cast<float>(m_size.width),
                                                                 static_cast<float>(m_size.height) }));
        }
    }

    void set_size(slint::PhysicalSize size) override {
        m_size = size;
        window().dispatch_resize_event(slint::LogicalSize({ static_cast<float>(size.width),
                                                             static_cast<float>(size.height) }));
    }

    void request_redraw() override { }

    // Render to a pixel buffer and save as PNG
    bool render_to_png(const std::string& path) {
        uint32_t width = m_size.width;
        uint32_t height = m_size.height;
        std::vector<slint::Rgb8Pixel> buffer(width * height);
        m_renderer.render(std::span<slint::Rgb8Pixel>(buffer.data(), buffer.size()), width);
        return write_png(path, buffer.data(), width, height, width);
    }
};

// Headless platform
class HeadlessPlatform : public slint::platform::Platform {
    std::vector<slint::platform::Platform::Task> pending_tasks;
    std::mutex task_mutex;
    std::atomic<bool> m_quit { false };

public:
    HeadlessWindowAdapter* last_adapter = nullptr;
    uint32_t m_width;
    uint32_t m_height;

    HeadlessPlatform(uint32_t width, uint32_t height)
        : m_width(width), m_height(height) {}

    std::unique_ptr<slint::platform::WindowAdapter> create_window_adapter() override {
        auto adapter = std::make_unique<HeadlessWindowAdapter>(m_width, m_height);
        last_adapter = adapter.get();
        return adapter;
    }

    void run_event_loop() override {
        while (!m_quit.load()) {
            slint::platform::update_timers_and_animations();

            // Run pending tasks
            {
                std::vector<slint::platform::Platform::Task> tasks;
                {
                    std::lock_guard<std::mutex> lock(task_mutex);
                    std::swap(tasks, pending_tasks);
                }
                for (auto& task : tasks) {
                    std::move(task).run();
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

    void quit_event_loop() override {
        m_quit.store(true);
    }

    void run_in_event_loop(Task task) override {
        std::lock_guard<std::mutex> lock(task_mutex);
        pending_tasks.push_back(std::move(task));
    }
};

int main(int argc, char* argv[]) {
    uint32_t width = 800;
    uint32_t height = 600;
    std::vector<std::string> positional_args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--width" || arg == "-w") && i + 1 < argc) {
            width = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if ((arg == "--height" || arg == "-h") && i + 1 < argc) {
            height = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else {
            positional_args.push_back(arg);
        }
    }

    if (positional_args.size() != 2) {
        std::cerr << "Usage: " << argv[0]
                  << " [--width <pixels>] [--height <pixels>]"
                  << " <path-to-slint-file> <screenshot-output-path>" << std::endl;
        return 1;
    }

    const std::string slint_file_path = positional_args[0];
    const std::string screenshot_path = positional_args[1];

    // Prevent the Slint library's bundled Qt backend from trying to connect
    // to a display server — we use our own headless platform instead.
    setenv("QT_QPA_PLATFORM", "offscreen", 0);

    // Set up headless platform before any Slint operations
    auto platform = std::make_unique<HeadlessPlatform>(width, height);
    auto* platform_ptr = platform.get();
    slint::platform::set_platform(std::move(platform));

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
        auto diagnostics = compiler.diagnostics();
        for (const auto& diag : diagnostics) {
            std::cerr << diag.message << std::endl;
        }
        return 1;
    }

    // Create component instance
    auto instance = definition->create();

    // Show the window (triggers the headless adapter)
    instance->show();

    // Start stdin reader thread
    std::thread stdin_thread(stdin_reader_thread);

    // Set up a timer to process commands from stdin
    slint::Timer command_processor;
    command_processor.start(slint::TimerMode::Repeated, std::chrono::milliseconds(50),
        [instance]() {
            std::lock_guard<std::mutex> lock(queue_mutex);
            while (!command_queue.empty()) {
                Json::Value cmd = command_queue.front();
                command_queue.pop();
                process_json_command(cmd, instance);
            }
        });

    // Set up a single-shot timer to take screenshot after 2 seconds and exit
    slint::Timer screenshot_timer;
    screenshot_timer.start(slint::TimerMode::SingleShot, std::chrono::milliseconds(2000),
        [platform_ptr, &screenshot_path]() {
            if (platform_ptr->last_adapter) {
                // Render and save
                slint::platform::update_timers_and_animations();
                if (!platform_ptr->last_adapter->render_to_png(screenshot_path)) {
                    std::cerr << "Error: failed to write screenshot to " << screenshot_path << std::endl;
                }
            }
            slint::quit_event_loop();
        });

    // Run the event loop - blocks until quit_event_loop() is called
    slint::run_event_loop();

    // Clean up
    should_exit.store(true);
    stdin_thread.join();

    return 0;
}
