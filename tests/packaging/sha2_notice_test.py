#!/usr/bin/env python3
"""Verify pinned SHA-2 notice and release packaging references."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class NoticeTest(unittest.TestCase):
    def test_pinned_vendor_and_release_staging(self):
        vendor = ROOT / "third_party/sha2"
        self.assertEqual(hashlib.sha256((vendor / "sha2.c").read_bytes()).hexdigest(), "7c3d891116381f5119e823463772ea5994cbf18ab78843af52c507647f516b14")
        self.assertEqual(hashlib.sha256((vendor / "sha2.h").read_bytes()).hexdigest(), "f2e36e5ca90e46ea22ce47e957d338bb2af637730890c9a9e3d8286af8f70f0d")
        self.assertIn("Redistributions in binary form", (vendor / "LICENSE").read_text())
        workflow = (ROOT / ".github/workflows/release.yml").read_text()
        self.assertGreaterEqual(workflow.count("sha2-BSD-3-Clause.txt"), 2)
        self.assertIn("sha2-BSD-3-Clause.txt", (ROOT / "packaging/macos/bundle-libraries.sh").read_text())
        self.assertIn("sha2-BSD-3-Clause.txt", (ROOT / "src/CMakeLists.txt").read_text())

    @unittest.skipIf(os.name == "nt", "BSD shell packaging test requires POSIX")
    def test_bsd_archive_contains_notice(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for directory in ("packaging/bsd", "packaging/linux", "third_party/sha2", "build-bsd/bin/assets", "fakebin"):
                (root / directory).mkdir(parents=True, exist_ok=True)
            for name in ("packaging/bsd/package.sh", "packaging/linux/dev.lucid.editor.desktop", "packaging/linux/dev.lucid.editor.png", "third_party/sha2/LICENSE", "LICENSE", "THIRD_PARTY_NOTICES.md"):
                shutil.copy2(ROOT / name, root / name)
            for name in ("lucid", "lucid-shader-preview-helper"):
                (root / "build-bsd/bin" / name).write_bytes(b"fixture")
            fake_uname = root / "fakebin/uname"
            fake_uname.write_text("#!/bin/sh\nprintf 'FreeBSD\\n'\n")
            fake_uname.chmod(0o755)
            env = dict(os.environ, PATH=str(root / "fakebin") + os.pathsep + os.environ["PATH"])
            subprocess.run(["sh", "packaging/bsd/package.sh", "v1.0"], cwd=root, env=env, check=True)
            with tarfile.open(root / "dist/Lucid-v1.0-FreeBSD.tar.gz") as archive:
                notice = archive.extractfile("./licenses/sha2-BSD-3-Clause.txt")
                self.assertIsNotNone(notice)
                self.assertEqual(notice.read(), (ROOT / "third_party/sha2/LICENSE").read_bytes())


if __name__ == "__main__":
    unittest.main()
