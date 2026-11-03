#!/bin/bash
set -euo pipefail
app=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
output=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
: "${DMGBUILD:?Set DMGBUILD to the dmgbuild executable}"
tmp=$(mktemp -d "${TMPDIR:-/tmp}/lucid-dmg.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT
swift "$(dirname "$0")/dmg-background.swift" "$tmp/background.png"
"$DMGBUILD" -s "$(dirname "$0")/dmg-settings.py" -D "app=$app" \
    -D "background=$tmp/background.png" 'Lucid' "$output"
