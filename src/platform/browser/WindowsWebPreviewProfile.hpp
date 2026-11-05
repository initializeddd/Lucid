#pragma once
#include <filesystem>
#include <string>
namespace lucid::platform {
class WindowsWebPreviewProcessWatch;
// Creates a unique owner-private profile directory for exactly one session.
// Cleanup requires a matching browser exit and WebView2's profile-released event.
// Callers must ensure no other session shares the profile, and report retained
// paths on failure. Cleanup refuses reparse points and bounds traversal but cannot
// prevent same-user races.
class WindowsWebPreviewProfile {
public:
    WindowsWebPreviewProfile()=default;
    WindowsWebPreviewProfile(const WindowsWebPreviewProfile&)=delete;
    WindowsWebPreviewProfile& operator=(const WindowsWebPreviewProfile&)=delete;
    [[nodiscard]] bool create(std::string& error);
    [[nodiscard]] bool cleanupAfterBrowserExit(const WindowsWebPreviewProcessWatch& watch, std::string& error);
    // Only for a profile never passed to a browser start operation.
    [[nodiscard]] bool discardUnused(std::string& error);
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
private:
    std::filesystem::path path_;
    bool claimed_ = false;
    [[nodiscard]] bool claim(std::string& error);
    [[nodiscard]] bool cleanup(std::string& error);
    friend class WindowsWebPreview;
};
}
