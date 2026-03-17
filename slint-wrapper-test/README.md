# slint-wrapper-test

## Overview
A C++20 headless test utility that compiles a Slint UI file, renders it using a software renderer, and writes a PNG screenshot to disk after 2 seconds. It accepts JSON commands via stdin during the 2-second window to set properties or invoke callbacks before the screenshot is taken.

## Usage

```bash
slint-wrapper-test [--width <pixels>] [--height <pixels>] <path-to-slint-file> <screenshot-output-path>
```

### Arguments
- `<path-to-slint-file>` — path to the root `.slint` file to render
- `<screenshot-output-path>` — path where the PNG screenshot will be written

### Options
- `--width`, `-w` — window and screenshot width in pixels (default: 800)
- `--height`, `-h` — window and screenshot height in pixels (default: 600)

### Input (stdin) — one JSON object per line
- `{"action": "set", "key": "property-name", "value": "new-value"}` — set property
- `{"action": "set", "key": "GlobalName::property-name", "value": "new-value"}` — set global property
- `{"action": "invoke", "name": "callback-name", "args": []}` — invoke callback
- `{"action": "invoke", "name": "GlobalName::callback-name", "args": []}` — invoke global callback

