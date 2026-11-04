# Getting started

Lucid is a project-based editor. Open a project folder first, then open files inside it.

## 1. Install and launch

Download [Lucid v0.2.1-beta.2](https://github.com/initializeddd/Lucid/releases/tag/v0.2.1-beta.2)
for your platform, or browse all [releases](https://github.com/initializeddd/Lucid/releases). The
beta release page includes `SHA256SUMS.txt` for checking the downloaded files.

To verify a download, calculate its SHA-256 digest and compare it with the line for that filename
in `SHA256SUMS.txt`. On macOS use `shasum -a 256 <filename>`; on Linux use
`sha256sum <filename>`; on Windows PowerShell use `Get-FileHash -Algorithm SHA256 <filename>`.

| Platform | Get started |
| --- | --- |
| macOS | Open the DMG or extract the ZIP, then launch `Lucid.app`. The app is ad-hoc signed and is not notarized. |
| Windows | Run the MSI or extract the ZIP and launch `Lucid.exe`. The MSI requires administrator access. |
| Linux | Extract the Linux archive and run `./lucid`. A working Vulkan driver is required. |
| FreeBSD or OpenBSD | Extract the matching archive and run `./lucid`; install GLFW, Vulkan and system cryptography libraries first. |

Beta builds are not signed with distribution certificates. macOS Gatekeeper and Windows
SmartScreen may display a warning. BSD graphical desktop testing is still limited.

### Build from source

For the full platform-specific instructions, see [Building](building.md). To build on macOS,
clone the repository and run these commands from its root:

```sh
brew install cmake ninja glfw glslang molten-vk vulkan-loader vulkan-headers
cmake --preset release
cmake --build --preset release
open build/release/bin/Lucid.app
```

To keep the app in Applications:

```sh
cmake --install build/release --prefix /Applications
```

`cmake --install` re-signs the bundle after copying it, because changing the bundle invalidates the
ad-hoc signature. You can also copy it with `ditto`; sign it again afterwards if you modify the
bundle. The deployment target defaults to macOS 14 unless overridden; locally built bundles may require installed Homebrew libraries.

Windows builds produce `build\bin\Lucid.exe`. Linux builds install a launcher with
`cmake --install build --prefix ~/.local`.

## 2. Open a project

Press `Ctrl+O` (Command+O on macOS). Type the folder that contains the project, not
an individual source file.

```text
~/code/example
```

Absolute paths and paths under the Lucid projects folder work too. The project folder becomes the
working directory for compile functions, so `%project%` always means that folder.

Press `Enter` to open it. If the folder does not exist, Lucid creates it.

## 3. Open a file

Press `Ctrl+K` (or `Ctrl+N`). Type a relative path:

```text
src/main.cpp
```

`Enter` opens an existing file or creates a missing one. Folders ending in `/` can be opened from
the suggestion list.

## 4. Change settings

Press `Ctrl+,` (Command+, on macOS). Lucid opens the settings menu with theme, primary color, text size, cursor, icon, editor, animation and AI options. **Open settings file** opens `settings.json` for advanced changes.

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

| Key | Values | Effect |
| --- | --- | --- |
| `animationSpeed` | `0.5`, `1.0`, `2.0` | Path Lens and caret animation speed |
| `showSyntaxErrors` | `true`, `false` | Show live JSON and compiler diagnostics |
| `showLineNumbers` | `true`, `false` | Show the editor gutter |
| `highlightCurrentLine` | `true`, `false` | Tint the caret row |
| `cursorBlink` | `true`, `false` | Blink the editor caret |
| `cursorStyle` | `line`, `block`, `underscore`, `kitty` | Draw the caret as a line, block, underscore or a line with ears |
| `backgroundMotion` | `true`, `false` | Show the selected animated 3D scene; it softens while a file is open |
| `zoomFactor` | `0.5` to `3.0` | Scale text, layout and hit testing |
| `theme` | `lucid-dark`, `graphite`, `lucid-light` | Apply a built-in palette |
| `primary` | `#RRGGBB` | Override the theme accent and selection tint; omit to use the theme default |

Changes apply as you edit. Press `Ctrl+S` to persist them. Invalid JSON leaves the previous settings
active and shows a red error; click the error to return to the settings file.

Settings live at:

| Platform | Path |
| --- | --- |
| macOS | `~/Library/Application Support/Lucid/Settings/settings.json` |
| Windows | `%APPDATA%\Lucid\Settings\settings.json` |
| Linux | `$XDG_CONFIG_HOME/Lucid/Settings/settings.json` or `~/.config/Lucid/Settings/settings.json` |

## 5. Run custom commands from a project

Lucid reads compile functions from C and C++ block comments. A function starts with `@name`, has one
or more `$` commands, and ends with `@end`:

```cpp
/*
@build
$cmake -S %project% -B %project%/build
$cmake --build %project%/build
@end
*/
```

Put the block near the top of any open `.c`, `.cpp`, `.h` or `.hpp` file. For a cloned open-source
project, a local `lucid.cpp` in the project root is a convenient scratch file. It is a valid C++
file containing only comments, and you can exclude it from version control if you do not want to
commit it.

Commands run from the project root. This means an existing project can be built without changing
its `Makefile`, `CMakeLists.txt` or scripts:

```cpp
/*
@configure
$cmake -S %project% -B %project%/build -DCMAKE_BUILD_TYPE=Debug
@end

@build:macOS
$cmake --build %project%/build -j
@end

@test
$ctest --test-dir %project%/build --output-on-failure
@end
*/
```

Use a platform suffix when a command only applies to one system:

```cpp
/*
@build:macOS
$cmake --build %project%/build -j
@end

@build:windows
$cmake --build %project%/build --config Debug
@end
*/
```

Run one of these with `Ctrl+R` while the caret is inside the block, click the highlighted `@name`, or
use **Run > Run Compile Function** on macOS. Lucid saves the file first.

A terminal panel opens beside the function. It streams output, and you can click it to focus and
type input. The red traffic light closes the panel and terminates the running process. Drag the
terminal title bar to move it, or drag a diagnostic card by its body.

Press `Ctrl+Shift+R` or use **Run > Search Project Functions…** to
search every function in the project. Suggestions show the function name, file, line and platform.
Pressing `Enter` opens the defining file and runs the function. Lucid caches the parsed index in
`.lucid/cache/functions.json` so later searches only re-read files whose size or modification time
changed.

### Variables

| Variable | Expands to |
| --- | --- |
| `%project%` | project root |
| `%projectName%` | project folder name |
| `%file%` | absolute path of the open file |
| `%fileDir%` | directory containing the open file |
| `%fileName%` | open file name |
| `%relative%` | open file path relative to the project root |
| `%line%` | line where the compile function starts |
| `%platform%` | `macOS`, `windows`, `linux`, `freebsd` or `openbsd` |

Unknown variables are left unchanged. Commands execute with `/bin/sh` on macOS, Linux and BSD. Compile
terminals are not yet available on Windows.

## Common messages

| Message | Meaning |
| --- | --- |
| `Open a project first` | Open the project folder with `Ctrl+O` before opening a file or running commands |
| `Compile function ... is not available on this platform` | The block has a platform suffix for another OS |
| `Compile function ... has no commands` | Add at least one line beginning with `$` before `@end` |
| `Compile terminals are not supported on this platform yet` | The PTY terminal is unavailable on Windows |

## Useful macOS menu items

The macOS menu bar mirrors the keyboard workflow:

- **File**: Open Project, Open File, Save, Close File
- **Edit**: Undo, Redo, Cut, Copy, Paste, Select All
- **Run**: Run Compile Function, Search Project Functions…, Close Terminal
- **Window**: Minimize, Zoom, Bring All to Front
- **Lucid**: About Lucid, Settings, Hide, Quit
