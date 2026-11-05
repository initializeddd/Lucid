#include "platform/browser/WindowsWebPreviewHost.hpp"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <chrono>
#include <string>
#include <thread>
int main() {
    wchar_t enabled[4]{}; if (!GetEnvironmentVariableW(L"LUCID_WEBVIEW2_SMOKE",enabled,4) || std::wstring(enabled)!=L"1") return 0;
    HWND window=CreateWindowExW(0,L"STATIC",L"Lucid WebView2 host test",WS_OVERLAPPEDWINDOW,0,0,400,300,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if (!window) return 1;
    int status=0; ShowWindow(window,SW_SHOW);
    {
        lucid::modules::WebPreviewAssets assets; std::string error; std::vector<lucid::modules::WebPreviewAsset> files;
        files.push_back({"/index.html","<!doctype html><html><body style=\"background:red\">preview</body></html>"});
        lucid::platform::WindowsWebPreviewHost host;
        if (!assets.load(std::move(files),error) || !host.start(window,std::move(assets),"/index.html",256,128,true,error)) status=2;
        else {
            auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(25);
            while (!host.finished() && std::chrono::steady_clock::now()<deadline) { MSG msg{}; while (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); } host.poll(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (!host.finished() || !host.result().error.empty() || host.result().png.size()<8 || host.result().png.compare(0,8,"\x89PNG\r\n\x1a\n",8)!=0) status=3;
            if (host.running()) host.cancel();
            deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
            while (!host.retainedProfile().empty() && std::chrono::steady_clock::now()<deadline) { MSG msg{}; while (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); } host.poll(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (!host.retainedProfile().empty() && !status) status=4;
        }
    }
    DestroyWindow(window); return status;
}
#else
int main() { return 0; }
#endif
