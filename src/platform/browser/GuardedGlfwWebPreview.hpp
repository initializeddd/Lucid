#pragma once
#include "platform/browser/GlfwWebPreviewBridge.hpp"
#include "platform/browser/WebPreviewContextGate.hpp"
namespace lucid::platform {
struct GuardedWebPreviewEvent { bool available=false, discarded=false; std::string png, error; };
class GuardedWebPreviewBridge {
public:
    virtual ~GuardedWebPreviewBridge()=default;
    virtual bool start(GLFWwindow*,modules::WebPreviewAssets,std::string,unsigned,unsigned,bool,std::string&)=0;
    virtual void poll()=0;
    virtual void cancel()=0;
    [[nodiscard]] virtual bool finished() const noexcept=0;
    [[nodiscard]] virtual const WindowsWebPreviewResult& result() const noexcept=0;
    [[nodiscard]] virtual const std::filesystem::path& retainedProfile() const noexcept=0;
    [[nodiscard]] virtual const std::string& cleanupError() const noexcept=0;
};
// UI-thread integration wrapper. The caller must re-read the active module and
// supply current authorization/context on every poll. Keep polling after cancel
// or completion to allow guarded profile cleanup; surface retainedProfile and
// cleanupError if shutdown cannot be confirmed. Preview runs trusted JavaScript
// with user privileges and the browser is not a network/process sandbox.
class GuardedGlfwWebPreview {
public:
    GuardedGlfwWebPreview();
    // An explicit bridge allows deterministic lifecycle testing without running
    // a browser; production uses the default GLFW/WebView2 bridge.
    explicit GuardedGlfwWebPreview(std::unique_ptr<GuardedWebPreviewBridge> bridge);
    [[nodiscard]] bool start(GLFWwindow* window,const WebPreviewContext& context,bool authorized,bool trusted,
        modules::WebPreviewAssets assets,std::string target,unsigned width,unsigned height,std::string& error);
    [[nodiscard]] GuardedWebPreviewEvent poll(const WebPreviewContext& current,bool authorized);
    void cancel();
    [[nodiscard]] const std::filesystem::path& retainedProfile() const noexcept { return bridge_->retainedProfile(); }
    [[nodiscard]] const std::string& cleanupError() const noexcept { return bridge_->cleanupError(); }
private:
    std::unique_ptr<GuardedWebPreviewBridge> bridge_;
    WebPreviewContextGate gate_;
    std::uint64_t token_=0;
};
}
