#pragma once
#include "modules/WebPreviewAssets.hpp"
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
namespace lucid::platform {
class WindowsWebPreviewProfile;
struct WindowsWebPreviewResult { std::string png, error; };
struct WindowsWebPreviewProcessState;
// Tracks an observed browser by handle and the WebView2 runtime's distinct
// BrowserProcessExited event. Neither is available if startup was cancelled
// before a browser could be observed.
class WindowsWebPreviewProcessWatch {
public:
    [[nodiscard]] bool observed() const noexcept;
    [[nodiscard]] bool exited() const noexcept;
    [[nodiscard]] bool profileReleased() const noexcept;
private:
    explicit WindowsWebPreviewProcessWatch(std::shared_ptr<WindowsWebPreviewProcessState> state):state_(std::move(state)) {}
    [[nodiscard]] bool matchesProfile(const std::filesystem::path& path) const noexcept;
    std::shared_ptr<WindowsWebPreviewProcessState> state_;
    friend class WindowsWebPreview;
    friend class WindowsWebPreviewProfile;
};
// Loads only the explicitly supplied trusted loader and checks for an installed
// WebView2 runtime. A successful probe is not a rendering or sandbox guarantee.
[[nodiscard]] bool probeWindowsWebView2Runtime(const std::filesystem::path& loader,
                                               std::string& version, std::string& error);
// Opt-in WebView2 snapshot. Methods, poll and callbacks must run on the owning
// Windows STA UI thread with a live HWND and message loop. JavaScript executes
// with user privileges; routing/CSP are defense in depth, not a sandbox. The
// caller must explicitly trust the project, provide a trusted packaged loader
// and a private per-session WebView2 user-data directory, and remove that profile
// after WebView2 processes release it. The controller may briefly overlay HWND.
class WindowsWebPreview {
public:
    using Completion=std::function<void(WindowsWebPreviewResult)>;
    WindowsWebPreview(); ~WindowsWebPreview();
    WindowsWebPreview(const WindowsWebPreview&)=delete;
    WindowsWebPreview& operator=(const WindowsWebPreview&)=delete;
    [[nodiscard]] bool start(void* parent, const std::filesystem::path& loader,
        WindowsWebPreviewProfile& privateProfile, modules::WebPreviewAssets assets,
        std::string target, unsigned width, unsigned height, bool trusted,
        Completion completion, std::string& error);
    void poll(); void cancel();
    // Retain this watch before starting a new session; cancellation before a
    // browser is observed may never produce release evidence, so retain that
    // profile for manual cleanup rather than assuming it is unused.
    [[nodiscard]] WindowsWebPreviewProcessWatch processWatch() const;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
}
