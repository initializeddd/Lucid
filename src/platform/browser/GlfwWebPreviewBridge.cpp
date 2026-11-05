#include "platform/browser/GlfwWebPreviewBridge.hpp"
#include <utility>
#if defined(_WIN32) && defined(LUCID_ENABLE_WEBVIEW2)
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
namespace lucid::platform {
GlfwWebPreviewBridge::GlfwWebPreviewBridge() {
#if defined(_WIN32) && defined(LUCID_ENABLE_WEBVIEW2)
    creatorThread_=GetCurrentThreadId();
#endif
}
GlfwWebPreviewBridge::~GlfwWebPreviewBridge()=default;
bool GlfwWebPreviewBridge::start(GLFWwindow* window,modules::WebPreviewAssets assets,std::string target,unsigned width,unsigned height,bool trusted,std::string& error) {
    if (!trusted) { error="Web preview requires explicit trust"; return false; }
#if defined(_WIN32) && defined(LUCID_ENABLE_WEBVIEW2)
    if (creatorThread_!=GetCurrentThreadId()) { error="Preview must start on its owning UI thread"; return false; }
    if (!window) { error="Preview window is unavailable"; return false; }
    const auto handle=glfwGetWin32Window(window);
    if (!handle) { error="Cannot access the native preview window"; return false; }
    return host_.start(handle,std::move(assets),std::move(target),width,height,true,error);
#else
    (void)window; (void)assets; (void)target; (void)width; (void)height;
    error="Windows WebView2 preview is not available in this build"; return false;
#endif
}
void GlfwWebPreviewBridge::poll() { host_.poll(); }
void GlfwWebPreviewBridge::cancel() { host_.cancel(); }
bool GlfwWebPreviewBridge::running() const noexcept { return host_.running(); }
bool GlfwWebPreviewBridge::finished() const noexcept { return host_.finished(); }
const WindowsWebPreviewResult& GlfwWebPreviewBridge::result() const noexcept { return host_.result(); }
const std::filesystem::path& GlfwWebPreviewBridge::retainedProfile() const noexcept { return host_.retainedProfile(); }
const std::string& GlfwWebPreviewBridge::cleanupError() const noexcept { return host_.cleanupError(); }
}
