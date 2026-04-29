# slint-wrapper

A C++20 application that wraps a [Slint](https://slint.dev) UI and exposes it through a JSON-over-stdin/stdout interface. Lets an external process drive a live Slint GUI window — setting properties, invoking callbacks, and receiving change notifications — without requiring direct Slint bindings.

## How it works

1. Pass a `.slint` file as the sole argument.
2. `slint-wrapper` compiles and displays the UI window.
3. Send JSON commands to stdin; property changes and callback events arrive on stdout.

All messages are newline-delimited JSON objects (one per line).

## Requirements

- **CMake** ≥ 3.16
- **C++20** compiler (GCC or Clang)
- **jsoncpp** — system package
  - Debian/Ubuntu: `sudo apt install libjsoncpp-dev`
  - Fedora: `sudo dnf install jsoncpp-devel`
- **Slint** interpreter library — bundled in `lib/` (no separate install needed)

## Build

```bash
cmake -B build -S .
cmake --build build
```

The binary is written to `bin/slint-wrapper`.

## Usage

```bash
slint-wrapper <path-to-slint-file>
```

### Example

```bash
./bin/slint-wrapper helloworld.slint
```

Pipe a sequence of commands:

```bash
cat commands.jsonl | ./bin/slint-wrapper myui.slint
```

## JSON Protocol

All messages are newline-delimited JSON objects — one per line.

### Input (stdin)

| Message | Description |
|---------|-------------|
| `{"action":"set","key":"prop","value":…}` | Set a property on the main component |
| `{"action":"set","key":"Global::prop","value":…}` | Set a property on a global singleton |
| `{"action":"invoke","name":"cb","args":[…]}` | Invoke a callback on the main component |
| `{"action":"invoke","name":"Global::cb","args":[…]}` | Invoke a callback on a global singleton |

### Output (stdout)

| Message | Description |
|---------|-------------|
| `{"action":"set","key":"prop","value":…}` | A property changed in the UI |
| `{"action":"set","key":"Global::prop","value":…}` | A global property changed in the UI |
| `{"action":"callback","name":"cb","args":[…]}` | The UI triggered a callback |
| `{"action":"callback","name":"Global::cb","args":[…]}` | A global callback was triggered |
| `{"action":"result","name":"cb","value":…}` | Return value from an `invoke` command |

Global singletons and their members are addressed with `::` notation: `GlobalName::memberName`.

### Value types

| JSON | Slint |
|------|-------|
| `string` | `string` |
| `number` | `float` / `int` |
| `bool` | `bool` |
| `object` | `struct` |
| `array` | array / model |
| `null` | void |

## Property change detection

`slint-wrapper` polls all properties of the main component and all global singletons every 50 ms. When a value differs from the last observed value, it emits a `set` event to stdout. The initial property snapshot taken at startup is not emitted — only subsequent changes are reported.

## License

GPL-3.0 — see [LICENSE](../LICENSE).
