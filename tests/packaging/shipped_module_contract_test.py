#!/usr/bin/env python3
"""Ensure an unrelated extra module cannot hide a missing requested module."""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


executable, source = sys.argv[1:]
with tempfile.TemporaryDirectory(prefix="lucid-shipped-contract-") as temp:
    modules = Path(temp) / "modules"
    shutil.copytree(source, modules)
    shutil.rmtree(modules / "lucid-shader-workbench")
    shutil.copytree(modules / "lucid-binary-inspector", modules / "lucid-extra-module")
    result = subprocess.run([executable, str(modules)], capture_output=True, text=True, check=False)
    if result.returncode == 0 or "Missing requested module: lucid-shader-workbench" not in result.stderr:
        raise SystemExit("A missing requested module was not rejected")
