#pragma once
#include "platform/browser/WindowsWebPreviewHost.hpp"
#include <memory>
struct GLFWwindow;
namespace lucid::platform {
// A GLFW window must be live, created on the calling Windows UI thread, and
// remain alive until capture ends. The bridge never retains the GLFW pointer;
// the host observes the native HWND and requests cancellation if it disappears.
// Keep calling poll on the same thread after capture/cancel until the private
// profile is removed, or record retainedProfile for manual cleanup. This does
// not enable the feature or sandbox trusted project JavaScript.
class GlfwWebPreviewBridge {
public:
    GlfwWebPreviewBridge(); ~GlfwWebPreviewBridge();
    GlfwWebPreviewBridge(const GlfwWebPreviewBridge&)=delete;
    GlfwWebPreviewBridge& operator=(const GlfwWebPreviewBridge&)=delete;
    [[nodiscard]] bool start(GLFWwindow* window, modules::WebPreviewAssets assets,
        std::string target, unsigned width, unsigned height, bool trusted, std::string& error);
    void poll(); void cancel();
    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] bool finished() const noexcept;
    [[nodiscard]] const WindowsWebPreviewResult& result() const noexcept;
    [[nodiscard]] const std::filesystem::path& retainedProfile() const noexcept;
    [[nodiscard]] const std::string& cleanupError() const noexcept;
private:
    WindowsWebPreviewHost host_;
    [[maybe_unused]] unsigned long creatorThread_ = 0;
};
}
