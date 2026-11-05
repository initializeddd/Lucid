#include "platform/browser/GlfwWebPreviewBridge.hpp"
#if defined(_WIN32)
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <chrono>
#include <string>
#include <thread>
int main() {
    wchar_t enabled[4]{}; if (!GetEnvironmentVariableW(L"LUCID_WEBVIEW2_SMOKE",enabled,4) || std::wstring(enabled)!=L"1") return 0;
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CLIENT_API,GLFW_NO_API);
    GLFWwindow* window=glfwCreateWindow(400,300,"Lucid WebView2 bridge test",nullptr,nullptr); if (!window) { glfwTerminate(); return 2; }
    int status=0;
    {
        lucid::modules::WebPreviewAssets assets; std::string error; std::vector<lucid::modules::WebPreviewAsset> files;
        files.push_back({"/index.html","<!doctype html><html><body style=\"background:red\">preview</body></html>"});
        lucid::platform::GlfwWebPreviewBridge bridge;
        if (!assets.load(std::move(files),error) || !bridge.start(window,std::move(assets),"/index.html",256,128,true,error)) status=3;
        else {
            auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(25);
            while (!bridge.finished() && std::chrono::steady_clock::now()<deadline) { glfwPollEvents(); bridge.poll(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (!bridge.finished() || !bridge.result().error.empty() || bridge.result().png.size()<8 || bridge.result().png.compare(0,8,"\x89PNG\r\n\x1a\n",8)!=0) status=4;
            if (bridge.running()) bridge.cancel();
            deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
            while (!bridge.retainedProfile().empty() && std::chrono::steady_clock::now()<deadline) { glfwPollEvents(); bridge.poll(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (!bridge.retainedProfile().empty() && !status) status=5;
        }
    }
    glfwDestroyWindow(window); glfwTerminate(); return status;
}
#else
int main() { return 0; }
#endif
