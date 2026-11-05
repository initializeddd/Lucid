#!/bin/bash
set -euo pipefail
app=$1
exe="$app/Contents/MacOS/Lucid"
frameworks="$app/Contents/Frameworks"
icd="$app/Contents/Resources/vulkan/icd.d"
mkdir -p "$app/Contents/Resources/licenses"
cp "$(dirname "$0")/../../third_party/sha2/LICENSE" "$app/Contents/Resources/licenses/sha2-BSD-3-Clause.txt"
mkdir -p "$frameworks" "$icd"
for spec in "glfw:libglfw.3.dylib" "vulkan-loader:libvulkan.1.dylib"; do
    formula=${spec%%:*}; name=${spec#*:}
    lib="$(brew --prefix "$formula")/lib/$name"
    old=$(otool -L "$exe" | awk -v name="$name" 'index($1,name) {print $1; exit}')
    test -n "$old"
    cp -L "$lib" "$frameworks/$name"
    chmod u+w "$frameworks/$name"
    install_name_tool -id "@executable_path/../Frameworks/$name" "$frameworks/$name"
    install_name_tool -change "$old" "@executable_path/../Frameworks/$name" "$exe"
done
helper="$app/Contents/MacOS/lucid-shader-preview-helper"
old=$(otool -L "$helper" | awk 'index($1,"libvulkan.1.dylib") {print $1; exit}')
test -n "$old"
install_name_tool -change "$old" '@executable_path/../Frameworks/libvulkan.1.dylib' "$helper"
cp -L "$(brew --prefix molten-vk)/lib/libMoltenVK.dylib" "$frameworks/libMoltenVK.dylib"
chmod u+w "$frameworks/libMoltenVK.dylib"
install_name_tool -id '@executable_path/../Frameworks/libMoltenVK.dylib' "$frameworks/libMoltenVK.dylib"
cat > "$icd/MoltenVK_icd.json" <<'JSON'
{"file_format_version":"1.0.0","ICD":{"library_path":"../../../Frameworks/libMoltenVK.dylib","api_version":"1.2.0","is_portability_driver":true}}
JSON
for lib in "$frameworks/"*.dylib; do codesign --force --sign - --timestamp=none "$lib"; done
codesign --force --sign - --timestamp=none "$helper"
codesign --force --sign - --timestamp=none "$app"
