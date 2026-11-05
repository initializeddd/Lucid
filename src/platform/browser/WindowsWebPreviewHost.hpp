#pragma once
#include "platform/browser/WindowsWebPreviewSession.hpp"
#include <memory>
#include <string>
namespace lucid::platform {
// Main-thread bridge for a future Windows UI. The caller passes its live native
// HWND, not a GLFW pointer. Runtime data/profile cleanup requires continued poll
// calls on this same thread after capture or cancellation. Destroying the host
// early may leave a private profile; record retainedProfile() for manual cleanup.
// This does not sandbox trusted project JavaScript or guarantee network isolation.
class WindowsWebPreviewHost {
public:
    WindowsWebPreviewHost(); ~WindowsWebPreviewHost();
    WindowsWebPreviewHost(const WindowsWebPreviewHost&)=delete;
    WindowsWebPreviewHost& operator=(const WindowsWebPreviewHost&)=delete;
    [[nodiscard]] bool start(void* nativeWindow, modules::WebPreviewAssets assets,
        std::string target, unsigned width, unsigned height, bool trusted, std::string& error);
    void poll(); void cancel();
    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] bool finished() const noexcept;
    [[nodiscard]] const WindowsWebPreviewResult& result() const noexcept;
    [[nodiscard]] const std::filesystem::path& retainedProfile() const noexcept;
    [[nodiscard]] const std::string& cleanupError() const noexcept;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
}
