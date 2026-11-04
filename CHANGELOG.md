# Changelog

All notable changes to Lucid are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Public documentation site built with MkDocs Material, including a landing page, screenshots and a demo.
- CodeQL, OpenSSF Scorecard and dependency-review workflows, plus `SUPPORT.md` and a public security policy.

### Changed

- README now includes screenshots, a demo, documentation and community links.
- Removed private-repository wording from release notes and documentation.

## [0.2.1-beta.2] - 2026-11-03

### Fixed

- Reduced overshoot in menu selection highlighting.
- Improved Gaussian blur and scene resolution on standard-density displays.
- Let a selected background override a startup preview and enable background motion when selecting a scene.

## [0.2.1-beta.1] - 2026-11-03

### Added

- Nine selectable, animated 3D background scenes with primary-color tinting, Gaussian blur, and an option to disable motion.
- C/C++ language-server integration through a locally installed `clangd`, including completion, diagnostics, navigation, references and formatting.
- Project-wide search and reviewed replacement, recent-project and open-file session restore, and saved-file history.
- Git history and reviewed fetch, fast-forward pull and push for a configured upstream.
- Scrollable file and project suggestions with subtle animated edge cues, and welcome and status transitions.

### Changed

- Improved save checks, recovery snapshots and preservation of supported file metadata. Recovery remains periodic and is not a substitute for backups.
- Improved local AI model download feedback and error reporting, local prompts, and read-only response scrolling.
- Refined file icons, keyboard shortcuts, compile output selection, debugger handling and settings.

### Limitations

- Multiplayer and the full module runtime are not available. Debugger, BSD desktop, accessibility and other platform-specific workflows need further validation.
- Packages are unsigned for distribution. The installer script downloads release assets directly.

## [0.2.0-beta.1] - 2026-11-03

### Added

- LLDB debug-adapter sessions with breakpoints, stepping, stack frames, variables, task-linked launch settings, and opt-in paused-state explanations.
- Git review actions to stage and unstage files, mark edited conflicts resolved, and review staged paths before creating a local commit.
- Keyboard navigation of settings controls using Tab, arrow keys, Enter and Space.
- A temporary open-file switcher with per-file cursor, scroll position and undo history.
- A full-page settings view, adjustable text/UI scale, and three running-app icon themes that follow the primary color.
- File-opening support for OS launch and drag-and-drop, and installation-time Open With registration.
- A themed macOS DMG, Windows MSI, and Linux installer script alongside standalone archives.
- Native FreeBSD and OpenBSD archives; system graphics and cryptography libraries are required.
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

[Unreleased]: https://github.com/initializeddd/Lucid/compare/v0.2.1-beta.2...HEAD
[0.2.1-beta.2]: https://github.com/initializeddd/Lucid/releases/tag/v0.2.1-beta.2
[0.2.1-beta.1]: https://github.com/initializeddd/Lucid/releases/tag/v0.2.1-beta.1
[0.2.0-beta.1]: https://github.com/initializeddd/Lucid/releases/tag/v0.2.0-beta.1
[0.1.0-beta.4]: https://github.com/initializeddd/Lucid/releases/tag/v0.1.0-beta.4
[0.1.0-beta.3]: https://github.com/initializeddd/Lucid/releases/tag/v0.1.0-beta.3
[0.1.0-beta.2]: https://github.com/initializeddd/Lucid/releases/tag/v0.1.0-beta.2
[0.1.0-beta.1]: https://github.com/initializeddd/Lucid/releases/tag/v0.1.0-beta.1
[0.1.0]: https://github.com/initializeddd/Lucid/releases/tag/v0.1.0
