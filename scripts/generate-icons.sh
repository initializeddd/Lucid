#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ICON_SOURCE="$ROOT/packaging/macos/Lucid.icon"
MAC_ICNS="$ROOT/packaging/macos/Lucid.icns"
MAC_CAR="$ROOT/packaging/macos/Assets.car"
WINDOWS_SVG="$ROOT/assets/icons/lucid-windows.svg"
WINDOWS_ICO="$ROOT/packaging/windows/lucid.ico"
LINUX_PNG="$ROOT/packaging/linux/dev.lucid.editor.png"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/lucid-icons.XXXXXX")
trap 'rm -rf "$TMP"' EXIT

render_svg() {
    svg=$1
    output=$2
    target=$(basename "$svg")
    mkdir -p "$output"
    qlmanage -t -s 1024 -o "$output" "$svg" >/dev/null 2>&1
    mv "$output/$target.png" "$output/source.png"
}

if command -v xcrun >/dev/null 2>&1 && xcrun --find actool >/dev/null 2>&1; then
    mkdir -p "$TMP/macos"
    xcrun actool \
        --compile "$TMP/macos" \
        --platform macosx \
        --minimum-deployment-target 26.0 \
        --app-icon Lucid \
        --output-partial-info-plist "$TMP/macos/partial.plist" \
        "$ICON_SOURCE" >/dev/null
    cp "$TMP/macos/Lucid.icns" "$MAC_ICNS"
    cp "$TMP/macos/Assets.car" "$MAC_CAR"
else
    render_svg "$ROOT/assets/icons/lucid-macos.svg" "$TMP/macos-flat"
    mkdir -p "$TMP/Lucid.iconset"
    for size in 16 32 128 256 512; do
        sips -z "$size" "$size" "$TMP/macos-flat/source.png" \
            --out "$TMP/Lucid.iconset/icon_${size}x${size}.png" >/dev/null
        doubled=$((size * 2))
        sips -z "$doubled" "$doubled" "$TMP/macos-flat/source.png" \
            --out "$TMP/Lucid.iconset/icon_${size}x${size}@2x.png" >/dev/null
    done
    iconutil -c icns "$TMP/Lucid.iconset" -o "$MAC_ICNS"
fi

render_svg "$WINDOWS_SVG" "$TMP/windows"
mkdir -p "$TMP/ico"
for size in 16 24 32 48 64 128 256; do
    sips -z "$size" "$size" "$TMP/windows/source.png" \
        --out "$TMP/ico/icon-${size}.png" >/dev/null
done
python3 - "$TMP/ico" "$WINDOWS_ICO" <<'PY'
import pathlib
import struct
import sys

source = pathlib.Path(sys.argv[1])
output = pathlib.Path(sys.argv[2])
sizes = [16, 24, 32, 48, 64, 128, 256]
images = [(size, (source / f"icon-{size}.png").read_bytes()) for size in sizes]
offset = 6 + 16 * len(images)

with output.open("wb") as stream:
    stream.write(struct.pack("<HHH", 0, 1, len(images)))
    for size, data in images:
        dimension = 0 if size == 256 else size
        stream.write(struct.pack("<BBBBHHII", dimension, dimension, 0, 0, 1, 32,
                                 len(data), offset))
        offset += len(data)
    for _, data in images:
        stream.write(data)
PY

sips -z 512 512 "$TMP/windows/source.png" --out "$LINUX_PNG" >/dev/null

printf 'Generated %s\nGenerated %s\nGenerated %s\nGenerated %s\n' \
    "$MAC_ICNS" "$MAC_CAR" "$WINDOWS_ICO" "$LINUX_PNG"
