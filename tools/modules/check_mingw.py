#!/usr/bin/env python3
"""Optional MinGW syntax check; does not link or execute Windows binaries."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
SOURCES = (
    "ApiRequests.cpp", "Coverage.cpp", "Cobertura.cpp", "DiagramPreview.cpp",
    "GitHubPullRequests.cpp", "SqlWorkbench.cpp", "DesignTokens.cpp", "CTestRunner.cpp",
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="x86_64-w64-mingw32-g++",
                        help="MinGW C++ compiler executable (default: %(default)s)")
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error("MinGW compiler not found: " + args.compiler)
    includes = ("src", "third_party/sqlite", "third_party/md4c", "third_party/sha2")
    command = [compiler, "-std=c++20", *("-I" + str(ROOT / path) for path in includes),
               "-fsyntax-only", *(str(ROOT / "src/modules" / source) for source in SOURCES)]
    print("Checking eight module translation units; this does not link or run Lucid.", flush=True)
    return subprocess.run(command, cwd=ROOT, check=False).returncode


if __name__ == "__main__":
    sys.exit(main())
