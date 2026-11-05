#include "platform/browser/WindowsWebPreviewSession.hpp"
#include "platform/browser/WindowsWebPreviewProfile.hpp"
#include "platform/browser/WindowsWebPreviewRouter.hpp"
#include <optional>
#include <utility>
namespace lucid::platform {
struct WindowsWebPreviewSession::State {
    WindowsWebPreview preview;
    WindowsWebPreviewProfile profile;
    std::optional<WindowsWebPreviewProcessWatch> watch;
    WindowsWebPreviewResult result;
    std::string cleanupError;
    bool running=false, finished=false;
};
WindowsWebPreviewSession::WindowsWebPreviewSession():state_(std::make_shared<State>()) {}
WindowsWebPreviewSession::~WindowsWebPreviewSession() { cancel(); }
bool WindowsWebPreviewSession::start(void* hwnd,const std::filesystem::path& loader,modules::WebPreviewAssets assets,std::string target,unsigned width,unsigned height,bool trusted,std::string& error) {
    error.clear();
    if (!trusted) { error="Web preview requires explicit trust"; return false; }
    if (!hwnd || !loader.is_absolute() || !width || !height || width>2048 || height>2048 || target.size()>4096) { error="Preview window, trusted loader and valid dimensions are required"; return false; }
    if (state_->running || !state_->profile.path().empty()) { error="Previous preview profile has not been released; retain it for cleanup"; return false; }
    if (!allowWindowsWebPreviewNavigation(assets,"https://lucid-preview.invalid"+target)) { error="Preview target is unavailable"; return false; }
    std::string version; if (!probeWindowsWebView2Runtime(loader,version,error)) return false;
    if (!state_->profile.create(error)) return false;
    state_->result={}; state_->cleanupError.clear(); state_->finished=false; state_->running=true;
    std::weak_ptr<State> weak=state_;
    const bool started=state_->preview.start(hwnd,loader,state_->profile,std::move(assets),std::move(target),width,height,true,[weak](WindowsWebPreviewResult result) {
        if (const auto state=weak.lock()) { state->result=std::move(result); state->running=false; state->finished=true; state->cleanupError="Preview profile retained until WebView2 shutdown is confirmed"; }
    },error);
    state_->watch=state_->preview.processWatch();
    if (!started) {
        state_->running=false; state_->finished=true; state_->result.error=error;
        std::string cleanupError; if (!state_->profile.discardUnused(cleanupError)) state_->cleanupError=std::move(cleanupError);
        return false;
    }
    return true;
}
void WindowsWebPreviewSession::poll() {
    state_->preview.poll();
    if (!state_->running && !state_->profile.path().empty() && state_->watch && state_->watch->observed() && state_->watch->exited() && state_->watch->profileReleased()) {
        std::string error; if (!state_->profile.cleanupAfterBrowserExit(*state_->watch,error)) state_->cleanupError=std::move(error); else state_->cleanupError.clear();
    }
}
void WindowsWebPreviewSession::cancel() {
    if (!state_->running) return;
    state_->preview.cancel(); state_->running=false; state_->finished=true; state_->result={{},"Web preview cancelled"};
    if (!state_->profile.path().empty()) state_->cleanupError="Preview profile retained until WebView2 shutdown is confirmed";
}
bool WindowsWebPreviewSession::running() const noexcept { return state_->running; }
bool WindowsWebPreviewSession::finished() const noexcept { return state_->finished; }
const WindowsWebPreviewResult& WindowsWebPreviewSession::result() const noexcept { return state_->result; }
const std::filesystem::path& WindowsWebPreviewSession::retainedProfile() const noexcept { return state_->profile.path(); }
const std::string& WindowsWebPreviewSession::cleanupError() const noexcept { return state_->cleanupError; }
}
