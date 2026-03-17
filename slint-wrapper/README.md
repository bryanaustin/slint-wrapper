# slint-wrapper

## Overview
In this directory is a C++ application that wraps a Slint UI and normalizes changes over stdin and stdout. This is intended to be used by another application that does the work under the hood.


## Arguments
This application only accepts a single argument, the path to the root slint file.

### Input (stdin) — one JSON object per line
- `{"action": "set", "key": "property-name", "value": "new-value"}` — set property
- `{"action": "set", "key": "GlobalName::property-name", "value": "new-value"}` — set global property
- `{"action": "invoke", "name": "callback-name", "args": []}` — invoke callback
- `{"action": "invoke", "name": "GlobalName::callback-name", "args": []}` — invoke global callback

### Output (stdout) — one JSON object per line
- `{"action": "set", "key": "property-name", "value": "new-value"}` — property changed in UI
- `{"action": "set", "key": "GlobalName::property-name", "value": "new-value"}` — global property changed in UI
- `{"action": "callback", "name": "callback-name", "args": [...]}` — UI callback triggered
- `{"action": "result", "name": "callback-name", "value": ...}` — invoke return value

