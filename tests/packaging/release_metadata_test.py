import importlib.util
import pathlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("release_metadata", ROOT / "tools/check_release_metadata.py")
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleaseMetadataTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.tmp.name)
        fixtures = {
            "src/core/Version.hpp": 'kVersionString = "0.2.1-beta.2";\nkAppDisplayName = "Lucid (Beta)";\nkReleaseChannel = "beta";\nkIsPrerelease = true;\nkVersionMajor = 0;\nkVersionMinor = 2;\nkVersionPatch = 1;\n',
            "src/platform/app/PlatformApp.mm": 'auto title = "Lucid (Beta)";\n',
            "CMakeLists.txt": "project(Lucid VERSION 0.2.1)\n",
            "packaging/macos/Info.plist": '<?xml version="1.0"?><plist version="1.0"><dict><key>CFBundleDisplayName</key><string>Lucid (Beta)</string><key>CFBundleName</key><string>Lucid (Beta)</string><key>CFBundleGetInfoString</key><string>Lucid (Beta) 0.2.1-beta.2</string></dict></plist>',
            "packaging/windows/lucid.rc": 'VALUE "ProductName", "Lucid (Beta)"\nVALUE "FileVersion", "0.2.1-beta.2"\nVALUE "ProductVersion", "0.2.1-beta.2"\n',
            "packaging/windows/Lucid.wxs": '<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi"><Product Name="Lucid (Beta)" Version="0.2.1"/></Wix>',
        }
        for path, contents in fixtures.items():
            dest = self.root / path
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_text(contents, encoding="utf-8")
        self.addCleanup(self.tmp.cleanup)

    def replace(self, path, old, new):
        file = self.root / path
        text = file.read_text(encoding="utf-8")
        self.assertIn(old, text)
        file.write_text(text.replace(old, new), encoding="utf-8")

    def test_current_beta_matches(self):
        release.check(self.root, "v0.2.1-beta.2")

    def test_stable_tag_rejects_beta_binary(self):
        with self.assertRaises(ValueError):
            release.check(self.root, "v0.2.1")

    def test_rejects_invalid_tag_and_mismatched_installer(self):
        with self.assertRaises(ValueError):
            release.check(self.root, "v0.2.1-beta.2/other")
        self.replace("packaging/windows/lucid.rc", 'VALUE "ProductVersion", "0.2.1-beta.2"', 'VALUE "ProductVersion", "0.2.0"')
        with self.assertRaises(ValueError):
            release.check(self.root, "v0.2.1-beta.2")

    def test_matching_stable_metadata(self):
        for path in ("src/core/Version.hpp", "src/platform/app/PlatformApp.mm", "packaging/macos/Info.plist", "packaging/windows/lucid.rc", "packaging/windows/Lucid.wxs"):
            file = self.root / path
            file.write_text(file.read_text(encoding="utf-8").replace("0.2.1-beta.2", "0.2.1").replace("Lucid (Beta)", "Lucid"), encoding="utf-8")
        self.replace("src/core/Version.hpp", "kIsPrerelease = true", "kIsPrerelease = false")
        self.replace("src/core/Version.hpp", 'kReleaseChannel = "beta"', 'kReleaseChannel = "stable"')
        release.check(self.root, "v0.2.1")

    def test_stable_rejects_beta_platform_label(self):
        self.test_matching_stable_metadata()
        self.replace("src/platform/app/PlatformApp.mm", '"Lucid"', '"Lucid (Beta)"')
        with self.assertRaises(ValueError):
            release.check(self.root, "v0.2.1")


if __name__ == "__main__":
    unittest.main()
