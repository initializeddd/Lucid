#include "platform/browser/GuardedGlfwWebPreview.hpp"
#include <stdexcept>
#include <utility>
namespace lucid::platform {
namespace {
class RealBridge final : public GuardedWebPreviewBridge {
public:
    bool start(GLFWwindow* w,modules::WebPreviewAssets a,std::string t,unsigned width,unsigned height,bool trusted,std::string& error) override { return bridge_.start(w,std::move(a),std::move(t),width,height,trusted,error); }
    void poll() override { bridge_.poll(); }
    void cancel() override { bridge_.cancel(); }
    bool finished() const noexcept override { return bridge_.finished(); }
    const WindowsWebPreviewResult& result() const noexcept override { return bridge_.result(); }
    const std::filesystem::path& retainedProfile() const noexcept override { return bridge_.retainedProfile(); }
    const std::string& cleanupError() const noexcept override { return bridge_.cleanupError(); }
private: GlfwWebPreviewBridge bridge_;
};
}
GuardedGlfwWebPreview::GuardedGlfwWebPreview():bridge_(std::make_unique<RealBridge>()) {}
GuardedGlfwWebPreview::GuardedGlfwWebPreview(std::unique_ptr<GuardedWebPreviewBridge> bridge):bridge_(std::move(bridge)) { if (!bridge_) throw std::invalid_argument("Preview bridge is required"); }
bool GuardedGlfwWebPreview::start(GLFWwindow* window,const WebPreviewContext& context,bool authorized,bool trusted,modules::WebPreviewAssets assets,std::string target,unsigned width,unsigned height,std::string& error) {
    error.clear(); if (!gate_.begin(context,authorized,trusted,token_)) { error="Preview requires explicit trust and a current authorized context"; return false; }
    if (!bridge_->start(window,std::move(assets),std::move(target),width,height,true,error)) { gate_.cancel(); return false; }
    return true;
}
GuardedWebPreviewEvent GuardedGlfwWebPreview::poll(const WebPreviewContext& current,bool authorized) {
    bridge_->poll(); GuardedWebPreviewEvent event;
    if (!gate_.active()) return event;
    if (!gate_.matches(current,authorized,token_)) { bridge_->cancel(); gate_.cancel(); event.available=true; event.discarded=true; event.error="Preview context or authorization changed"; return event; }
    if (!bridge_->finished()) return event;
    event.available=true; event.error=bridge_->result().error;
    if (event.error.empty()) {
        const auto& png=bridge_->result().png;
        if (png.empty() || png.size()>16U*1024U*1024U) event.error="Preview image is empty or too large";
        else event.png=png;
    }
    gate_.cancel(); return event;
}
void GuardedGlfwWebPreview::cancel() { gate_.cancel(); bridge_->cancel(); }
}
