#include "platform/browser/WindowsWebPreviewSession.hpp"
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
        HWND window=CreateWindowExW(0,L"STATIC",L"Lucid WebView2 session test",WS_OVERLAPPEDWINDOW,0,0,400,300,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        if (!window) status=3;
        else {
            ShowWindow(window,SW_SHOW); lucid::modules::WebPreviewAssets assets; std::string error;
            std::vector<lucid::modules::WebPreviewAsset> files; files.push_back({"/index.html","<!doctype html><html><body style=\"background:red\">preview</body></html>"});
            lucid::platform::WindowsWebPreviewSession session;
            if (!assets.load(std::move(files),error) || !session.start(window,loader,std::move(assets),"/index.html",256,128,true,error)) status=4;
            else {
                auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(25);
                while (!session.finished() && std::chrono::steady_clock::now()<deadline) { MSG msg{}; while (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); } session.poll(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
                if (!session.finished() || !session.result().error.empty() || session.result().png.size()<8 || session.result().png.compare(0,8,"\x89PNG\r\n\x1a\n",8)!=0) status=5;
                if (session.running()) session.cancel();
                deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
                while (!session.retainedProfile().empty() && std::chrono::steady_clock::now()<deadline) { MSG msg{}; while (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); } session.poll(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
                if (!session.retainedProfile().empty() && !status) status=6;
            }
            DestroyWindow(window);
        }
    }
    CoUninitialize(); return status;
}
#else
int main() { return 0; }
#endif
