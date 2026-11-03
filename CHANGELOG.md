# Changelog

All notable changes to Lucid are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.0-beta.1] - Unreleased

### Added

- LLDB debug-adapter sessions with breakpoints, stepping, stack frames, variables, task-linked launch settings, and opt-in paused-state explanations.
- Git review actions to stage and unstage files, mark edited conflicts resolved, and review staged paths before creating a local commit.
- Keyboard navigation of settings controls using Tab, arrow keys, Enter and Space.
- A temporary open-file switcher with per-file cursor, scroll position and undo history.
- A full-page settings view, adjustable text/UI scale, and three running-app icon themes that follow the primary color.
- File-opening support for OS launch and drag-and-drop, and installation-time Open With registration.
- A themed macOS DMG, Windows MSI configuration, and per-user Linux installer script alongside standalone archives.
- Find and replace text in the current file, including next/previous and wraparound.
- Word and line selection by repeated clicks, word drag selection, and word navigation.
- Cursor-side function suggestions with Ctrl+Shift+Space (Command+Shift+Space on macOS).
- Optional local AI with a bundled inference engine and a verified model downloaded on first use.
  Suggestions are shown for review before insertion.

## [0.1.0-beta.4] - 2026-11-02

### Changed

- Windows and Linux keycaps show the Control symbol next to `Ctrl`; macOS keeps the Command glyph.
- Documentation writes shortcuts with `Ctrl` and `Alt` and maps macOS Command/Option explicitly.

## [0.1.0-beta.3] - 2026-11-02

### Fixed

- Release builds bundle the JetBrains Mono font in `assets/fonts`. Font discovery now falls back
  to it relative to the executable, so Lucid starts on systems with no installed fonts.

## [0.1.0-beta.2] - 2026-11-02

### Fixed

- Linux release tarballs are now self-contained: `libglfw`, the Vulkan loader and the X11 runtime
  libraries are bundled in `lib/` and resolved through an `$ORIGIN/lib` runtime path.
  No dependency installation is required beyond a working Vulkan driver.

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

[Unreleased]: https://github.com/initializeddd/Lucid/compare/v0.1.0-beta.4...HEAD
[0.1.0-beta.4]: https://github.com/initializeddd/Lucid/releases/tag/v0.1.0-beta.4
[0.1.0-beta.3]: https://github.com/initializeddd/Lucid/releases/tag/v0.1.0-beta.3
[0.1.0-beta.2]: https://github.com/initializeddd/Lucid/releases/tag/v0.1.0-beta.2
[0.1.0-beta.1]: https://github.com/initializeddd/Lucid/releases/tag/v0.1.0-beta.1
[0.1.0]: https://github.com/initializeddd/Lucid/releases/tag/v0.1.0
