# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

slint-wrapper-test is a C++20 headless test utility that compiles a Slint UI file, renders it using a software renderer, and writes a PNG screenshot to disk after 2 seconds. It accepts JSON commands via stdin during the 2-second window to set properties or invoke callbacks before the screenshot is taken.

## Build Commands

```bash
# Configure (from project root)
cmake -B build -S .

# Build
cmake --build build

# The binary is output to bin/slint-wrapper-test

# Run (requires a .slint file and output PNG path)
./bin/slint-wrapper-test path/to/file.slint output/screenshot.png

# Run with custom window/screenshot size (default: 800x600)
./bin/slint-wrapper-test --width 1024 --height 768 path/to/file.slint output/screenshot.png
```

## Manual Testing

No automated test suite. Test manually using the example `.slint` files:

```bash
# Basic screenshot — renders the UI and saves a PNG after 2 seconds
./bin/slint-wrapper-test helloworld.slint screenshot.png

# Set properties before screenshot via stdin
echo '{"action":"set","key":"counter","value":42}' | ./bin/slint-wrapper-test simple_test.slint screenshot.png
```

Example `.slint` files: `helloworld.slint`, `example-callback.slint`, `test_callbacks.slint`, `simple_test.slint`, `property_test.slint`, `property_change_test.slint`, `struct_test.slint`.

## Dependencies

- **Slint** (interpreter library) — bundled in `lib/` with headers in `include/slint/` and CMake config in `lib/cmake/`
- **jsoncpp** — system dependency, found via pkg-config
- **libpng** — system dependency, found via pkg-config
- **CMake** ≥ 3.16

## Architecture

Single-file C++ application (`src/main.cpp`) with one build target: `slint-wrapper-test`.

**Runtime flow:**
1. Takes a `.slint` file path and a PNG output path as arguments
2. Sets up a headless platform with a `SoftwareRenderer` (no display server needed)
3. Uses `slint::interpreter::ComponentCompiler` to dynamically compile and instantiate the Slint UI
4. Spawns a stdin reader thread that parses JSON commands into a thread-safe queue
5. A `slint::Timer` (50ms interval) polls the queue and dispatches commands on the UI thread
6. After 2 seconds, renders the UI to an RGB8 pixel buffer, writes it as PNG, and exits

**Key conversion function:**
- `json_to_slint_value` — converts `Json::Value` to Slint `Value` types

**Headless platform:**
- `HeadlessPlatform` / `HeadlessWindowAdapter` — custom Slint platform using `SoftwareRenderer` for offscreen rendering
- Default window size is 800x600 pixels, configurable via `--width` / `-w` and `--height` / `-h` CLI options
- Sets `QT_QPA_PLATFORM=offscreen` to prevent the bundled Qt backend from trying to connect to a display server

**Threading model:**
- The stdin reader thread uses non-blocking I/O and pushes parsed JSON into a `std::queue` protected by a mutex
- The UI thread polls this queue via a `slint::Timer` every 50ms — all Slint API calls happen on the UI thread
- `std::atomic<bool> should_exit` coordinates shutdown after the event loop ends

**JSON protocol** (newline-delimited, one JSON object per line):

Input only (no stdout output):
- `{"action": "set", "key": "property-name", "value": "new-value"}` — set property
- `{"action": "set", "key": "GlobalName::property-name", "value": "new-value"}` — set global property
- `{"action": "invoke", "name": "callback-name", "args": []}` — invoke callback
- `{"action": "invoke", "name": "GlobalName::callback-name", "args": []}` — invoke global callback

**Known bug (`fix.md`):** `HeadlessWindowAdapter::request_redraw()` is a no-op, so property changes via stdin JSON commands are never reflected in the output PNG. The `SoftwareRenderer` dirty region is only set once during `show()` and never updated. Fix: implement `request_redraw()` to call `m_renderer.mark_dirty_region()` over the full window.

**Build warnings:** Compiled with `-Wall -Wextra -Wpedantic` (GCC/Clang). Keep the build warning-free.
