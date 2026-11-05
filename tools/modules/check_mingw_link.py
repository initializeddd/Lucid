#!/usr/bin/env python3
"""Cross-link focused module tests into disposable Windows executables."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CASES = (
    ("diagram_preview", "DiagramPreview.cpp"),
    ("coverage_summary", "CoverageSummary.cpp"),
    ("test_runner_plus", "TestRunnerPlus.cpp"),
    ("hex_buffer", "HexBuffer.cpp"),
    ("rust_ownership", "RustOwnership.cpp"),
    ("benchmark_comparison", "BenchmarkComparison.cpp"),
    ("design_tokens", "DesignTokens.cpp"),
    ("release_notes", "ReleaseNotes.cpp"),
    ("accessibility_audit", "AccessibilityAudit.cpp"),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="x86_64-w64-mingw32-g++")
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error("MinGW compiler not found: " + args.compiler)
    with tempfile.TemporaryDirectory(prefix="lucid-mingw-link-") as directory:
        for test, source in CASES:
            output = Path(directory) / (test + ".exe")
            command = [compiler, "-std=c++20", "-I" + str(ROOT / "src"),
                       str(ROOT / "tests/workspace" / (test + "_test.cpp")),
                       str(ROOT / "src/modules" / source),
                       str(ROOT / "src/editor/json/Json.cpp")]
            if test == "rust_ownership":
                command.append(str(ROOT / "src/editor/document/Document.cpp"))
            command.extend(["-o", str(output)])
            result = subprocess.run(command, check=False)
            if result.returncode:
                return result.returncode
            with output.open("rb") as executable:
                if executable.read(2) != b"MZ":
                    raise RuntimeError("Compiler did not produce a Windows executable: " + test)
            print("Linked", test, "(not executed)", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
