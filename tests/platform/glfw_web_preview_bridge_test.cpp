#include "platform/browser/GlfwWebPreviewBridge.hpp"
#include <string>
#include <thread>
#if defined(_WIN32) && defined(LUCID_GLFW_TEST_STUB)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
extern "C" HWND glfwGetWin32Window(GLFWwindow*) { return nullptr; }
#endif
int main() {
    lucid::modules::WebPreviewAssets assets; std::string error;
    if (!assets.load({{"/index.html","<p>test</p>"}},error)) return 1;
    lucid::platform::GlfwWebPreviewBridge bridge;
    if (bridge.start(nullptr,assets,"/index.html",32,32,false,error) || error.find("trust")==std::string::npos || bridge.running() || !bridge.retainedProfile().empty()) return 2;
    if (bridge.start(nullptr,assets,"/index.html",32,32,true,error) || error.empty() || bridge.running() || !bridge.retainedProfile().empty()) return 3;
#if defined(_WIN32) && defined(LUCID_GLFW_TEST_STUB)
    bool rejected=false;
    std::thread worker([&] { std::string message; rejected=!bridge.start(reinterpret_cast<GLFWwindow*>(1),assets,"/index.html",32,32,true,message) && message.find("owning UI thread")!=std::string::npos; });
    worker.join(); if (!rejected) return 5;
#endif
    bridge.poll(); bridge.cancel(); if (bridge.finished() || !bridge.result().png.empty()) return 4;
    return 0;
}
