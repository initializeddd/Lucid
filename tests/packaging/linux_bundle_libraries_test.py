import os
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "packaging/linux/bundle-libraries.sh"


class BundleLibrariesTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="lucid-bundle-")
        self.root = pathlib.Path(self.temp.name)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.lib = self.root / "host"
        self.lib.mkdir()
        self.dest = self.root / "bundle"
        for name in ("libmain.so.1", "libhelper.so.1", "libc.so.6"):
            (self.lib / name).write_text(name)
        (self.lib / "libmain-real.so.1").write_text("libmain.so.1")
        for name in ("lucid", "helper"):
            (self.root / name).touch()
        ldd = self.bin / "ldd"
        ldd.write_text("""#!/bin/sh
case "$1" in
  *lucid) name=libmain.so.1 ;;
  *helper) name=libhelper.so.1 ;;
  *) exit 2 ;;
esac
if [ "${FAIL_HELPER:-}" = 1 ] && [ "$name" = libhelper.so.1 ]; then
  echo 'libhelper.so.1 => not found'; exit 0
fi
if [ "${HOST_ONLY:-}" != 1 ] && [ -n "${LD_LIBRARY_PATH:-}" ] && [ -f "$LD_LIBRARY_PATH/$name" ]; then
  echo "$name => $LD_LIBRARY_PATH/$name (0x1)"
elif [ "${ALIAS_MAIN:-}" = 1 ] && [ "$name" = libmain.so.1 ]; then
  echo "$name => $FIXTURE_LIB/libmain-real.so.1 (0x1)"
else
  echo "$name => $FIXTURE_LIB/$name (0x1)"
fi
echo "libc.so.6 => $FIXTURE_LIB/libc.so.6 (0x2)"
echo '/lib64/ld-linux-x86-64.so.2 (0x4)'
if [ "${DIRECT_ABSOLUTE:-}" = 1 ]; then echo "$FIXTURE_LIB/libabsolute.so.1 (0x5)"; fi
if [ "$name" = libhelper.so.1 ]; then
  if [ "${HOST_ONLY:-}" != 1 ] && [ -f "${LD_LIBRARY_PATH:-/nonexistent}/libmain.so.1" ]; then
    echo "libmain.so.1 => $LD_LIBRARY_PATH/libmain.so.1 (0x3)"
  else
    echo "libmain.so.1 => $FIXTURE_LIB/libmain.so.1 (0x3)"
  fi
fi
""")
        ldd.chmod(0o755)
        self.env = dict(os.environ, PATH=str(self.bin) + os.pathsep + os.environ["PATH"], FIXTURE_LIB=str(self.lib), LD_LIBRARY_PATH="")

    def tearDown(self):
        self.temp.cleanup()

    def run_bundle(self):
        return subprocess.run(["bash", str(SCRIPT), str(self.root / "lucid"), str(self.root / "helper"), str(self.dest)], env=self.env, capture_output=True, text=True)

    def test_bundles_both_executables_and_excludes_system_libraries(self):
        result = self.run_bundle()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(sorted(p.name for p in self.dest.iterdir()), ["libhelper.so.1", "libmain.so.1"])

    def test_missing_helper_dependency_fails(self):
        self.env["FAIL_HELPER"] = "1"
        result = self.run_bundle()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Unresolved dependencies", result.stderr)

    def test_conflicting_existing_library_fails(self):
        self.dest.mkdir()
        (self.dest / "libhelper.so.1").write_text("different")
        result = self.run_bundle()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Conflicting bundled library", result.stderr)

    def test_requested_soname_is_preserved_when_host_file_name_differs(self):
        self.env["ALIAS_MAIN"] = "1"
        result = self.run_bundle()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.dest / "libmain.so.1").read_text(), "libmain.so.1")
        self.assertFalse((self.dest / "libmain-real.so.1").exists())

    def test_host_resolution_fails_even_with_identical_staged_bytes(self):
        self.env["HOST_ONLY"] = "1"
        result = self.run_bundle()
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue((self.dest / "libmain.so.1").exists())
        self.assertIn("does not resolve to bundled SONAME", result.stderr)

    def test_staged_symlink_to_host_is_rejected(self):
        self.dest.mkdir()
        (self.dest / "libmain.so.1").symlink_to(self.lib / "libmain.so.1")
        result = self.run_bundle()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("symlink", result.stderr)

    def test_direct_absolute_dependency_is_rejected(self):
        self.env["DIRECT_ABSOLUTE"] = "1"
        result = self.run_bundle()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Non-relocatable absolute dependency", result.stderr)


if __name__ == "__main__":
    unittest.main()
