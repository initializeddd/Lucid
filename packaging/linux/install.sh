#!/bin/sh
# Install Lucid for the current user from its latest GitHub release.
set -eu
repo=https://github.com/initializeddd/Lucid
command -v curl >/dev/null 2>&1 || { echo 'curl is required.' >&2; exit 1; }
command -v tar >/dev/null 2>&1 || { echo 'tar is required.' >&2; exit 1; }
case "$(uname -s):$(uname -m)" in
    Linux:x86_64|Linux:amd64) platform=Linux ;;
    FreeBSD:amd64|FreeBSD:x86_64) platform=FreeBSD ;;
    OpenBSD:amd64|OpenBSD:x86_64) platform=OpenBSD ;;
    *) echo 'No installer is available for this operating system and architecture.' >&2; exit 1 ;;
esac
tag=${LUCID_VERSION:-@LUCID_RELEASE_TAG@}
case "$tag" in v[0-9]* ) ;; *) echo 'Could not determine the latest release.' >&2; exit 1 ;; esac
case "$tag" in *[!a-zA-Z0-9._-]* ) echo 'Invalid release tag.' >&2; exit 1 ;; esac
archive="Lucid-$tag-$platform.tar.gz"
data_dir=${XDG_DATA_HOME:-"$HOME/.local/share"}
bin_dir=${XDG_BIN_HOME:-"$HOME/.local/bin"}
mkdir -p "$data_dir" "$bin_dir" "$data_dir/applications" "$data_dir/icons/hicolor/512x512/apps"
if [ -e "$bin_dir/lucid" ] && [ ! -L "$bin_dir/lucid" ]; then
    echo "Refusing to overwrite existing file: $bin_dir/lucid" >&2; exit 1
fi
tmp=$(mktemp -d "$data_dir/.lucid-install.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
curl -fL --retry 3 "$repo/releases/download/$tag/$archive" -o "$tmp/$archive"
curl -fL --retry 3 "$repo/releases/download/$tag/SHA256SUMS.txt" -o "$tmp/SHA256SUMS.txt"
expected=$(awk -v name="$archive" '$2 == name || $2 == "*" name { print $1; found=1 } END { if (!found) exit 1 }' "$tmp/SHA256SUMS.txt") || { echo 'Release checksum missing.' >&2; exit 1; }
case "$expected" in *[!0123456789abcdefABCDEF]*|'') echo 'Invalid release checksum.' >&2; exit 1 ;; esac
[ "${#expected}" -eq 64 ] || { echo 'Invalid release checksum.' >&2; exit 1; }
if command -v sha256sum >/dev/null 2>&1; then actual=$(sha256sum "$tmp/$archive"); actual=${actual%% *}
elif command -v sha256 >/dev/null 2>&1; then actual=$(sha256 -q "$tmp/$archive")
elif command -v shasum >/dev/null 2>&1; then actual=$(shasum -a 256 "$tmp/$archive"); actual=${actual%% *}
else echo 'A SHA-256 checksum utility is required.' >&2; exit 1; fi
[ "$actual" = "$expected" ] || { echo 'Release checksum mismatch.' >&2; exit 1; }
mkdir "$tmp/payload"
tar -xzf "$tmp/$archive" -C "$tmp/payload"
[ -x "$tmp/payload/lucid" ] && [ -d "$tmp/payload/assets" ] || { echo 'Invalid release archive.' >&2; exit 1; }
# Never discard an existing install until the replacement has been verified.
old="$data_dir/lucid.previous.$$"
if [ -e "$data_dir/lucid" ]; then mv "$data_dir/lucid" "$old"; fi
if ! mv "$tmp/payload" "$data_dir/lucid"; then
    [ ! -e "$old" ] || mv "$old" "$data_dir/lucid"
    exit 1
fi
[ ! -e "$old" ] || rm -rf "$old"
ln -sf "$data_dir/lucid/lucid" "$bin_dir/lucid"
cp "$data_dir/lucid/dev.lucid.editor.png" "$data_dir/icons/hicolor/512x512/apps/dev.lucid.editor.png"
# Desktop Exec uses double quotes and escaping, not shell expansion.
escaped=$(printf '%s' "$data_dir/lucid/lucid" | sed 's/[%]/%%/g; s/[\\"`$|\&]/\\&/g')
sed "s|^Exec=.*|Exec=\"$escaped\" %f|" "$data_dir/lucid/dev.lucid.editor.desktop" > "$data_dir/applications/dev.lucid.editor.desktop"
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$data_dir/applications" >/dev/null 2>&1 || true
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q "$data_dir/icons/hicolor" >/dev/null 2>&1 || true
printf '\nLucid installed. Open it from your application menu or run %s/lucid.\n' "$bin_dir"
