#pragma once
#include "platform/browser/GuardedGlfwWebPreview.hpp"
#include "modules/VisualComparison.hpp"
#include <functional>
#include <future>
#include <memory>
namespace lucid::platform {
struct DecodedWebPreview { modules::VisualImage image; std::string error; };
// Production decoder for the presentation worker. Uses Lucid's bounded PNG
// decoder; a valid image is not evidence of browser isolation or trusted content.
[[nodiscard]] DecodedWebPreview decodeWebPreviewPng(std::string png);
enum class WebPreviewPresentationStatus { None, Ready, Failed, Discarded };
struct WebPreviewPresentationEvent { WebPreviewPresentationStatus status=WebPreviewPresentationStatus::None; modules::VisualImage image; std::string error; };
// Called on the UI thread. PNG decoding is asynchronous and cannot itself be
// interrupted; cancel discards its result. Poll even after completion/cancel so
// the browser profile can finish cleanup. Destruction waits for any decoder.
class WebPreviewPresentation {
public:
    using Decoder=std::function<DecodedWebPreview(std::string)>;
    explicit WebPreviewPresentation(Decoder decoder);
    WebPreviewPresentation(std::unique_ptr<GuardedGlfwWebPreview> bridge,Decoder decoder);
    [[nodiscard]] bool start(GLFWwindow* window,const WebPreviewContext& context,bool authorized,bool trusted,
        modules::WebPreviewAssets assets,std::string target,unsigned width,unsigned height,std::string& error);
    [[nodiscard]] WebPreviewPresentationEvent poll(const WebPreviewContext& current,bool authorized);
    void cancel();
    [[nodiscard]] const std::filesystem::path& retainedProfile() const noexcept { return bridge_->retainedProfile(); }
    [[nodiscard]] const std::string& cleanupError() const noexcept { return bridge_->cleanupError(); }
private:
    std::unique_ptr<GuardedGlfwWebPreview> bridge_;
    Decoder decoder_;
    WebPreviewContextGate gate_;
    std::uint64_t token_=0;
    std::future<DecodedWebPreview> decode_;
};
}
