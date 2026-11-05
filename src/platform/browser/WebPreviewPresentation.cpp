#include "platform/browser/WebPreviewPresentation.hpp"
#include <chrono>
#include <stdexcept>
#include <utility>
namespace lucid::platform {
WebPreviewPresentation::WebPreviewPresentation(Decoder decoder):WebPreviewPresentation(std::make_unique<GuardedGlfwWebPreview>(),std::move(decoder)) {}
WebPreviewPresentation::WebPreviewPresentation(std::unique_ptr<GuardedGlfwWebPreview> bridge,Decoder decoder):bridge_(std::move(bridge)),decoder_(std::move(decoder)) {
    if (!bridge_ || !decoder_) throw std::invalid_argument("Preview bridge and decoder are required");
}
bool WebPreviewPresentation::start(GLFWwindow* window,const WebPreviewContext& context,bool authorized,bool trusted,modules::WebPreviewAssets assets,std::string target,unsigned width,unsigned height,std::string& error) {
    error.clear(); if (decode_.valid()) { error="Previous preview decoding is still finishing"; return false; }
    if (!gate_.begin(context,authorized,trusted,token_)) { error="Preview requires explicit trust and a current authorized context"; return false; }
    if (!bridge_->start(window,context,authorized,trusted,std::move(assets),std::move(target),width,height,error)) { gate_.cancel(); return false; }
    return true;
}
WebPreviewPresentationEvent WebPreviewPresentation::poll(const WebPreviewContext& current,bool authorized) {
    auto captured=bridge_->poll(current,authorized);
    if (!gate_.active()) { if (decode_.valid() && decode_.wait_for(std::chrono::seconds(0))==std::future_status::ready) { try { (void)decode_.get(); } catch (...) {} } return {}; }
    if (!gate_.matches(current,authorized,token_)) { bridge_->cancel(); gate_.cancel(); return {WebPreviewPresentationStatus::Discarded,{},"Preview context or authorization changed"}; }
    if (captured.available) {
        if (captured.discarded) { gate_.cancel(); return {WebPreviewPresentationStatus::Discarded,{},captured.error}; }
        if (!captured.error.empty()) { gate_.cancel(); return {WebPreviewPresentationStatus::Failed,{},captured.error}; }
        if (captured.png.empty() || captured.png.size()>16U*1024U*1024U) { gate_.cancel(); return {WebPreviewPresentationStatus::Failed,{},"Preview image is empty or too large"}; }
        try { decode_=std::async(std::launch::async,decoder_,std::move(captured.png)); }
        catch (const std::exception&) { gate_.cancel(); return {WebPreviewPresentationStatus::Failed,{},"Cannot start preview decoding"}; }
    }
    if (!decode_.valid() || decode_.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return {};
    DecodedWebPreview decoded; try { decoded=decode_.get(); } catch (...) { decoded.error="Preview decoding failed"; }
    gate_.cancel();
    if (!decoded.error.empty()) return {WebPreviewPresentationStatus::Failed,{},std::move(decoded.error)};
    const auto width=decoded.image.width,height=decoded.image.height;
    if (!width || !height || width>4096 || height>4096 || width>16U*1024U*1024U/height || decoded.image.rgba.size()!=width*height*4) return {WebPreviewPresentationStatus::Failed,{},"Preview decoder returned invalid pixels"};
    return {WebPreviewPresentationStatus::Ready,std::move(decoded.image),{}};
}
void WebPreviewPresentation::cancel() { gate_.cancel(); bridge_->cancel(); }
}
