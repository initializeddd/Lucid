#include "platform/browser/WindowsWebPreview.hpp"
#include "platform/browser/WindowsWebPreviewProfile.hpp"
#include <filesystem>
#include <string>
int main() {
    lucid::modules::WebPreviewAssets assets; std::string error;
    if (!assets.load({{"/index.html","<html></html>"}},error)) return 1;
    lucid::platform::WindowsWebPreview preview; bool called=false;
    lucid::platform::WindowsWebPreviewProfile profile;
    const auto path=std::filesystem::temp_directory_path()/"missing-webview2-loader.dll";
    if (preview.start(nullptr,path,profile,assets,"/index.html",640,480,false,[&](auto) { called=true; },error) || error.empty()) return 2;
    preview.poll(); preview.cancel(); if (called) return 3;
    const auto watch=preview.processWatch(); if (watch.observed() || watch.exited() || watch.profileReleased()) return 6;
    std::string version="sentinel"; if (lucid::platform::probeWindowsWebView2Runtime(path,version,error) || !version.empty() || error.empty()) return 7;
#if !defined(_WIN32)
    if (profile.create(error) || error.empty() || !profile.path().empty()) return 4;
    if (profile.cleanupAfterBrowserExit(watch,error) || error.empty() || !profile.discardUnused(error)) return 5;
#endif
    return 0;
}
