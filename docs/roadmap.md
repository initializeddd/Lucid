# Roadmap

These are development themes, not promised release dates or version numbers. The checklists
describe the current source tree; a published beta can have a different feature set. See the
[changelog](https://github.com/initializeddd/Lucid/blob/main/CHANGELOG.md) for released versions
and their changes.

## Foundation

- [x] Project structure and build system
- [x] GLFW window with a Vulkan surface
- [x] MoltenVK-compatible instance, device and swapchain
- [x] Resize handling and a dark, sRGB-aware clear pass
- [x] Validation layer wiring

## Rendering foundations

- [ ] Render hardware interface (`renderer/rhi`)
- [ ] Pipeline cache
- [x] Shader compilation pipeline
- [ ] Fullscreen triangle
- [x] Alpha-blended UI geometry
- [x] Glyph atlas and text rendering prototype
- [ ] Theme loading from `assets/themes`

## Editor shell

- [x] UTF-8 document model with cursor and keyboard editing
- [x] Shift-keyboard and mouse-drag text selection
- [x] System clipboard copy/paste and duplicate-line shortcut
- [x] Undo/redo
- [x] Editor layout, line-number gutter, animated caret and status line
- [x] File open/save through the project workspace
- [x] C++ syntax highlighting tokenizer
- [x] Compiler-backed C/C++ diagnostics with anchored, dismissible error card
- [x] File-backed settings project with live JSON validation
- [x] Mouse caret placement, viewport scrolling and cursor affordances
- [x] Brand palette, light/dark marks and GPU-drawn start surface
- [x] Smooth, low-overshoot spring animation for carets, highlights and Path Lens
- [x] Inline compile functions with platform filters, variables and clickable run targets
- [x] Attached PTY terminal with streamed output, input, focus opacity and close control
- [x] Movable compile terminal and diagnostic cards
- [x] Project-wide function palette with cached `.lucid/cache/functions.json` index
- [x] First-run shortcut guidance on the start and empty-project surfaces
- [x] macOS `.app` bundle, layered Liquid Glass icon source, ICNS asset catalog and platform menu bar
- [x] Windows executable icon, version resource and Windows 11 DWM caption colors
- [x] Linux desktop entry and installed application icon

## Path-native workspace

- [x] Project prompt that opens or creates a project root
- [x] File prompt with fuzzy path suggestions
- [x] Automatic folder creation from path segments
- [x] Folder creation with a trailing slash or `Ctrl+Enter`
- [x] Folder suggestions and keyboard navigation through directories
- [x] Save before switching files/projects and on quit
- [x] In-session recent projects and project directory browsing
- [x] Persisted recent projects and session restore (clean exits; crash recovery is separate)
- [ ] Large-project index updates without a full refresh

## Daily workflows

- [x] Multiple documents through a temporary switcher without a permanent tab bar
- [ ] Split editors
- [ ] Command palette and configurable key bindings
- [x] Persisted settings and UI zoom
- [x] JSON syntax highlighting and diagnostics
- [ ] Symbol index
- [ ] macOS app bundle and notarization

## Later

- [ ] Incremental parsing
- [x] C/C++ language server client (clangd; more languages and capabilities remain)
- [ ] Multi-cursor editing
- [ ] Plugin surface
- [x] Windows and Linux packaging

## VS Code replacement work

Lucid does not yet have feature or extension compatibility with VS Code. The following milestones
make the missing work explicit while retaining the path-first interface and optional local AI.

1. **Everyday editing:** reliable open/save and external-change handling; a transient open-file switcher and splits; file and
   project search with replace; multi-cursor editing; indentation, formatting, encodings and line
   endings; persistent workspaces and keyboard shortcuts. In-file and project-wide literal search and reviewed replacement are available now.
2. **Development tools:** extend the existing comment-driven compile windows for builds and test
   results; provide Git status, diffs, staging, commits and merge conflict resolution. Do not add a
   general-purpose terminal.
3. **Language support:** extend C/C++ clangd integration for additional languages, rename support
   and discoverable language setup.
4. **Debugging:** debug-adapter integration, launch configurations, breakpoints, variables,
   call stacks and stepping.
5. **Extensibility and accessibility:** a documented module API, themes, configurable
   keybindings, accessibility and assistive-technology support. VS Code extension compatibility
   requires separate investigation and should not be assumed.
6. **Reliability:** large projects, responsive indexing, cross-platform packaging and automated
   interaction tests for macOS, Linux and Windows.
7. **Multiplayer:** shared projects reached through private-network links, with both a participant-hosted
   session and a self-hosted server. Authentication, access controls, conflict resolution and recovery
   remain unfinished; collaboration is not exposed in the editor.
