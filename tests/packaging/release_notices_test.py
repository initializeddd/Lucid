#!/usr/bin/env python3
"""Verify every release package stages Lucid's license and third-party notices."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


def workflow_step(text, name):
    marker = f"- name: {name}"
    start = text.index(marker)
    end = text.find("\n      - name:", start + len(marker))
    return text[start:] if end == -1 else text[start:end]


class ReleaseNoticeTest(unittest.TestCase):
    def test_macos_bundle_stages_notices(self):
        script = (ROOT / "packaging/macos/bundle-libraries.sh").read_text()
        self.assertIn('cp "$(dirname "$0")/../../LICENSE"', script)
        self.assertIn('cp "$(dirname "$0")/../../THIRD_PARTY_NOTICES.md"', script)

    def test_bsd_package_stages_notices(self):
        script = (ROOT / "packaging/bsd/package.sh").read_text()
        self.assertIn("cp LICENSE THIRD_PARTY_NOTICES.md", script)

    def test_release_workflow_stages_notices(self):
        workflow = (ROOT / ".github/workflows/release.yml").read_text()
        linux = workflow_step(workflow, "Package (Linux)")
        self.assertIn('cp LICENSE "${stage}/LICENSE"', linux)
        self.assertIn('cp THIRD_PARTY_NOTICES.md "${stage}/THIRD_PARTY_NOTICES.md"', linux)
        windows = workflow_step(workflow, "Package (Windows)")
        self.assertIn("Copy-Item LICENSE $stage", windows)
        self.assertIn("Copy-Item THIRD_PARTY_NOTICES.md $stage", windows)


if __name__ == "__main__":
    unittest.main()
