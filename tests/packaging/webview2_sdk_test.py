import contextlib
import hashlib
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

cmake = sys.argv[1]
repo = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("lucid_webview2_sdk", repo / "tools/webview2/fetch_sdk.py")
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)
with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary)
    archive = root / "sdk.nupkg"
    with zipfile.ZipFile(archive, "w") as package:
        package.writestr("build/native/include/WebView2.h", b"header")
        package.writestr("build/native/x64/WebView2Loader.dll", b"loader")
        package.writestr("LICENSE.txt", b"license")
    sdk.SHA256 = hashlib.sha256(archive.read_bytes()).hexdigest()
    output = root / "out"
    sys.argv = ["fetch_sdk.py", str(output), "--arch", "x64", "--archive", str(archive)]
    with contextlib.redirect_stdout(io.StringIO()):
        sdk.main()
    assert (output / "WebView2.h").read_bytes() == b"header"
    assert (output / "WebView2Loader.dll").read_bytes() == b"loader"
    assert (output / "LICENSE.txt").read_bytes() == b"license"
    try:
        sdk.main()
        raise AssertionError("existing output accepted")
    except ValueError:
        pass
    bad_output = root / "bad-out"
    archive.write_bytes(b"tampered")
    sys.argv = ["fetch_sdk.py", str(bad_output), "--arch", "x64", "--archive", str(archive)]
    try:
        sdk.main()
        raise AssertionError("tampered archive accepted")
    except ValueError:
        assert not bad_output.exists()
    cmake_script = root / "check.cmake"
    cmake_script.write_text(f'include("{(repo / "cmake/WebView2Sdk.cmake").as_posix()}")\nlucid_validate_webview2_sdk("{output.as_posix()}" "x64")\n')
    assert subprocess.run([cmake, "-P", str(cmake_script)], capture_output=True).returncode != 0
