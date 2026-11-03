# Building

Lucid builds with CMake on macOS, Linux, Windows, FreeBSD and OpenBSD. The project needs CMake 3.24 or newer, a
C++20 compiler, GLFW 3.3+, the Vulkan 1.2 headers and loader, and `glslangValidator`. GLFW is
fetched automatically when a system installation is not found.

For the short version, see the [README](../README.md#build-from-source). This page covers platform
details, packaging, signing and validation.

## Common options

| Option | Default | Description |
| --- | --- | --- |
| `LUCID_ENABLE_VALIDATION` | `ON` | Enable `VK_LAYER_KHRONOS_validation` in Debug builds |
| `LUCID_WARNINGS_AS_ERRORS` | `OFF` | Promote compiler warnings to errors |
| `LUCID_BUILD_TESTS` | `ON` | Build the test suite |

Pass options at configure time:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLUCID_WARNINGS_AS_ERRORS=ON
```

## macOS

```sh
xcode-select --install
brew install cmake ninja glfw glslang molten-vk vulkan-loader vulkan-headers

cmake --preset debug
cmake --build --preset debug
open build/debug/bin/Lucid.app
```

The build produces a complete `Lucid.app` bundle with `Info.plist`, `Lucid.icns`, an
`Assets.car` icon catalog and ad-hoc code signing. For an optimized bundle:

```sh
cmake --preset release
cmake --build --preset release
open build/release/bin/Lucid.app
```

Install it with:

```sh
cmake --install build/release --prefix /Applications
```

The install step copies the bundle and re-signs it, because replacing the executable or its icon
catalog invalidates the previous ad-hoc signature. If you copy the bundle manually with `ditto`, run
`codesign --force --deep --sign - /Applications/Lucid.app` afterwards.

The macOS deployment target defaults to 14.0; linked libraries may impose their own minimum. Set `-DCMAKE_OSX_DEPLOYMENT_TARGET=...` to override it, but the app
cannot run below the minimum version of its linked dependencies. The bundle is not self-contained;
bundle the dylibs or use a Developer ID build pipeline for distribution.

The ad-hoc signature is suitable for local use. For distribution outside your Mac, sign with a
Developer ID certificate and notarize the bundle:

```sh
codesign --force --deep --options runtime \
  --sign "Developer ID Application: Your Name (TEAMID)" build/release/bin/Lucid.app
xcrun notarytool submit Lucid.zip --wait
```

Lucid links against the Vulkan loader. MoltenVK must be discoverable as a Vulkan ICD. The LunarG SDK
installs an ICD entry in `/usr/local/share/vulkan/icd.d`; Homebrew installations usually do the same
if `molten-vk` is installed. If the loader cannot find a driver, point it at MoltenVK explicitly:

```sh
export VK_ICD_FILENAMES=/opt/homebrew/share/vulkan/icd.d/MoltenVK_icd.json
```

or, with a LunarG SDK:

```sh
export VK_ICD_FILENAMES="$VULKAN_SDK/share/vulkan/icd.d/MoltenVK_icd.json"
```

## Linux

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

Configure and build:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/bin/lucid
```

The Linux executable is named `lucid`. Install it with its desktop entry and icon:

```sh
cmake --install build --prefix ~/.local
```

The desktop file is `packaging/linux/dev.lucid.editor.desktop` and the icon is installed under
`share/icons/hicolor/512x512/apps`.

The release workflow produces a self-contained tarball. It copies the executable's shared
dependencies into `lib/` (except glibc, libstdc++ and the dynamic loader) and links Lucid with
`$ORIGIN/lib`, so the downloaded build runs without installing GLFW, the Vulkan loader or the
X11/Wayland development packages. A working Vulkan driver is still required.

## Windows

Install the **Vulkan SDK** from LunarG, CMake 3.24+, and a C++20 toolchain (Visual Studio 2022 with
the *Desktop development with C++* workload). The SDK provides the Vulkan loader, headers and
`glslangValidator`. GLFW is fetched automatically if a system installation is not found by CMake.

From **x64 Native Tools Command Prompt for VS 2022**:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\bin\Lucid.exe
```

For an optimized build use `--config Release` and run `.\build\bin\Lucid.exe`.

The executable embeds `packaging/windows/lucid.ico`. On Windows 11, Lucid applies a dark caption,
text and border color with DWM while keeping the standard minimize, maximize and close controls.
Compile functions and their PTY terminals currently run on macOS, Linux and BSD; the Windows platform
layer reports that the terminal is unavailable.

## FreeBSD and OpenBSD

Native builds require CMake, Ninja, Git, GLFW, Vulkan headers and loader, glslang, and system crypto libraries. Install these with `pkg` on FreeBSD or `pkg_add` on OpenBSD, then configure with `-DCMAKE_PREFIX_PATH=/usr/local`. The release workflow builds and tests both systems in virtual machines; graphical desktop and driver coverage remains limited. The archives use system libraries.

## Icons

The macOS source icon is the layered `packaging/macos/Lucid.icon` package. It is opened with Apple's
Icon Composer and rendered through `ictool`/`actool` into `Lucid.icns` and `Assets.car`. The Windows
icon is generated from `assets/icons/lucid-windows.svg` into a multi-resolution `lucid.ico`. The
Linux PNG is generated from the same source.

Regenerate all committed icon artifacts on macOS with:

```sh
./scripts/generate-icons.sh
```

`actool` comes with Xcode. Without it, the script falls back to the flat
`assets/icons/lucid-macos.svg` and `iconutil`.

## Validation

Debug builds enable the Khronos validation layer when it is available. The renderer logs a warning
and continues if the layer is missing. Release builds disable validation by default; both can be
overridden with `-DLUCID_ENABLE_VALIDATION=ON|OFF`.

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| `glslangValidator was not found` | Install `glslang` / `glslang-tools`, or set `VULKAN_SDK` so `$VULKAN_SDK/bin` is searched |
| `Could NOT find Vulkan` | Install the Vulkan SDK or your distribution's `vulkan-headers` and loader development packages |
| `Vulkan surface creation failed` (macOS) | Point `VK_ICD_FILENAMES` at `MoltenVK_icd.json` |
| GLFW errors about X11 / Wayland | Install the X11 and Wayland development packages listed above |
| `glfw3` not found | Let CMake fetch it, or install `glfw` / `glfw-devel` / `glfw3-dev` |
