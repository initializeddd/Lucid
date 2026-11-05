#include "platform/browser/GuardedGlfwWebPreview.hpp"
#include <memory>
#include <string>
using namespace lucid::platform;
class FakeBridge final : public GuardedWebPreviewBridge {
public:
    bool start(GLFWwindow*,lucid::modules::WebPreviewAssets,std::string,unsigned,unsigned,bool trusted,std::string& error) override { ++starts; if (!trusted || failStart) { error="failed"; return false; } done=false; value={}; return true; }
    void poll() override { ++polls; }
    void cancel() override { ++cancels; }
    bool finished() const noexcept override { return done; }
    const WindowsWebPreviewResult& result() const noexcept override { return value; }
    const std::filesystem::path& retainedProfile() const noexcept override { return profile; }
    const std::string& cleanupError() const noexcept override { return cleanup; }
    int starts=0,polls=0,cancels=0; bool done=false,failStart=false; WindowsWebPreviewResult value; std::filesystem::path profile="retained-profile"; std::string cleanup="waiting";
};
int main() {
    lucid::modules::WebPreviewAssets assets; std::string error; if (!assets.load({{"/index.html","<p>ok</p>"}},error)) return 1;
    auto impl=std::make_unique<FakeBridge>(); auto* fake=impl.get(); GuardedGlfwWebPreview preview(std::move(impl));
    WebPreviewContext context{"project","file","module",std::string(64,'a'),1,1};
    if (preview.start(nullptr,context,true,false,assets,"/index.html",32,32,error) || error.empty() || fake->starts) return 2;
    if (preview.start(nullptr,context,false,true,assets,"/index.html",32,32,error) || error.empty() || fake->starts) return 3;
    if (!preview.start(nullptr,context,true,true,assets,"/index.html",32,32,error) || fake->starts!=1) return 4;
    fake->done=true; fake->value.png="stale"; auto changed=context; changed.moduleFingerprint="changed";
    const auto stale=preview.poll(changed,true); if (!stale.available || !stale.discarded || !stale.png.empty() || fake->cancels!=1) return 5;
    const auto before=fake->polls; if (preview.poll(changed,true).available || fake->polls!=before+1 || preview.retainedProfile()!=std::filesystem::path("retained-profile") || preview.cleanupError()!="waiting") return 6;
    if (!preview.start(nullptr,context,true,true,assets,"/index.html",32,32,error)) return 7;
    fake->done=true; fake->value.png="captured"; const auto result=preview.poll(context,true);
    if (!result.available || result.discarded || result.png!="captured" || preview.poll(context,true).available) return 8;
    if (!preview.start(nullptr,context,true,true,assets,"/index.html",32,32,error)) return 9;
    fake->done=true; fake->value.png="revoked"; const auto revoked=preview.poll(context,false);
    if (!revoked.available || !revoked.discarded || !revoked.png.empty() || fake->cancels!=2) return 10;
    if (!preview.start(nullptr,context,true,true,assets,"/index.html",32,32,error)) return 11;
    preview.cancel(); fake->done=true; fake->value.png="late"; if (preview.poll(context,true).available || fake->cancels!=3) return 12;
    fake->failStart=true; if (preview.start(nullptr,context,true,true,assets,"/index.html",32,32,error) || error!="failed" || preview.poll(context,true).available) return 13;
    fake->failStart=false; if (!preview.start(nullptr,context,true,true,assets,"/index.html",32,32,error)) return 14;
    fake->done=true; fake->value={"unexpected pixels","browser failed"}; const auto failed=preview.poll(context,true);
    if (!failed.available || failed.error!="browser failed" || !failed.png.empty()) return 15;
    if (!preview.start(nullptr,context,true,true,assets,"/index.html",32,32,error)) return 16;
    fake->done=true; fake->value.png=std::string(16U*1024U*1024U+1,'x'); const auto oversized=preview.poll(context,true);
    if (!oversized.available || oversized.error.empty() || !oversized.png.empty()) return 17;
    return 0;
}
