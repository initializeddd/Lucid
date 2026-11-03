#!/bin/sh
set -eu
script=$1
tmp=$(mktemp -d "${TMPDIR:-/tmp}/lucid-installer-test.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/bin" "$tmp/release" "$tmp/payload/assets" "$tmp/home"
printf '#!/bin/sh\nexit 0\n' > "$tmp/payload/lucid"
chmod +x "$tmp/payload/lucid"
printf '[Desktop Entry]\nExec=lucid %%f\n' > "$tmp/payload/dev.lucid.editor.desktop"
printf 'icon' > "$tmp/payload/dev.lucid.editor.png"
for platform in Linux FreeBSD OpenBSD; do
    tar -czf "$tmp/release/Lucid-v0.2.0-beta.1-$platform.tar.gz" -C "$tmp/payload" .
done
for archive in "$tmp"/release/*.tar.gz; do
    if command -v sha256sum >/dev/null 2>&1; then checksum=$(sha256sum "$archive"); checksum=${checksum%% *}
    elif command -v sha256 >/dev/null 2>&1; then checksum=$(sha256 -q "$archive")
    else checksum=$(shasum -a 256 "$archive"); checksum=${checksum%% *}; fi
    printf '%s  %s\n' "$checksum" "${archive##*/}" >> "$tmp/release/SHA256SUMS.txt"
done
cat > "$tmp/bin/uname" <<'MOCK'
#!/bin/sh
case "$1" in -s) printf '%s\n' "$TEST_OS" ;; -m) printf 'amd64\n' ;; esac
MOCK
cat > "$tmp/bin/curl" <<'MOCK'
#!/bin/sh
url= dest=
while [ "$#" -gt 0 ]; do
    case "$1" in https://*) url=$1 ;; -o) shift; dest=$1 ;; --retry) shift ;; esac
    shift
done
[ -n "$url" ] && [ -n "$dest" ] || exit 1
cp "$TEST_RELEASE/${url##*/}" "$dest"
MOCK
chmod +x "$tmp/bin/uname" "$tmp/bin/curl"
for platform in Linux FreeBSD OpenBSD; do
    TEST_OS=$platform TEST_RELEASE="$tmp/release" LUCID_VERSION=v0.2.0-beta.1 \
        HOME="$tmp/home/$platform" PATH="$tmp/bin:$PATH" sh "$script" > /dev/null
    test -x "$tmp/home/$platform/.local/bin/lucid"
    test -f "$tmp/home/$platform/.local/share/applications/dev.lucid.editor.desktop"
done
printf '\n' >> "$tmp/release/Lucid-v0.2.0-beta.1-Linux.tar.gz"
if TEST_OS=Linux TEST_RELEASE="$tmp/release" LUCID_VERSION=v0.2.0-beta.1 \
    HOME="$tmp/home/Linux" PATH="$tmp/bin:$PATH" sh "$script" > /dev/null 2>&1; then
    echo 'Corrupt archive was accepted' >&2; exit 1
fi
test -x "$tmp/home/Linux/.local/bin/lucid"
