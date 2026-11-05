#pragma once
#include "platform/browser/WindowsWebPreview.hpp"
#include <memory>
#include <string>
namespace lucid::platform {
// Poll-driven owner for one trusted Windows WebView2 snapshot session. Must be
// used only on the owning STA UI thread. No browser is started without explicit
// trust; scripts execute with user privileges and this is not a sandbox.
// A session profile is removed only after the matching browser handle exits and
// WebView2 reports BrowserProcessExited. Unknown shutdowns retain the profile.
class WindowsWebPreviewSession {
public:
    WindowsWebPreviewSession(); ~WindowsWebPreviewSession();
    WindowsWebPreviewSession(const WindowsWebPreviewSession&)=delete;
    WindowsWebPreviewSession& operator=(const WindowsWebPreviewSession&)=delete;
    [[nodiscard]] bool start(void* hwnd, const std::filesystem::path& trustedLoader,
        modules::WebPreviewAssets assets, std::string target, unsigned width,
        unsigned height, bool trusted, std::string& error);
    void poll();
    void cancel();
    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] bool finished() const noexcept;
    [[nodiscard]] const WindowsWebPreviewResult& result() const noexcept;
    [[nodiscard]] const std::filesystem::path& retainedProfile() const noexcept;
    [[nodiscard]] const std::string& cleanupError() const noexcept;
private:
    struct State; std::shared_ptr<State> state_;
};
}
