#include "platform/browser/WindowsWebPreview.hpp"
#include "platform/browser/WindowsWebPreviewProfile.hpp"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
int main() {
    wchar_t enabled[4]{}; if (!GetEnvironmentVariableW(L"LUCID_WEBVIEW2_SMOKE",enabled,4) || std::wstring(enabled)!=L"1") return 0;
    wchar_t loader[32768]{}; const DWORD n=GetEnvironmentVariableW(L"LUCID_WEBVIEW2_LOADER",loader,32768); if (!n || n>=32768) return 1;
    if (FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED))) return 2;
    int status=0;
    {
        HWND window=CreateWindowExW(0,L"STATIC",L"Lucid WebView2 smoke test",WS_OVERLAPPEDWINDOW,0,0,400,300,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        if (!window) status=3;
        else {
            ShowWindow(window,SW_SHOW); std::string error,version;
            lucid::platform::WindowsWebPreviewProfile profile; lucid::platform::WindowsWebPreview preview;
            lucid::modules::WebPreviewAssets assets;
            std::vector<lucid::modules::WebPreviewAsset> files; files.push_back({"/index.html","<!doctype html><html><body style=\"background:red\">preview</body></html>"});
            if (!lucid::platform::probeWindowsWebView2Runtime(loader,version,error) || !profile.create(error) || !assets.load(std::move(files),error)) status=4;
            else {
                bool done=false; lucid::platform::WindowsWebPreviewResult result;
                if (!preview.start(window,loader,profile,std::move(assets),"/index.html",256,128,true,[&](auto value) { result=std::move(value); done=true; },error)) status=5;
                else {
                    const auto watch=preview.processWatch(); auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(25);
                    while (!done && std::chrono::steady_clock::now()<deadline) { MSG msg{}; while (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); } preview.poll(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
                    if (!done || !result.error.empty() || result.png.size()<8 || result.png.compare(0,8,"\x89PNG\r\n\x1a\n",8)!=0) status=6;
                    preview.cancel(); deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
                    while ((!watch.exited() || !watch.profileReleased()) && std::chrono::steady_clock::now()<deadline) { MSG msg{}; while (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); } std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
                    if (!profile.cleanupAfterBrowserExit(watch,error) && !status) status=7;
                }
            }
            if (status && !profile.path().empty()) (void)profile.discardUnused(error);
            DestroyWindow(window);
        }
    }
    CoUninitialize(); return status;
}
#else
int main() { return 0; }
#endif
