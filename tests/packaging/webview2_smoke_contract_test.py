"""Static contract checks; this does not run PowerShell or WebView2."""
from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]
script = (repo / "tools/webview2/run_smoke.ps1").read_text(encoding="utf-8")
cmake = (repo / "tests/CMakeLists.txt").read_text(encoding="utf-8")
options = (repo / "cmake/LucidOptions.cmake").read_text(encoding="utf-8")
match = re.search(r"\$targets\s*=\s*@\((.*?)\)", script, re.S)
assert match, "smoke target list is absent"
targets = re.findall(r'"(lucid_[a-z0-9_]+_smoke_test)"', match.group(1))
expected = {
    "lucid_windows_web_preview_smoke_test",
    "lucid_windows_web_preview_session_smoke_test",
    "lucid_windows_web_preview_host_smoke_test",
    "lucid_glfw_web_preview_bridge_smoke_test",
    "lucid_windows_web_preview_security_smoke_test",
}
assert len(targets) == len(expected) and set(targets) == expected
for target in targets:
    name = target.removeprefix("lucid_")
    source = repo / "tests/platform" / f"{name}.cpp"
    text = source.read_text(encoding="utf-8")
    assert "LUCID_WEBVIEW2_SMOKE" in text, f"{name} lacks explicit runtime opt-in"
    if name.startswith("windows_web_preview_security_"):
        assert target in cmake and "EXCLUDE_FROM_ALL" in cmake
    else:
        stem = name.removesuffix("_smoke_test")
        assert re.search(r"foreach\(web_preview_smoke IN ITEMS[^)]*\b" + re.escape(stem) + r"\b", cmake)
assert 'SetEnvironmentVariable("LUCID_WEBVIEW2_SMOKE", "1", "Process")' in script
assert 'SetEnvironmentVariable("LUCID_WEBVIEW2_LOADER", $loader, "Process")' in script
assert "UserInteractive" in script and "WaitForExit" in script and "taskkill" in script
assert '[ValidateSet("x64", "x86", "arm64")]' in script
assert '$Architecture -eq "x86"' in script and '"Win32"' in script
assert '$configureArguments += @("-A", $platform)' in script
assert "editor UI is not exercised" in script and "not proof of network isolation" in script
assert re.search(r"option\(LUCID_ENABLE_WEBVIEW2\s+\"[^\"]+\"\s+OFF\)", options)
assert re.search(r"option\(LUCID_WEBVIEW2_UI_TEST\s+\"[^\"]+\"\s+OFF\)", options)
