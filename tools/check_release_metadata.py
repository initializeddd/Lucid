#!/usr/bin/env python3
"""Fail closed when a release tag disagrees with bundled product metadata."""
import argparse
import pathlib
import plistlib
import re
import sys
import xml.etree.ElementTree as ET

ROOT = pathlib.Path(__file__).resolve().parents[1]
TAG = re.compile(r"v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?\Z")


def check(root, tag):
    match = TAG.fullmatch(tag)
    if not match:
        raise ValueError("release tag must be vMAJOR.MINOR.PATCH with an optional prerelease")
    numeric = ".".join(match.group(i) for i in (1, 2, 3))
    version = tag[1:]
    prerelease = match.group(4) is not None
    header = (root / "src/core/Version.hpp").read_text(encoding="utf-8")
    def one(pattern, text, description):
        values = re.findall(pattern, text, re.MULTILINE)
        if len(values) != 1:
            raise ValueError(f"missing or ambiguous {description}")
        return values[0]
    actual = one(r'\bkVersionString\s*=\s*"([^"]+)"', header, "runtime version")
    if actual != version:
        raise ValueError(f"tag {tag} does not match runtime version {actual}")
    flag = one(r"\bkIsPrerelease\s*=\s*(true|false)\s*;", header, "prerelease flag")
    if (flag == "true") != prerelease:
        raise ValueError("runtime prerelease flag does not match tag")
    channel = one(r'\bkReleaseChannel\s*=\s*"([^"]+)"', header, "release channel")
    if (channel == "stable") == prerelease:
        raise ValueError("runtime release channel does not match tag")
    for key, value in zip(("Major", "Minor", "Patch"), match.group(1, 2, 3)):
        if one(r"\bkVersion" + key + r"\s*=\s*([0-9]+)\s*;", header, "runtime numeric version") != value:
            raise ValueError("runtime numeric version does not match tag")
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    if one(r"\bVERSION\s+([0-9]+\.[0-9]+\.[0-9]+)", cmake, "CMake version") != numeric:
        raise ValueError("CMake version does not match tag")
    plist = plistlib.loads((root / "packaging/macos/Info.plist").read_bytes())
    display = one(r'\bkAppDisplayName\s*=\s*"([^"]+)"', header, "runtime display name")
    if plist.get("CFBundleDisplayName") != display or plist.get("CFBundleName") != display or plist.get("CFBundleGetInfoString") != f"{display} {version}":
        raise ValueError("macOS display metadata does not match runtime")
    rc = (root / "packaging/windows/lucid.rc").read_text(encoding="utf-8")
    for key in ("FileVersion", "ProductVersion"):
        if one(r'VALUE\s+"' + key + r'",\s*"([^"]+)"', rc, "Windows " + key) != version:
            raise ValueError("Windows version does not match tag")
    if one(r'VALUE\s+"ProductName",\s*"([^"]+)"', rc, "Windows product name") != display:
        raise ValueError("Windows product name does not match runtime")
    wix = ET.parse(root / "packaging/windows/Lucid.wxs").getroot()
    product = wix.find("{http://schemas.microsoft.com/wix/2006/wi}Product")
    if product is None or product.get("Version") != numeric or product.get("Name") != display:
        raise ValueError("MSI product metadata does not match runtime")
    if not prerelease and "beta" in display.lower():
        raise ValueError("stable release retains a beta display name")
    if not prerelease:
        platform_menu = (root / "src/platform/app/PlatformApp.mm").read_text(encoding="utf-8")
        if "Lucid (Beta)" in platform_menu or "Lucid (Beta)" in rc:
            raise ValueError("stable release retains beta platform labels")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("tag")
    parser.add_argument("--root", type=pathlib.Path, default=ROOT)
    args = parser.parse_args()
    try:
        check(args.root, args.tag)
    except (OSError, ValueError, ET.ParseError, plistlib.InvalidFileException) as exc:
        print(f"Release metadata check failed: {exc}", file=sys.stderr)
        return 1
    print(f"Release metadata matches {args.tag}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
