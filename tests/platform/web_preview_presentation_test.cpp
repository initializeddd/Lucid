#include "platform/browser/WebPreviewPresentation.hpp"
#include <chrono>
#include <future>
#include <memory>
#include <thread>
using namespace lucid::platform;
class FakeBridge final : public GuardedWebPreviewBridge {
public:
    bool start(GLFWwindow*,lucid::modules::WebPreviewAssets,std::string,unsigned,unsigned,bool trusted,std::string&) override { ++starts; done=false; value={}; return trusted; }
    void poll() override { ++polls; }
    void cancel() override { ++cancels; }
    bool finished() const noexcept override { return done; }
    const WindowsWebPreviewResult& result() const noexcept override { return value; }
    const std::filesystem::path& retainedProfile() const noexcept override { return profile; }
    const std::string& cleanupError() const noexcept override { return cleanup; }
    int starts=0,polls=0,cancels=0; bool done=false; WindowsWebPreviewResult value; std::filesystem::path profile="retained"; std::string cleanup="waiting";
};
static DecodedWebPreview image() { DecodedWebPreview result; result.image.width=1; result.image.height=1; result.image.rgba={255,0,0,255}; return result; }
int main() {
    lucid::modules::WebPreviewAssets assets; std::string error; if (!assets.load({{"/index.html","<p>ok</p>"}},error)) return 1;
    const WebPreviewContext context{"project","page","module",std::string(64,'a'),1,2};
    {
        auto fake=std::make_unique<FakeBridge>(); auto* observed=fake.get(); auto bridge=std::make_unique<GuardedGlfwWebPreview>(std::move(fake));
        auto promise=std::make_shared<std::promise<DecodedWebPreview>>(); auto future=promise->get_future().share();
        WebPreviewPresentation presentation(std::move(bridge),[future](std::string) { return future.get(); });
        if (presentation.start(nullptr,context,true,false,assets,"/index.html",32,32,error) || observed->starts) return 2;
        if (!presentation.start(nullptr,context,true,true,assets,"/index.html",32,32,error)) return 3;
        observed->done=true; observed->value.png="png"; if (presentation.poll(context,true).status!=WebPreviewPresentationStatus::None) return 4;
        auto changed=context; ++changed.documentRevision; if (presentation.poll(changed,true).status!=WebPreviewPresentationStatus::Discarded) return 5;
        if (presentation.start(nullptr,context,true,true,assets,"/index.html",32,32,error) || error.find("still finishing")==std::string::npos) return 6;
        promise->set_value(image());
        for (int i=0;i<100;++i) { (void)presentation.poll(changed,true); if (presentation.start(nullptr,context,true,true,assets,"/index.html",32,32,error)) { presentation.cancel(); break; } std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        if (observed->starts!=2 || presentation.retainedProfile()!=std::filesystem::path("retained") || presentation.cleanupError()!="waiting") return 7;
    }
    {
        auto fake=std::make_unique<FakeBridge>(); auto* observed=fake.get(); auto bridge=std::make_unique<GuardedGlfwWebPreview>(std::move(fake));
        WebPreviewPresentation presentation(std::move(bridge),[](std::string) { return image(); });
        if (!presentation.start(nullptr,context,true,true,assets,"/index.html",32,32,error)) return 8;
        observed->done=true; observed->value.png="png"; WebPreviewPresentationEvent event;
        for (int i=0;i<100;++i) { event=presentation.poll(context,true); if (event.status!=WebPreviewPresentationStatus::None) break; std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        if (event.status!=WebPreviewPresentationStatus::Ready || event.image.rgba.size()!=4 || presentation.poll(context,true).status!=WebPreviewPresentationStatus::None) return 9;
    }
    {
        auto fake=std::make_unique<FakeBridge>(); auto* observed=fake.get(); auto bridge=std::make_unique<GuardedGlfwWebPreview>(std::move(fake));
        WebPreviewPresentation presentation(std::move(bridge),[](std::string) { return DecodedWebPreview{}; });
        if (!presentation.start(nullptr,context,true,true,assets,"/index.html",32,32,error)) return 10;
        observed->done=true; observed->value.png="png"; WebPreviewPresentationEvent event;
        for (int i=0;i<100;++i) { event=presentation.poll(context,true); if (event.status!=WebPreviewPresentationStatus::None) break; std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        if (event.status!=WebPreviewPresentationStatus::Failed) return 11;
    }
    return 0;
}
