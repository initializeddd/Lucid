# Architecture

Lucid is organized as a small application built on top of a standalone core library. The goal is a
code editor that stays fast, predictable and easy to reason about as features accumulate.

## Principles

- **Minimal by default.** No immediate-mode toolkit, no hidden global state, no framework gravity.
- **The GPU draws everything.** The window surface is a Vulkan swapchain from the first commit.
- **Explicit ownership.** RAII wrappers own Vulkan objects. The renderer facade hides Vulkan from
  the application layer.
- **Data-oriented editor core.** Documents, buffers and selections are plain data with focused
  operations; rendering reads them, never owns them.
- **Path-native workspace.** There is no file tree. A project is a root plus flat file and
  directory indexes, and the Path Lens opens, creates or navigates by path. Folders remain visible
  as path segments and trailing-slash suggestions rather than separate panels.
- **Platform seams are narrow.** GLFW and native services live under `src/platform`.

## Layers

| Layer | Responsibility |
| --- | --- |
| `app` | Application lifecycle and the main loop |
| `core` | Version, fundamental types, assertions |
| `platform` | Window creation, native menus, title bars, PTY processes and future filesystem/input |
| `renderer` | Renderer facade plus the Vulkan backend |
| `editor` | Text documents, cursors, selections, syntax and diagnostics |
| `ui` | Layout, settings, panels, overlays and theme application |
| `text` | Font discovery, shaping, text layout and glyph atlases |
| `workspace` | Projects, directory/file indexes, sessions and persisted state |
| `events` | Event queue and dispatch between subsystems |
| `input` | Key bindings and input state |
| `fs` | Cross-platform filesystem helpers |
| `logging` | Minimal thread-safe logger |

## Startup sequence

```text
main
 └─ Application
     ├─ Window            GLFW, no client API, Vulkan support check
     └─ Renderer
         ├─ Instance      instance + optional validation/debug messenger
         ├─ Surface       GLFW window surface
         ├─ Device        physical device selection + queues
         ├─ Swapchain     images, views, render pass, framebuffers
         ├─ CommandPool   primary command buffers
         ├─ FrameSync     semaphores + in-flight fences
         ├─ FontAtlas     CPU-baked R8 glyph atlas from a system font
         ├─ SceneTarget   offscreen color target per frame in flight
         ├─ UiPass        rounded-rect and text pipelines, UI vertex buffers
         └─ BlurPass      separable gaussian passes over the scene target
```

Each frame waits on the current in-flight fence and acquires a swapchain image. The base UI renders
to an offscreen scene target. While the Path Lens is visible, `BlurPass` runs a horizontal and then a
vertical separable Gaussian pass over a second offscreen target and composites a darker, blurred
canvas to the swapchain; a final overlay pass draws the command bar sharply on top. Shader-read
barriers separate the scene, blur and composite passes. Framebuffer size changes recreate the
swapchain and all UI targets. The renderer also compares the window's content scale every frame;
moving to a display with a different backing scale rebuilds the font atlas and UI resources even
when the framebuffer extent happens to stay the same.

`Renderer::setUiZoom` scales `contentScale` by the settings `zoomFactor`, rebuilds the font atlas at
the new physical size and recreates UI resources after waiting for the device to idle. Application
mouse coordinates and logical window dimensions are divided by the same factor, so layout constants
remain shared between hit testing and rendering. After each frame the application refreshes
`logicalCharacterAdvance()` so caret placement follows the current font atlas.

## Platform integration

GLFW remains the window and input layer, while `src/platform/app/PlatformApp.*` adds only the native
chrome that each desktop expects.

- **macOS:** `PlatformApp.mm` receives GLFW's `NSWindow`, enables
  `NSWindowStyleMaskFullSizeContentView`, makes the title bar transparent and hides the native title
  while keeping the traffic-light controls. `UiPass` draws the centered `Lucid (Beta) - filename*` title,
  and the real window title is kept in sync for Mission Control. The default application menu is
  replaced with **Lucid**, **File**, **Edit**, **Run**, **Window** and **Services** menus. Menu
  items route through `platform::MenuActions` back into `Application`, so menu commands and keyboard
  shortcuts share the same code paths. Destructive actions use `NSAlert` for confirmation.
- **Windows:** `PlatformApp.cpp` gets the native `HWND`, enables immersive dark mode and sets
  `DWMWA_CAPTION_COLOR`, `DWMWA_TEXT_COLOR` and `DWMWA_BORDER_COLOR`. DWM still draws the standard
  caption buttons.
- **Linux:** the platform hooks are no-ops; the desktop entry and installed icon live under
  `packaging/linux`. Destructive actions are refused because no native confirmation dialog is
  available yet.

The macOS executable is a CMake `MACOSX_BUNDLE`. `packaging/macos/Info.plist` supplies the bundle
metadata, `Lucid.icns` is the bitmap fallback, and `Assets.car` carries the icon compiled from the
layered `Lucid.icon` source. The build ad-hoc signs the finished bundle after assets and icons are
copied. Windows embeds `packaging/windows/lucid.ico` plus version metadata through
`packaging/windows/lucid.rc`.

## Path Lens

`Application` owns the project, the open document and the command-line state. It builds a
`ui::FrameState` every frame; `UiPass` only reads that snapshot. The snapshot borrows views into
application-owned strings and document lines, so no editor state leaks into the renderer.

- `⌘P` opens the project prompt. Relative project paths resolve inside `~/Documents/Lucid`,
  while `~/...` and absolute paths stay explicit. The resolved path is passed to
  `workspace::Project::openOrCreate`. An empty query lists recent projects and directories inside
  the Lucid projects folder; a partial query browses the matching directory.
- `⌘K` opens the file prompt. Files come from the project's fuzzy file index and folders from the
  directory index. A non-existent typed path becomes a `+` create-file suggestion and a
  `+ path/` create-folder suggestion.
- `Enter` opens an existing path or creates a missing one. Every parent directory is created as a
  side effect. A trailing slash, or `⌘↵`, creates a directory and keeps the prompt open.
- Selecting a folder suggestion and pressing `Enter` or `Tab` steps into it, which makes deep paths
  navigable without a tree.
- The command surface is permanently borderless. Query text floats over a blurred, darkened canvas
  with no bar or outline; suggestions and status text anchor beneath it while the selection glides
  between rows. While the command surface is hidden, the base UI draws directly to the swapchain;
  when it is visible, the scene renders to an offscreen target, a fullscreen gaussian pass blurs and
  composites it, and the command overlay is drawn sharply on top.
- The caret, highlight, command bar, suggestion selection and row reveal use critically damped or
  lightly underdamped springs rather than linear interpolation. The command bar drives scale and
  offset from clamped progress instead of overshooting spring values, so motion stays smooth and
  settles without bounce.
- Before switching documents or projects, the current document is saved. The same save runs when
  the window closes.
- The project index skips VCS, dependency, build and cache directories, including `.lucid`. Paths
  are validated to stay inside the project root.
- `workspace::loadFunctionIndex` parses compile functions from every text file and caches them in
  `.lucid/cache/functions.json`. Cache entries store file size and modification time; unchanged
  files are reused, and saves or deletes invalidate the in-memory index.

## Selection and clipboard

`Document` owns an anchor plus an active cursor. Navigation methods take an `extendSelection`
flag, so `Shift` movement and `⌘⇧←/→` grow the range without a separate selection
object. Editing replaces or removes the active range, and `selectionUtf8()` provides clipboard-ready
text. `Application` maps `⌘C/X/V/A/D/Z` to copy, cut, system paste, select-all, duplicate-line and
undo/redo; clipboard text goes through GLFW so it interoperates with other applications.

`UiPass` renders selection ranges from `FrameState` before drawing line text. Mouse press starts a
selection anchor and mouse drag extends it; `Shift`-click extends from the existing anchor.

## Start surface

When no project is open, `UiPass` draws the start surface from `ui::welcomeLayout`: the mark
geometry from `assets/branding/lucid-dark.svg`, a project action, and a compact shortcut grid. At
650 logical pixels and wider, the mark sits left of a hairline divider with a short accent cap; the
action and a three-column grid sit to its right. Below that width the composition stacks centered
with a two-column grid. The mark is 140 logical pixels in the wide layout and 160 when stacked.
The action is a low-contrast command card with the shortcut keycaps inset into it. An open project
with no document shows the same grid around a `No file open` label instead, with the project name as
a muted eyebrow and a short accent rule above the title. Shortcut hints are drawn
as keycaps only; keyboard shortcuts are never rendered as text.
`assets/shaders/welcome.frag` draws a full-screen, slowly drifting aurora behind the editor and
both empty surfaces using the primary colour and white at low opacity. Three soft ribbons and a
gentle edge fade keep text and buttons readable, while one draw call replaces the previous per-dot
geometry. With a document open, `FrameState::backgroundDim` eases the shader toward lower opacity,
a darker tint and wider ribbons, and eases back when the document closes. The `backgroundMotion`
setting removes the pass entirely, and the settings panel exposes the same toggle as **Motion**.
`Application` uses the same layout for hit testing, so clicking the action opens the project prompt.
On macOS the command and shift keycaps use the real `⌘` and `⇧` glyphs; the font atlas packs both
symbols alongside its ASCII range. The surface remains visible under the blurred Path Lens overlay.

## Diagnostics and settings

Settings are a normal project named `Settings`. On macOS the root is
`~/Library/Application Support/Lucid/Settings`; `⌘,` opens the modal settings panel.
`ui::settingsPanelLayout` owns its geometry and `ui::settingsControlAt` maps clicks to controls. The
panel changes theme, animation speed, UI zoom and editor toggles immediately and persists them to
`settings.json`. **Edit settings...** opens the file for advanced changes, including a custom primary color.

`app::parseSettings` validates the flat schema (`animationSpeed`, `showSyntaxErrors`,
`showLineNumbers`, `highlightCurrentLine`, `cursorBlink`, `cursorStyle`, `backgroundMotion`,
`zoomFactor`, `theme`, `primary`).
Themes are `lucid-dark`, `graphite` and `lucid-light`. `primary` is an optional `#RRGGBB` override;
the settings panel offers eight swatches and selecting a theme clears the override. A custom
primary also derives the selection tint by blending it over the theme background. `cursorStyle` is
`line`, `block`, `underscore` or `kitty`; `bar` and `underline` remain aliases. The kitty style is a
normal line caret with two small ears peeking from its right edge. `Application` applies
valid values live by document revision and keeps the last valid configuration when parsing fails.
Config errors appear as red text in the top-right settings position; clicking the text opens the
file. Animation speed scales the Path Lens spring, suggestion reveal, caret blink and editor
highlight interpolation.

C and C++ files are diagnosed by the system `clang`/`clang++`. `Application` debounces compiler runs,
captures the document revision and path, and runs the compiler asynchronously on a temporary copy.
`editor::parseCompilerDiagnostics` reads `-fdiagnostics-print-source-range-info` output for error
line, column, token length and optional `[-W...]` code. JSON files use the built-in
`editor::diagnoseJson` parser diagnostics instead. Retrieved diagnostics are cached by revision,
ignored keys are skipped per file, and the first visible diagnostic is exposed to `UiPass`.

`UiPass` tints the diagnostic row, colors its line number and underlines the offending token. The
error card anchors beneath the token when it fits, otherwise above it, and falls back to the
top-right when the line is off-screen. A connector line joins the card to the token. The card label
identifies JSON, C or C++ and includes the compiler code when present. Its `x` button ignores that
diagnostic for the session.

## Compile functions and terminals

C and C++ blocks containing `@name`, `$` commands and `@end` are parsed by
`editor::parseCompileScript`. The parser tracks block comments, platform suffixes and command line
numbers. `%project%`, `%file%`, `%fileDir%`, `%fileName%`, `%relative%`, `%line%` and `%platform%`
are expanded immediately before execution.

`Application` caches the parsed script by document revision. Clicking a highlighted directive or
pressing `⌘R` saves the file and starts `platform::PtyProcess`, which runs `/bin/sh` through
`forkpty` in the project root. Commands run sequentially; a unique marker is appended to each shell
command so output can be attributed before the next command is sent. PTY reads and command
sequencing are nonblocking. ANSI escape sequences, carriage returns and control bytes are sanitized
for display, and terminal input is forwarded to the PTY. Closing the panel sends hangup/kill to the
child process.

`UiPass` renders the compile comment with muted gray text and yellow directives, commands and
variables. A small yellow rail marks each runnable function line. The terminal is anchored beside
the function's visible row, eases into place, and renders recent output plus an input prompt. Its
opacity is 50% until clicked and 100% while focused. The terminal uses a window-style title bar with
traffic lights, a running/finished status pill and a soft drop shadow; the red light closes it. The
terminal title bar and diagnostic cards are
draggable; offsets are clamped to the viewport and reset when a new function runs, the terminal
closes, diagnostics reset or the project changes.

`⌘⇧R` opens the project function prompt. `workspace::searchFunctions` ranks exact name
matches first, then prefixes, substrings, file matches and subsequences. Choosing a result opens
the defining file, moves the caret to the function and runs it through the same PTY path as
`⌘R`. The function index is built lazily and cached under `.lucid/cache/functions.json`.

## Mouse input

GLFW mouse events are translated into `input::MouseButtonEvent`, `MouseMoveEvent` and
`ScrollEvent`. Application coordinates are divided by `zoomFactor`, then hit-tested against UI
layout constants; `UiPass` multiplies the same constants by the zoomed `contentScale` for rendering.
Clicks place the caret by advancing through the visible line with
`Renderer::logicalCharacterAdvance()`, dragging extends a text selection, the wheel scrolls the
editor, and hover changes both cursor shape and selected suggestions.

## Syntax highlighting

`editor::highlightCppLine` turns one UTF-32 line into `TokenSpan` ranges for keywords, types,
strings, numbers, comments, preprocessor directives, functions and punctuation. A small `CppState`
carries block-comment state across lines and recognizes compile blocks, highlighting directives,
commands, variables and function names separately. `editor::highlightJsonLine` provides the JSON token set,
including distinct object keys. `Application` chooses by file extension, caches the tokenized
document and rebuilds it only when `Document::revision()` changes. `UiPass` colors each visible line
from the cached spans; the renderer still knows nothing about the document itself.

## Packaging

`scripts/generate-icons.sh` is the single source for committed icon artifacts. On macOS it asks
`actool` to compile `packaging/macos/Lucid.icon` into both `Lucid.icns` and `Assets.car`; this is the
same layered source that Icon Composer uses for Liquid Glass appearances. The icon background is
`#1C1C1C`, matching the application canvas. The script rasterizes the Windows SVG into a seven-size
ICO and produces the Linux PNG.

The CMake target installs the bundle on macOS and the executable on Windows/Linux. Linux installs
also include the `.desktop` entry and a 512 px icon.

## Color management

The theme stores sRGB colors. If the swapchain uses an sRGB format, the clear color and UI colors are
converted to linear before submission so the final pixels match the design tokens in
`assets/themes/lucid-dark.json`. `ui::theme` keeps a runtime palette; presets and custom overrides are
applied through `applyPalette`, and `Renderer::setClearColor` keeps the canvas in sync with the
selected theme.

## Near-term seams

- `renderer/rhi` will hold the backend-agnostic command and resource interfaces.
- `ui` will consume draw lists produced from editor and chrome state.
- `text` owns the current glyph atlas; shaping and shaped-run caching arrive with the editor core.
- `workspace` provides the project context to the application without the renderer knowing about
  the filesystem.
- Horizontal scrolling and incremental parsing are the next editor-core seams.

## File layout

The current tree intentionally leaves room for these subsystems. Empty module directories are
placeholders; each gains code as its milestone arrives.
