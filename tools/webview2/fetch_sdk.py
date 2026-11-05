#!/usr/bin/env python3
"""Fetch pinned WebView2 SDK assets for an explicitly requested local build.

The WebView2 Runtime is separate. This does not install a runtime or enable Lucid
preview. The caller must include LICENSE.txt when redistributing these files.
"""
import argparse
import hashlib
import io
import os
from pathlib import Path
import sys
import urllib.request
import zipfile

VERSION = "1.0.2903.40"
SHA256 = "ef128016dd1e51c59178c827ed5b8aa3322c57afa8675d930f8109505542ad74"
URL = f"https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/{VERSION}/microsoft.web.webview2.{VERSION}.nupkg"
MAX_ARCHIVE = 32 * 1024 * 1024
MAX_ENTRY = 4 * 1024 * 1024


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="new directory to create")
    parser.add_argument("--arch", choices=("x64", "x86", "arm64"), required=True)
    parser.add_argument("--archive", type=Path, help="use a locally cached package instead of downloading")
    args = parser.parse_args()
    if args.destination.exists() or args.destination.is_symlink():
        raise ValueError("Destination already exists")
    if args.archive:
        with args.archive.open("rb") as source:
            payload = source.read(MAX_ARCHIVE + 1)
    else:
        with urllib.request.urlopen(URL, timeout=30) as source:
            payload = source.read(MAX_ARCHIVE + 1)
    if len(payload) > MAX_ARCHIVE or hashlib.sha256(payload).hexdigest() != SHA256:
        raise ValueError("SDK archive failed the pinned SHA-256 check")
    wanted = {
        "build/native/include/WebView2.h": "WebView2.h",
        f"build/native/{args.arch}/WebView2Loader.dll": "WebView2Loader.dll",
        "LICENSE.txt": "LICENSE.txt",
    }
    files = {}
    with zipfile.ZipFile(io.BytesIO(payload)) as archive:
        for source, target in wanted.items():
            entries = [item for item in archive.infolist() if item.filename == source]
            if len(entries) != 1 or entries[0].file_size > MAX_ENTRY or entries[0].is_dir():
                raise ValueError("SDK archive is missing a required bounded entry")
            files[target] = archive.read(entries[0])
    os.mkdir(args.destination, 0o700)
    try:
        for name, data in files.items():
            with (args.destination / name).open("xb") as output:
                output.write(data)
    except Exception:
        for name in files:
            try:
                (args.destination / name).unlink(missing_ok=True)
            except OSError:
                pass
        try:
            args.destination.rmdir()
        except OSError:
            pass
        raise
    print(f"Prepared pinned WebView2 SDK {VERSION} ({args.arch}); include LICENSE.txt when redistributing.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, zipfile.BadZipFile) as exc:
        print(f"WebView2 SDK preparation failed: {exc}", file=sys.stderr)
        sys.exit(1)
