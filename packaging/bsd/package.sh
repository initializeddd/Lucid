#!/bin/sh
# Run inside the target BSD after building Lucid. Runtime libraries remain system packages.
set -eu
tag=$1
platform=$(uname -s)
case "$platform" in FreeBSD|OpenBSD) ;; *) echo 'BSD packaging must run on its target OS.' >&2; exit 1 ;; esac
case "$tag" in v[0-9]*) ;; *) echo 'Invalid release tag.' >&2; exit 1 ;; esac
case "$tag" in *[!a-zA-Z0-9._-]*) echo 'Invalid release tag.' >&2; exit 1 ;; esac
stage=$(mktemp -d "${TMPDIR:-/tmp}/lucid-package.XXXXXXXX")
trap 'rm -rf "$stage"' EXIT HUP INT TERM
mkdir -p dist
cp build-bsd/bin/lucid "$stage/lucid"
cp build-bsd/bin/lucid-shader-preview-helper "$stage/lucid-shader-preview-helper"
cp -R build-bsd/bin/assets "$stage/assets"
cp LICENSE packaging/linux/dev.lucid.editor.desktop packaging/linux/dev.lucid.editor.png "$stage/"
mkdir -p "$stage/licenses"
cp third_party/sha2/LICENSE "$stage/licenses/sha2-BSD-3-Clause.txt"
printf '%s\n' 'Lucid requires the native GLFW, Vulkan loader, OpenSSL or LibreSSL libraries, and a compatible Vulkan driver.' 'Run ./lucid from this directory.' > "$stage/README.txt"
tar -czf "dist/Lucid-$tag-$platform.tar.gz" -C "$stage" .
