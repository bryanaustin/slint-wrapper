# slint-wrapper-test

A C++20 headless test utility that compiles a Slint UI file, renders it using a software renderer, and saves a PNG screenshot — no display server required. Accepts the same JSON stdin protocol as `slint-wrapper` during a 2-second setup window before the screenshot is taken.

## How it works

1. Pass a `.slint` file and an output PNG path as arguments.
2. `slint-wrapper-test` instantiates the UI on a custom headless platform backed by Slint's `SoftwareRenderer`.
3. For 2 seconds it reads JSON commands from stdin to set properties or invoke callbacks.
4. After 2 seconds it renders the UI to an RGB8 pixel buffer, writes it as a PNG, and exits.

No display server, Wayland compositor, or X11 connection is needed.

## Requirements

- **CMake** ≥ 3.16
- **C++20** compiler (GCC or Clang)
- **jsoncpp** — system package
  - Debian/Ubuntu: `sudo apt install libjsoncpp-dev`
  - Fedora: `sudo dnf install jsoncpp-devel`
- **libpng** — system package
  - Debian/Ubuntu: `sudo apt install libpng-dev`
  - Fedora: `sudo dnf install libpng-devel`
- **Slint** interpreter library — bundled in `lib/` (no separate install needed)

## Build

```bash
cmake -B build -S .
cmake --build build
```

The binary is written to `bin/slint-wrapper-test`.

## Usage

```bash
slint-wrapper-test [--width <pixels>] [--height <pixels>] <path-to-slint-file> <screenshot-output-path>
```

### Arguments

| Argument | Description |
|----------|-------------|
| `<path-to-slint-file>` | Path to the root `.slint` file to render |
| `<screenshot-output-path>` | Path where the PNG screenshot will be written |

### Options

| Option | Default | Description |
|--------|---------|-------------|
| `--width`, `-w` | `800` | Screenshot width in pixels |
| `--height`, `-h` | `600` | Screenshot height in pixels |

### Examples

```bash
# Basic screenshot at default 800×600
./bin/slint-wrapper-test helloworld.slint screenshot.png

# Custom resolution
./bin/slint-wrapper-test --width 1920 --height 1080 myui.slint out.png

# Set a property before the screenshot
echo '{"action":"set","key":"counter","value":42}' | \
    ./bin/slint-wrapper-test simple.slint screenshot.png

# Pipe multiple commands
printf '{"action":"set","key":"title","value":"Hello"}\n{"action":"set","key":"count","value":7}\n' | \
    ./bin/slint-wrapper-test myui.slint out.png
```

## JSON Protocol

Input only — `slint-wrapper-test` produces no stdout output. All commands must arrive within the 2-second window before the screenshot is taken.

### Input (stdin)

| Message | Description |
|---------|-------------|
| `{"action":"set","key":"prop","value":…}` | Set a property on the main component |
| `{"action":"set","key":"Global::prop","value":…}` | Set a property on a global singleton |
| `{"action":"invoke","name":"cb","args":[…]}` | Invoke a callback on the main component |
| `{"action":"invoke","name":"Global::cb","args":[…]}` | Invoke a callback on a global singleton |

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

## License

GPL-3.0 — see [LICENSE](../LICENSE).
