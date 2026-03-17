# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

slint-wrapper is a C++20 application that wraps a Slint UI and provides a JSON-over-stdin/stdout interface. It allows an external process to control a Slint GUI by sending JSON commands via stdin and receiving callback notifications via stdout.

## Build Commands

```bash
# Configure (from project root)
cmake -B build -S .

# Build
cmake --build build

# The binary is output to bin/slint-wrapper

# Run (requires a .slint file as argument)
./bin/slint-wrapper path/to/file.slint
```

## Manual Testing

No automated test suite. Test manually using the example `.slint` files and JSON inputs:

```bash
# Interactive test — type JSON commands into stdin, observe stdout
./bin/slint-wrapper helloworld.slint

# Pipe commands from a file
cat test_commands.json | ./bin/slint-wrapper struct_test.slint
```

Example `.slint` files: `helloworld.slint`, `example-callback.slint`, `test_callbacks.slint`, `simple_test.slint`, `property_test.slint`, `property_change_test.slint`, `struct_test.slint`.

## Dependencies

- **Slint** (interpreter library) — bundled in `lib/` with headers in `include/slint/` and CMake config in `lib/cmake/`
- **jsoncpp** — system dependency, found via pkg-config
- **CMake** ≥ 3.16

## Architecture

Single-file C++ application (`src/main.cpp`) with one build target: `slint-wrapper`.

**Runtime flow:**
1. Takes a `.slint` file path as its sole argument
2. Uses `slint::interpreter::ComponentCompiler` to dynamically compile and instantiate the Slint UI
3. Auto-registers callback handlers on all callbacks (main component + globals) that emit JSON to stdout
4. Spawns a stdin reader thread that parses JSON commands into a thread-safe queue
5. A `slint::Timer` (50ms interval) polls the queue and dispatches commands on the UI thread

**Key conversion functions:**
- `slint_value_to_json` / `json_to_slint_value` — bidirectional conversion between Slint `Value` types and `Json::Value`

**Threading model:**
- The stdin reader thread uses non-blocking I/O and pushes parsed JSON into a `std::queue` protected by a mutex
- The UI thread polls this queue via a `slint::Timer` every 50ms — all Slint API calls happen on the UI thread
- `std::atomic<bool> should_exit` coordinates shutdown after the event loop ends

**JSON protocol** (newline-delimited, one JSON object per line):

Input:
- `{"action": "set", "key": "property-name", "value": "new-value"}` — set property
- `{"action": "set", "key": "GlobalName::property-name", "value": "new-value"}` — set global property
- `{"action": "invoke", "name": "callback-name", "args": []}` — invoke callback
- `{"action": "invoke", "name": "GlobalName::callback-name", "args": []}` — invoke global callback

Output:
- `{"action": "set", "key": "property-name", "value": "new-value"}` — property changed in UI
- `{"action": "set", "key": "GlobalName::property-name", "value": "new-value"}` — global property changed in UI
- `{"action": "callback", "name": "callback-name", "args": [...]}` — UI callback triggered
- `{"action": "result", "name": "callback-name", "value": ...}` — invoke return value

**Build warnings:** Compiled with `-Wall -Wextra -Wpedantic` (GCC/Clang). Keep the build warning-free.
