# Run only on an interactive Windows desktop with an installed WebView2 Runtime.
# These tests execute trusted local HTML/JavaScript and create private browser
# profiles. WebView2 and its runtime may access the network; these tests do not
# establish network isolation or that WebView2 is a security sandbox.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string] $SdkDirectory,
    [string] $BuildDirectory = "build-webview2-smoke",
    [ValidateSet("x64", "x86", "arm64")] [string] $Architecture = "x64",
    [ValidateRange(15, 300)] [int] $TimeoutSeconds = 45
)
$ErrorActionPreference = "Stop"
if ($env:OS -ne "Windows_NT" -or -not [Environment]::UserInteractive) {
    throw "Run this test from an interactive Windows desktop session."
}
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "../.."))
$sdk = [IO.Path]::GetFullPath($SdkDirectory)
$build = [IO.Path]::GetFullPath($BuildDirectory)
if (-not (Test-Path -LiteralPath $sdk -PathType Container)) {
    throw "SDK directory not found. Prepare the pinned SDK with tools/webview2/fetch_sdk.py first."
}
if (Test-Path -LiteralPath $build) {
    if (-not (Test-Path -LiteralPath $build -PathType Container) -or (Get-ChildItem -LiteralPath $build -Force | Select-Object -First 1)) {
        throw "Choose a new or empty build directory; this script does not replace an existing build."
    }
}
$loader = Join-Path $sdk "WebView2Loader.dll"
if (-not (Test-Path -LiteralPath $loader -PathType Leaf)) { throw "Pinned WebView2 loader not found in SDK directory." }
$targets = @(
    "lucid_windows_web_preview_smoke_test",
    "lucid_windows_web_preview_session_smoke_test",
    "lucid_windows_web_preview_host_smoke_test",
    "lucid_glfw_web_preview_bridge_smoke_test",
    "lucid_windows_web_preview_security_smoke_test"
)
Write-Host "Preparing an opt-in test build. WebView2 preview remains disabled in Lucid's module catalog."
$configureArguments = @("-S", $repository, "-B", $build, "-DLUCID_ENABLE_WEBVIEW2=ON", "-DLUCID_WEBVIEW2_UI_TEST=ON", "-DLUCID_WEBVIEW2_SDK_DIR=$sdk")
if (-not $env:CMAKE_GENERATOR -or $env:CMAKE_GENERATOR -like "Visual Studio*") {
    # Visual Studio's default target is not necessarily the SDK architecture.
    $platform = if ($Architecture -eq "x86") { "Win32" } else { $Architecture }
    $configureArguments += @("-A", $platform)
} else {
    Write-Host "Using $env:CMAKE_GENERATOR; select the $Architecture compiler in its environment. CMake verifies the SDK matches the target."
}
& cmake @configureArguments
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed, including pinned SDK verification." }
& cmake --build $build --config Release --parallel 2 --target $targets
if ($LASTEXITCODE -ne 0) { throw "WebView2 smoke targets failed to build." }
$oldSmoke = [Environment]::GetEnvironmentVariable("LUCID_WEBVIEW2_SMOKE", "Process")
$oldLoader = [Environment]::GetEnvironmentVariable("LUCID_WEBVIEW2_LOADER", "Process")
try {
    [Environment]::SetEnvironmentVariable("LUCID_WEBVIEW2_SMOKE", "1", "Process")
    [Environment]::SetEnvironmentVariable("LUCID_WEBVIEW2_LOADER", $loader, "Process")
    foreach ($target in $targets) {
        $executable = Join-Path $build "bin/$target.exe"
        if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw "Missing smoke executable: $executable" }
        Write-Host "Running $target on the Windows desktop..."
        $process = Start-Process -FilePath $executable -NoNewWindow -PassThru
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            & taskkill /PID $process.Id /T /F 2>$null | Out-Null
            throw "$target timed out. A WebView2 process or private temporary profile may remain; inspect before cleanup."
        }
        if ($process.ExitCode -ne 0) { throw "$target failed with exit code $($process.ExitCode). Inspect temporary profiles before cleanup." }
        Write-Host "$target passed."
    }
    Write-Host "All five platform smoke tests passed on this machine. The editor UI is not exercised; this is not proof of network isolation or a security sandbox."
} finally {
    [Environment]::SetEnvironmentVariable("LUCID_WEBVIEW2_SMOKE", $oldSmoke, "Process")
    [Environment]::SetEnvironmentVariable("LUCID_WEBVIEW2_LOADER", $oldLoader, "Process")
}
