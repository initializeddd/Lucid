# Changelog

All notable changes to Lucid are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0-beta.1] - 2026-11-02

First public beta. Download builds for macOS, Linux and Windows from the
[releases page](https://github.com/initializeddd/Lucid/releases/tag/v0.1.0-beta.1).

### Added

- Beta channel is surfaced in the window title, status bar, About panel and platform installers.
- The running version (`0.1.0-beta.1`) is shown in the status bar and written to the log.
- Path Lens command surface with path completion, folder stepping and create-file/create-folder
  affordances.
- GPU-rendered editor surface with line-number gutter, animated caret, status line and C++/JSON
  syntax highlighting.
- UTF-8 document model with selection, undo/redo, clipboard, duplicate-line and auto-save.
- Inline compile functions with platform filters, variable expansion, a movable PTY terminal and a
  project-wide function search.
- File-backed settings with live JSON validation, theme presets and a custom accent color.
- Native macOS app bundle, Windows resources and Linux desktop packaging.
- Automated CI builds and tests on macOS, Linux and Windows.

### Fixed

- Linux builds now compile the platform-neutral command and shift keycaps.
- Windows builds now compile the PTY stub and use `_popen`/`_pclose` for compiler diagnostics.
- MSVC compiles with `/utf-8`, keeping Unicode documents intact on Windows.

## [0.1.0]

- Initial public foundation: Vulkan renderer, window layer, editor core and build system.

[Unreleased]: https://github.com/initializeddd/Lucid/compare/v0.1.0-beta.1...HEAD
[0.1.0-beta.1]: https://github.com/initializeddd/Lucid/releases/tag/v0.1.0-beta.1
[0.1.0]: https://github.com/initializeddd/Lucid/releases/tag/v0.1.0
