# slint-wrapper

Utilities for driving a [Slint](https://slint.dev) UI from an external process via a JSON-over-stdin/stdout protocol.

## Projects

| Project | Description |
|---------|-------------|
| [`slint-wrapper`](slint-wrapper/) | Live GUI wrapper — keeps a Slint window open, receives JSON commands via stdin, and emits property changes and callback events to stdout |
| [`slint-wrapper-test`](slint-wrapper-test/) | Headless screenshot tool — renders a Slint UI to a PNG without a display server, useful for visual regression testing |

Both projects share the same JSON command protocol, so the same input commands work in both live and headless contexts.

## License

GPL-3.0 — see [LICENSE](LICENSE).
