<div align="center">

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="assets/branding/lucid-dark.svg">
  <source media="(prefers-color-scheme: light)" srcset="assets/branding/lucid-light.svg">
  <img src="assets/branding/lucid-dark.svg" width="88" alt="Lucid logo">
</picture>

# Lucid (Beta)

**A minimal, GPU-accelerated code editor built on Vulkan.**

Keyboard-first. Path-native. Drawn entirely on the GPU.

[![CI](https://github.com/initializeddd/Lucid/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/initializeddd/Lucid/actions/workflows/ci.yml)
[![Status](https://img.shields.io/badge/status-beta-787EFF)](#downloads)
[![License](https://img.shields.io/github/license/initializeddd/Lucid?color=787EFF)](LICENSE)
[![Release](https://img.shields.io/github/v/release/initializeddd/Lucid?color=787EFF&include_prereleases)](https://github.com/initializeddd/Lucid/releases)
[![Platforms](https://img.shields.io/badge/platforms-macOS%20%7C%20Linux%20%7C%20Windows-787EFF)](#platform-support)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-787EFF)](https://en.cppreference.com/w/cpp/20)

[Getting started](docs/getting-started.md) · [Architecture](docs/architecture.md) · [Building](docs/building.md) · [Roadmap](docs/roadmap.md) · [Contributing](CONTRIBUTING.md)

</div>

---

Lucid is a keyboard-first editor built around paths instead of a file tree. Projects, files and
folders are opened or created by typing where they live, so the workspace stays on the canvas and
out of your way. New projects default to `~/Documents/Lucid`, keeping them separate from other apps.

The window, text, chrome and overlays are all rendered by a small Vulkan renderer. On macOS it runs
on MoltenVK; on Windows and Linux it uses the native Vulkan loader.

> **Status:** public beta (`0.1.0-beta.1`). The core editing loop is usable and the test suite runs
> on all three platforms, but APIs and file formats may still change. The beta channel is shown in
> the window title, the status bar and the About panel.

## Highlights

- **Path-native workspace** — no file tree. Open, create and navigate projects by typing a path;
  folders are inferred and created automatically.
- **GPU-rendered surface** — Vulkan swapchain, shader-compiled UI, glyph atlas, and a separable
  Gaussian blur behind the Path Lens.
- **Keyboard-first** — a single command surface, on-screen keycaps, and native menu equivalents.
- **Fast, quiet UI** — low-overshoot spring animations, a restrained brand palette, and an aurora
  that fades behind text so the page stays readable.
- **Real editing** — UTF-8 documents, selection, undo/redo, clipboard, auto-save, syntax
  highlighting and live C/C++ and JSON diagnostics.
- **Inline compile functions** — declare build commands in a C/C++ comment block, run them with
  `⌘R`, and watch a movable PTY terminal stream output beside the definition.
- **Project-wide function search** — `⌘⇧R` lists compile functions from every text file, backed by
  a cache at `.lucid/cache/functions.json`.
- **File-backed settings** — a modal panel, three theme presets, a custom accent color and live
  JSON validation.

## Platform support

| | macOS | Linux | Windows |
| --- | :---: | :---: | :---: |
| Build | ✅ | ✅ | ✅ |
| Vulkan backend | MoltenVK | Vulkan loader | Vulkan loader |
| Native bundle / installer files | `.app` + ICNS | `.desktop` + PNG | `.exe` + ICO |
| Path Lens, editing, diagnostics | ✅ | ✅ | ✅ |
| Compile-function PTY terminal | ✅ | ✅ | Planned |
| Native destructive-action dialogs | ✅ | Planned | ✅ |
| Theme follows system caption bar | ✅ | — | ✅ (Windows 11) |

## Downloads

Prebuilt beta bundles are attached to the [latest release](https://github.com/initializeddd/Lucid/releases/tag/v0.1.0-beta.1):

| Platform | Artifact | Notes |
| --- | --- | --- |
| macOS | `Lucid-v0.1.0-beta.1-macOS.zip` | Ad-hoc signed; unzip and open `Lucid.app` |
| Linux | `Lucid-v0.1.0-beta.1-Linux.tar.gz` | Extract and run `lucid`; needs a Vulkan driver |
| Windows | `Lucid-v0.1.0-beta.1-Windows.zip` | Extract and run `Lucid.exe`; needs the Vulkan runtime |

Beta builds are not notarized or code-signed for distribution. macOS Gatekeeper and Windows
SmartScreen may warn on first launch; build from source below if you prefer.

## Requirements

| Requirement | Minimum | Notes |
| --- | --- | --- |
| CMake | 3.24 | Presets require CMake 3.24+ |
| C++ compiler | C++20 | Apple Clang, GCC, Clang or MSVC |
| GLFW | 3.3 | Fetched automatically when missing |
| Vulkan | 1.2 | Headers, loader and a working ICD |
| `glslangValidator` | — | Ships with the Vulkan SDK |

## Build from source

Clone the repository, then follow the section for your platform. All commands are run from the
repository root.

```sh
git clone https://github.com/initializeddd/Lucid.git
cd Lucid
```

### macOS

Install the toolchain and libraries with [Homebrew](https://brew.sh):

```sh
xcode-select --install
brew install cmake ninja glfw glslang molten-vk vulkan-loader vulkan-headers
```

Configure, build and launch the app bundle:

```sh
cmake --preset debug
cmake --build --preset debug
open build/debug/bin/Lucid.app
```

For an optimized build, swap `debug` for `release`:

```sh
cmake --preset release
cmake --build --preset release
open build/release/bin/Lucid.app
```

Install it into `/Applications` (the install step re-signs the bundle after copying):

```sh
cmake --install build/release --prefix /Applications
```

A LunarG Vulkan SDK also works and is discovered automatically under `~/VulkanSDK/*` when
`VULKAN_SDK` is not set. If the loader cannot find MoltenVK at runtime:

```sh
export VK_ICD_FILENAMES=/opt/homebrew/share/vulkan/icd.d/MoltenVK_icd.json
```

### Linux

Install a compiler, CMake, Ninja, GLFW, the Vulkan loader and headers, and `glslang`:

```sh
# Debian / Ubuntu
sudo apt install build-essential cmake ninja-build libglfw3-dev libvulkan-dev \
    vulkan-tools glslang-tools libx11-dev libxrandr-dev libxinerama-dev \
    libxcursor-dev libxi-dev libwayland-dev libxkbcommon-dev
```

```sh
# Fedora
sudo dnf install gcc-c++ cmake ninja-build glfw-devel vulkan-loader-devel \
    vulkan-tools glslang libXrandr-devel libXinerama-devel libXcursor-devel \
    libXi-devel wayland-devel libxkbcommon-devel
```

```sh
# Arch
sudo pacman -S --needed base-devel cmake ninja glfw vulkan-headers \
    vulkan-icd-loader vulkan-tools glslang libxrandr libxinerama libxcursor \
    libxi wayland libxkbcommon
```

Configure, build, test and run:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/bin/lucid
```

Install the executable, desktop entry and icon into your local prefix:

```sh
cmake --install build --prefix ~/.local
```

### Windows

Install the following, then open **x64 Native Tools Command Prompt for VS 2022**:

- **Visual Studio 2022** with the *Desktop development with C++* workload
- **CMake 3.24+** (bundled with Visual Studio, or `winget install Kitware.CMake`)
- **Vulkan SDK 1.2+** from LunarG, or `winget install KhronosGroup.VulkanSDK`

The SDK provides the loader, headers and `glslangValidator`. GLFW is fetched automatically when a
system installation is not found.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\bin\Lucid.exe
```

The executable embeds `packaging/windows/lucid.ico`. On Windows 11, Lucid applies a dark caption,
text and border color through DWM while keeping the standard window controls.

> Compile functions and their PTY terminals currently run on macOS and Linux only. On Windows the
> platform layer reports that the terminal is unavailable.

### Build options

| Option | Default | Description |
| --- | --- | --- |
| `LUCID_ENABLE_VALIDATION` | `ON` | Enable `VK_LAYER_KHRONOS_validation` in Debug builds |
| `LUCID_WARNINGS_AS_ERRORS` | `OFF` | Promote compiler warnings to errors |
| `LUCID_BUILD_TESTS` | `ON` | Build the test suite |

Pass them with `-D<OPTION>=<VALUE>` at configure time, for example:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLUCID_WARNINGS_AS_ERRORS=ON
```

See [Building](docs/building.md) for signing, packaging, icon regeneration and validation notes.

## Settings

`⌘,` opens the settings panel. **Edit settings...** opens the `Settings` project; the file lives at:

| Platform | Path |
| --- | --- |
| macOS | `~/Library/Application Support/Lucid/Settings/settings.json` |
| Windows | `%APPDATA%\Lucid\Settings\settings.json` |
| Linux | `$XDG_CONFIG_HOME/Lucid/Settings/settings.json` or `~/.config/Lucid/Settings/settings.json` |

```json
{
    "animationSpeed": 1.0,
    "showSyntaxErrors": true,
    "showLineNumbers": true,
    "highlightCurrentLine": true,
    "cursorBlink": true,
    "cursorStyle": "line",
    "backgroundMotion": true,
    "zoomFactor": 1.0,
    "theme": "lucid-dark",
    "primary": "#787eff"
}
```

`animationSpeed` accepts `0.5`, `1.0` or `2.0`. `cursorStyle` accepts `line`, `block`,
`underscore` or `kitty` (`bar` and `underline` remain accepted as aliases). `backgroundMotion`
toggles the soft animated aurora behind the editor and empty surfaces; with a file open it fades
darker and more diffuse so text stays readable. `zoomFactor` accepts `0.5` through `3.0` and scales
the whole interface, including text, layout and mouse hit testing. `theme` accepts `lucid-dark`,
`graphite` or `lucid-light`. `primary` accepts any `#RRGGBB` color and overrides the theme accent and
selection tint; omit it to use the theme default. The settings panel includes primary color swatches
for quick changes.

Settings apply live while the file is edited; `⌘S` persists them. Invalid settings leave the last
valid configuration active and show a red error in the top-right corner; click it to reopen
`settings.json`.

## Keyboard shortcuts

On macOS use `⌘`; on other platforms use `Ctrl`.

| Action | Shortcut |
| --- | --- |
| Open or create a project | `⌘P` / `⌘O` |
| Open or create a file by path | `⌘K` / `⌘N` |
| Complete the selected suggestion | `Tab` |
| Create the typed folder and keep the prompt open | `⌘↵` |
| Dismiss the Path Lens | `Escape` |
| Save | `⌘S` |
| Close file (saves first) | `⌘W` |
| Delete line | `⌘⌫` / `⌘⇧K` |
| Insert line below / above | `⌘↵` / `⌘⇧↵` |
| Toggle line comment | `⌘/` |
| Indent / outdent selection | `Tab` / `Shift+Tab` |
| Move current line | `Option+Up` / `Option+Down` |
| Undo / redo | `⌘Z` / `⌘⇧Z` / `⌘Y` |
| Run compile function under caret | `⌘R` |
| Search project compile functions | `⌘⇧R` |
| Open settings | `⌘,` |
| Select all / copy / cut / paste | `⌘A` / `⌘C` / `⌘X` / `⌘V` |
| Duplicate line | `⌘D` |
| Delete file / project | `⌘⇧⌫` / `⌘⇧⌥⌫` |

Arrows, `Home` and `End` move the caret; hold `Shift` to extend the selection. Click a line to place
the caret, drag to select, and `Shift`-click to extend. Scroll to move the viewport. Click anywhere
outside the Path Lens to dismiss it.

Suggestions ending in `/` are folders: press `Enter` or `Tab` to step into one. A trailing slash also
creates a folder, so `src/components/` followed by `Enter` makes the directory. Creating
`src/components/Button.cpp` creates both folders automatically.

## Compile functions

C and C++ comment blocks can define compile functions. Each function starts with `@name`, ends with
`@end`, and contains shell commands prefixed with `$`:

```cpp
/*
@build:macOS
$cmake -S %project% -B %project%/build
$cmake --build %project%/build
@end
*/
```

Use zero or more platform suffixes: `:macOS`, `:windows` or `:linux`. Unqualified functions run on
every platform. Variables are expanded before execution:

| Variable | Value |
| --- | --- |
| `%project%` | project root |
| `%projectName%` | project folder name |
| `%file%` | absolute path of the open file |
| `%fileDir%` | directory containing the open file |
| `%fileName%` | open file name |
| `%relative%` | file path relative to the project root |
| `%line%` | compile function definition line |
| `%platform%` | `macOS`, `windows` or `linux` |

Run a function with `⌘R` anywhere inside its block, click its highlighted `@name`, or use
**Run > Run Compile Function** on macOS. The file is saved first. A terminal panel opens beside the
function line, streams output and accepts keyboard input while the process is running. It is 50%
opaque until clicked, then becomes fully opaque and receives focus. Its red traffic light closes the
panel and terminates the process. Drag the terminal title bar to move the panel; drag a diagnostic
card by its body.

Press `⌘⇧R` (`Ctrl+Shift+R` elsewhere) or choose **Run > Search Project Functions…** to list
functions from every text file in the project. Type part of a name, then press `Enter` to open the
defining file and run the matching function. Parsed functions are cached in
`.lucid/cache/functions.json`; unchanged files are reused on later searches, and the cache is
invalidated when a file is saved or deleted.

## Project layout

```text
Lucid/
├── assets/          Runtime assets: themes, fonts, icons and shaders
├── cmake/           Reusable CMake modules and options
├── docs/            Architecture, build notes and roadmap
├── packaging/       macOS bundle, Windows resources and Linux desktop files
├── scripts/         Icon and packaging helpers
├── src/app/         Application lifecycle
├── src/core/        Fundamental types, version, assertions
├── src/editor/      Document, buffer, cursor, selection, syntax
├── src/events/      Event queue and dispatch
├── src/fs/          Filesystem abstraction
├── src/input/       Input mapping and key codes
├── src/logging/     Small thread-safe logger
├── src/platform/    Window, native menus, PTY processes and platform integration
├── src/renderer/    Renderer facade and Vulkan backend
├── src/text/        Font, shaping, layout and glyph atlas
├── src/ui/          Widgets, panels, layout and theming
├── src/workspace/   Projects, sessions and workspace state
├── tests/           Unit and smoke tests
├── third_party/     Vendored dependencies (`stb_truetype`)
└── tools/           Development tooling
```

See [Architecture](docs/architecture.md) for the design of each layer and
[Getting started](docs/getting-started.md) for a first-project walkthrough.

## Development

Format the C++ sources with the committed [`.clang-format`](.clang-format):

```sh
clang-format -i $(git ls-files '*.cpp' '*.hpp' '*.mm')
```

Run the full test suite:

```sh
ctest --preset debug          # macOS / Ninja presets
ctest --test-dir build -C Release --output-on-failure   # multi-config generators
```

Debug builds enable `VK_LAYER_KHRONOS_validation` automatically when the layer is available; the
renderer logs a warning and continues if it is missing.

## Contributing

Bug reports, feature ideas and pull requests are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md)
before opening a PR, and use the issue templates when reporting a problem. Please report security
issues privately as described in [SECURITY.md](SECURITY.md).

## License

Lucid is released under the [MIT License](LICENSE).
