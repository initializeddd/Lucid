#!/usr/bin/env bash
# Bundle dependencies for both executables. Do not rely on the build host's
# library search path to provide the helper's dependencies after installation.
set -euo pipefail
if [[ $# != 3 ]]; then echo "usage: $0 LUCID HELPER DESTINATION" >&2; exit 2; fi
lucid=$1 helper=$2 destination=$3
mkdir -p "$destination"
destination=$(cd "$destination" && pwd -P)
system_library() {
  case "$1" in
    libc.so.*|libm.so.*|libpthread.so.*|libdl.so.*|librt.so.*|libutil.so.*|libgcc_s.so.*|libstdc++.so.*|libresolv.so.*|ld-linux*) return 0 ;;
    *) return 1 ;;
  esac
}
check_direct_entries() {
  local entry name
  while IFS= read -r entry; do
    [[ -n "$entry" ]] || continue
    name=$(basename "$entry")
    # The dynamic loader is normally printed without an arrow. An absolute
    # DT_NEEDED for any other library cannot be relocated into our bundle.
    if [[ "$entry" == *"/../"* || "$entry" == *"/./"* ]] || [[ "$name" != ld-linux* ]]; then
      echo "Non-relocatable absolute dependency: $entry" >&2; return 1
    fi
    case "$entry" in /lib/*|/lib64/*|/usr/lib/*|/usr/lib64/*) ;; *) echo "Non-relocatable absolute dependency: $entry" >&2; return 1 ;; esac
  done < <(printf '%s\n' "$1" | awk '$1 ~ /^\// && $2 != "=>" {print $1}')
}
for executable in "$lucid" "$helper"; do
  if ! dependencies=$(ldd "$executable"); then echo "Could not inspect dependencies: $executable" >&2; exit 1; fi
  if [[ "$dependencies" == *"not found"* ]]; then echo "Unresolved dependencies: $executable" >&2; exit 1; fi
  check_direct_entries "$dependencies"
  while IFS=' ' read -r name library; do
    [[ -n "$name" ]] || continue
    if [[ ! "$name" =~ ^[A-Za-z0-9_+.-]+$ || "$name" == . || "$name" == .. || "$library" != /* ]]; then
      echo "Unsupported dependency entry: $name" >&2; exit 1
    fi
    if system_library "$name"; then continue; fi
    if [[ -L "$destination/$name" ]]; then echo "Bundled library is a symlink: $name" >&2; exit 1; fi
    if [[ -e "$destination/$name" ]]; then
      if ! cmp -s "$library" "$destination/$name"; then echo "Conflicting bundled library: $name" >&2; exit 1; fi
      continue
    fi
    cp -L "$library" "$destination/$name"
  done < <(printf '%s\n' "$dependencies" | awk '$2 == "=>" {print $1, $3}')
done
# Verify each executable against the staging directory, and ensure every
# non-system dependency is represented in the bundle even if ldd finds it on
# the packaging machine through its default search path.
for executable in "$lucid" "$helper"; do
  if ! dependencies=$(LD_LIBRARY_PATH="$destination" ldd "$executable"); then echo "Could not verify dependencies: $executable" >&2; exit 1; fi
  if [[ "$dependencies" == *"not found"* ]]; then echo "Unresolved staged dependencies: $executable" >&2; exit 1; fi
  check_direct_entries "$dependencies"
  while IFS=' ' read -r name library; do
    [[ -n "$name" ]] || continue
    if [[ ! "$name" =~ ^[A-Za-z0-9_+.-]+$ || "$name" == . || "$name" == .. || "$library" != /* ]]; then
      echo "Unsupported dependency entry: $name" >&2; exit 1
    fi
    if system_library "$name"; then continue; fi
    # DT_RPATH may override LD_LIBRARY_PATH. Refuse a host-resolved library
    # even when it has the same bytes as the staged SONAME.
    if [[ -L "$destination/$name" || ! -f "$destination/$name" ]] || [[ "$(cd "$(dirname "$library")" && pwd -P)/$(basename "$library")" != "$destination/$name" ]]; then
      echo "Dependency does not resolve to bundled SONAME: $name" >&2; exit 1
    fi
  done < <(printf '%s\n' "$dependencies" | awk '$2 == "=>" {print $1, $3}')
done
