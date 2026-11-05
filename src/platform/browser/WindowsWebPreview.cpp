#include "platform/browser/WindowsWebPreview.hpp"
#include "platform/browser/WindowsWebPreviewProfile.hpp"
#if defined(_WIN32) && defined(LUCID_ENABLE_WEBVIEW2)
#include "platform/browser/WindowsWebPreviewRouter.hpp"
#include "platform/browser/WebPreviewCaptureGate.hpp"
#include "platform/browser/WebView2LoaderPolicy.hpp"
#include <windows.h>
#include <objidl.h>
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#endif
#include <WebView2.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <wrl/client.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <map>
#include <utility>
namespace lucid::platform {
struct WindowsWebPreviewProcessState {
    std::filesystem::path profile;
    HANDLE handle=nullptr;
    UINT32 pid=0;
    std::atomic_bool released{false};
    Microsoft::WRL::ComPtr<ICoreWebView2Environment5> environment;
    EventRegistrationToken exitToken{};
    bool subscribed=false;
    ~WindowsWebPreviewProcessState() { if (subscribed && environment) environment->remove_BrowserProcessExited(exitToken); if (handle) CloseHandle(handle); }
};
bool WindowsWebPreviewProcessWatch::observed() const noexcept { return state_ && state_->handle; }
bool WindowsWebPreviewProcessWatch::exited() const noexcept { return observed() && WaitForSingleObject(state_->handle,0)==WAIT_OBJECT_0; }
bool WindowsWebPreviewProcessWatch::profileReleased() const noexcept { return state_ && state_->released.load(); }
bool WindowsWebPreviewProcessWatch::matchesProfile(const std::filesystem::path& path) const noexcept { return state_ && state_->profile==path; }
namespace {
using Microsoft::WRL::ComPtr;
template<class Interface,class... Args> class Handler final : public Interface {
public:
    using Fn=std::function<HRESULT(Args...)>;
    Handler(const IID& iid,Fn fn):iid_(iid),fn_(std::move(fn)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out) override { if (!out) return E_POINTER; *out=nullptr; if (IsEqualIID(iid,IID_IUnknown) || IsEqualIID(iid,iid_)) { *out=static_cast<Interface*>(this); AddRef(); return S_OK; } return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override { const auto n=--refs_; if (!n) delete this; return n; }
    HRESULT STDMETHODCALLTYPE Invoke(Args... args) override { try { return fn_(args...); } catch (...) { return E_FAIL; } }
private: std::atomic<ULONG> refs_{1}; IID iid_; Fn fn_;
};
template<class I,class... A,class F> ComPtr<I> handler(const IID& iid,F fn) { ComPtr<I> out; out.Attach(new Handler<I,A...>(iid,typename Handler<I,A...>::Fn(std::move(fn)))); return out; }
std::string utf8(const wchar_t* text) { if (!text) return {}; const auto n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,nullptr,0,nullptr,nullptr); if (n<=0) return {}; std::string out(static_cast<std::size_t>(n),'\0'); if (!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,out.data(),n,nullptr,nullptr)) return {}; out.pop_back(); return out; }
std::wstring wide(std::string_view text) { if (text.empty()) return {}; const auto n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0); if (n<=0) return {}; std::wstring out(static_cast<std::size_t>(n),L'\0'); if (!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),out.data(),n)) return {}; return out; }
HMODULE trustedLoader(const std::filesystem::path& path) {
    // All calls occur on the owning UI thread. Retain one loader reference per
    // path until process exit because cancelled WebView2 callbacks can arrive late.
    static std::map<std::filesystem::path,HMODULE> loaded;
    if (const auto it=loaded.find(path); it!=loaded.end()) return it->second;
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    if (file==INVALID_HANDLE_VALUE) return nullptr;
    BY_HANDLE_FILE_INFORMATION info{}; LARGE_INTEGER size{};
    if (GetFileType(file)!=FILE_TYPE_DISK || !GetFileInformationByHandle(file,&info) || (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) || !GetFileSizeEx(file,&size) || size.QuadPart<=0 || size.QuadPart>1024*1024) { CloseHandle(file); return nullptr; }
    std::string bytes(static_cast<std::size_t>(size.QuadPart),'\0'); DWORD actual=0;
    if (!ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&actual,nullptr) || actual!=bytes.size()) { CloseHandle(file); return nullptr; }
#if defined(_M_ARM64) || defined(__aarch64__)
    constexpr auto arch=WebView2LoaderArch::Arm64;
#elif defined(_WIN64)
    constexpr auto arch=WebView2LoaderArch::X64;
#else
    constexpr auto arch=WebView2LoaderArch::X86;
#endif
    if (!isPinnedWebView2Loader(bytes,arch)) { CloseHandle(file); return nullptr; }
    // Holding the read-only handle denies ordinary writes/deletes during load.
    // A separately changed parent path can still race; same-user code is trusted.
    HMODULE module=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    CloseHandle(file);
    if (module) loaded.emplace(path,module);
    return module;
}
struct Session : std::enable_shared_from_this<Session> {
    modules::WebPreviewAssets assets; WindowsWebPreview::Completion completion; ComPtr<ICoreWebView2Environment> environment; ComPtr<ICoreWebView2Controller> controller; ComPtr<ICoreWebView2> view;
    WebPreviewCaptureGate captureGate;
    std::shared_ptr<WindowsWebPreviewProcessState> process;
    HWND parent=nullptr; unsigned width=0,height=0; std::wstring url; bool finished=false; std::chrono::steady_clock::time_point deadline;
    void stop() { if (finished) return; finished=true; captureGate.cancel(); if (view) view->Stop(); if (controller) controller->Close(); view.Reset(); controller.Reset(); environment.Reset(); }
    void finish(WindowsWebPreviewResult result) { if (finished) return; auto callback=std::move(completion); stop(); if (callback) callback(std::move(result)); }
    void capture(std::uint64_t id) {
        if (finished || !view || !captureGate.isCurrentCapture(id)) return;
        ComPtr<IStream> stream; if (FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&stream))) { finish({{},"Cannot create preview image stream"}); return; }
        std::weak_ptr<Session> weak=shared_from_this();
        auto captured=handler<ICoreWebView2CapturePreviewCompletedHandler,HRESULT>(IID_ICoreWebView2CapturePreviewCompletedHandler,[weak,stream,id](HRESULT result) -> HRESULT {
            auto active=weak.lock(); if (!active || active->finished) return E_ABORT; if (!active->captureGate.isCurrentCapture(id)) return S_OK; STATSTG stat{};
            if (FAILED(result) || FAILED(stream->Stat(&stat,STATFLAG_NONAME)) || stat.cbSize.QuadPart==0 || stat.cbSize.QuadPart>16U*1024U*1024U) { active->finish({{},"Cannot capture web preview"}); return S_OK; }
            std::string png(static_cast<std::size_t>(stat.cbSize.QuadPart),'\0'); LARGE_INTEGER zero{}; ULONG read=0;
            if (FAILED(stream->Seek(zero,STREAM_SEEK_SET,nullptr)) || FAILED(stream->Read(png.data(),static_cast<ULONG>(png.size()),&read)) || read!=png.size()) active->finish({{},"Cannot read web preview image"}); else active->finish({std::move(png),{}}); return S_OK;
        });
        if (FAILED(view->CapturePreview(COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG,stream.Get(),captured.Get()))) finish({{},"Cannot start web preview capture"});
    }
    bool install() {
        std::weak_ptr<Session> weak=shared_from_this(); EventRegistrationToken token{};
        ComPtr<ICoreWebView2Settings> settings;
        if (FAILED(view->get_Settings(&settings)) || !settings || FAILED(settings->put_IsWebMessageEnabled(FALSE)) ||
            FAILED(settings->put_AreDefaultScriptDialogsEnabled(FALSE)) || FAILED(settings->put_IsStatusBarEnabled(FALSE)) ||
            FAILED(settings->put_AreDevToolsEnabled(FALSE)) || FAILED(settings->put_AreDefaultContextMenusEnabled(FALSE)) ||
            FAILED(settings->put_AreHostObjectsAllowed(FALSE))) return false;
        auto resource=handler<ICoreWebView2WebResourceRequestedEventHandler,ICoreWebView2*,ICoreWebView2WebResourceRequestedEventArgs*>(IID_ICoreWebView2WebResourceRequestedEventHandler,[weak](auto*,auto* args) -> HRESULT {
            auto s=weak.lock(); if (!s || s->finished) return E_ABORT;
            const auto fail=[&] { s->finish({{},"Cannot safely route web preview request"}); return E_ABORT; };
            ComPtr<ICoreWebView2WebResourceRequest> request; if (FAILED(args->get_Request(&request)) || !request) return fail();
            LPWSTR uri=nullptr,method=nullptr; if (FAILED(request->get_Uri(&uri)) || FAILED(request->get_Method(&method))) { CoTaskMemFree(uri); CoTaskMemFree(method); return fail(); }
            auto route=routeWindowsWebPreviewRequest(s->assets,utf8(method),utf8(uri)); CoTaskMemFree(uri); CoTaskMemFree(method);
            if (route.route==WindowsPreviewRoute::Blocked) { route.response.status=403; route.response.body.clear(); route.response.headers={{"Cache-Control","no-store"},{"Content-Security-Policy","default-src 'none'"}}; }
            ComPtr<IStream> body; if (FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&body))) return fail();
            if (!route.response.body.empty()) { ULONG written=0; if (FAILED(body->Write(route.response.body.data(),static_cast<ULONG>(route.response.body.size()),&written)) || written!=route.response.body.size()) return fail(); }
            LARGE_INTEGER zero{}; if (FAILED(body->Seek(zero,STREAM_SEEK_SET,nullptr))) return fail();
            std::string headers; for (const auto& [key,value]:route.response.headers) headers+=key+": "+value+"\r\n"; if (!route.response.contentType.empty()) headers+="Content-Type: "+route.response.contentType+"\r\n";
            const auto wheaders=wide(headers); ComPtr<ICoreWebView2WebResourceResponse> response;
            if (FAILED(s->environment->CreateWebResourceResponse(body.Get(),route.response.status,route.response.status==200?L"OK":L"Blocked",wheaders.c_str(),&response)) || !response || FAILED(args->put_Response(response.Get()))) return fail();
            return S_OK;
        });
        if (FAILED(view->AddWebResourceRequestedFilter(L"*",COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL)) || FAILED(view->add_WebResourceRequested(resource.Get(),&token))) return false;
        auto nav=handler<ICoreWebView2NavigationStartingEventHandler,ICoreWebView2*,ICoreWebView2NavigationStartingEventArgs*>(IID_ICoreWebView2NavigationStartingEventHandler,[weak](auto*,auto* args) -> HRESULT { auto s=weak.lock(); if (!s || s->finished) return E_ABORT; LPWSTR uri=nullptr; if (FAILED(args->get_Uri(&uri))) return E_FAIL; const bool blocked=!allowWindowsWebPreviewNavigation(s->assets,utf8(uri)); CoTaskMemFree(uri); UINT64 id=0; if (!blocked) { if (FAILED(args->get_NavigationId(&id))) return E_FAIL; s->captureGate.navigationStarted(id); } return args->put_Cancel(blocked); });
        if (FAILED(view->add_NavigationStarting(nav.Get(),&token))) return false;
        auto popup=handler<ICoreWebView2NewWindowRequestedEventHandler,ICoreWebView2*,ICoreWebView2NewWindowRequestedEventArgs*>(IID_ICoreWebView2NewWindowRequestedEventHandler,[](auto*,auto* args) -> HRESULT { return args->put_Handled(TRUE); });
        if (FAILED(view->add_NewWindowRequested(popup.Get(),&token))) return false;
        auto permission=handler<ICoreWebView2PermissionRequestedEventHandler,ICoreWebView2*,ICoreWebView2PermissionRequestedEventArgs*>(IID_ICoreWebView2PermissionRequestedEventHandler,[](auto*,auto* args) -> HRESULT { return args->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY); });
        if (FAILED(view->add_PermissionRequested(permission.Get(),&token))) return false;
        auto failed=handler<ICoreWebView2ProcessFailedEventHandler,ICoreWebView2*,ICoreWebView2ProcessFailedEventArgs*>(IID_ICoreWebView2ProcessFailedEventHandler,[weak](auto*,auto*) -> HRESULT {
            auto s=weak.lock(); if (!s || s->finished) return E_ABORT;
            s->finish({{},"WebView2 process failed; preview stopped"}); return S_OK;
        });
        if (FAILED(view->add_ProcessFailed(failed.Get(),&token))) return false;
        ComPtr<ICoreWebView2_4> downloadView;
        if (FAILED(view->QueryInterface(IID_ICoreWebView2_4,reinterpret_cast<void**>(downloadView.GetAddressOf()))) || !downloadView) return false;
        auto download=handler<ICoreWebView2DownloadStartingEventHandler,ICoreWebView2*,ICoreWebView2DownloadStartingEventArgs*>(IID_ICoreWebView2DownloadStartingEventHandler,[weak](auto*,auto* args) -> HRESULT {
            auto s=weak.lock(); if (!s || s->finished) return E_ABORT;
            if (!args || FAILED(args->put_Cancel(TRUE)) || FAILED(args->put_Handled(TRUE))) { s->finish({{},"Cannot safely block web preview download"}); return E_ABORT; }
            return S_OK;
        });
        if (FAILED(downloadView->add_DownloadStarting(download.Get(),&token))) return false;
        auto complete=handler<ICoreWebView2NavigationCompletedEventHandler,ICoreWebView2*,ICoreWebView2NavigationCompletedEventArgs*>(IID_ICoreWebView2NavigationCompletedEventHandler,[weak](auto*,auto* args) -> HRESULT {
            auto s=weak.lock(); if (!s || s->finished) return E_ABORT; UINT64 id=0; if (FAILED(args->get_NavigationId(&id)) || !s->captureGate.isCurrentNavigation(id)) return S_OK;
            BOOL ok=FALSE; if (FAILED(args->get_IsSuccess(&ok)) || !ok) { s->finish({{},"Web preview navigation failed"}); return S_OK; }
            s->captureGate.navigationCompleted(id,WebPreviewCaptureGate::Clock::now());
            return S_OK;
        });
        return SUCCEEDED(view->add_NavigationCompleted(complete.Get(),&token));
    }
};
}
struct WindowsWebPreview::Impl { std::shared_ptr<Session> session; std::shared_ptr<WindowsWebPreviewProcessState> process; };
WindowsWebPreview::WindowsWebPreview():impl_(std::make_unique<Impl>()) {}
WindowsWebPreview::~WindowsWebPreview() { cancel(); }
void WindowsWebPreview::cancel() { if (impl_->session) { impl_->session->completion={}; impl_->session->stop(); impl_->session.reset(); } }
void WindowsWebPreview::poll() {
    const auto session=impl_->session; if (!session || session->finished) return;
    const auto now=std::chrono::steady_clock::now();
    if (now>=session->deadline) { session->finish({{},"Web preview timed out"}); return; }
    if (const auto id=session->captureGate.takeReady(now)) session->capture(*id);
}
WindowsWebPreviewProcessWatch WindowsWebPreview::processWatch() const { return WindowsWebPreviewProcessWatch(impl_->process); }
bool probeWindowsWebView2Runtime(const std::filesystem::path& loader,std::string& version,std::string& error) {
    version.clear(); error.clear(); if (!loader.is_absolute()) { error="A trusted absolute WebView2 loader path is required"; return false; }
    const auto library=trustedLoader(loader); if (!library) { error="WebView2 loader is unavailable"; return false; }
    using Probe=HRESULT (STDAPICALLTYPE*)(PCWSTR,LPWSTR*); const auto address=GetProcAddress(library,"GetAvailableCoreWebView2BrowserVersionString"); Probe probe=nullptr;
    static_assert(sizeof(probe)==sizeof(address)); std::memcpy(&probe,&address,sizeof(probe)); if (!probe) { error="WebView2 loader is incompatible"; return false; }
    LPWSTR value=nullptr; const auto result=probe(nullptr,&value); if (FAILED(result) || !value) { CoTaskMemFree(value); error="WebView2 runtime is unavailable"; return false; }
    version=utf8(value); CoTaskMemFree(value); if (version.empty() || version.size()>256) { version.clear(); error="WebView2 runtime version is invalid"; return false; }
    return true;
}
bool WindowsWebPreview::start(void* parent,const std::filesystem::path& loader,WindowsWebPreviewProfile& privateProfile,modules::WebPreviewAssets assets,std::string target,unsigned width,unsigned height,bool trusted,Completion completion,std::string& error) {
    const auto profile=privateProfile.path();
    error.clear(); if (!trusted || !parent || !loader.is_absolute() || !profile.is_absolute() || !width || !height || width>2048 || height>2048 || target.size()>4096 || !completion) { error="Trusted preview, parent window, loader and private profile are required"; return false; }
    if (!allowWindowsWebPreviewNavigation(assets,"https://lucid-preview.invalid"+target)) { error="Preview target is unavailable"; return false; }
    cancel(); HMODULE library=trustedLoader(loader); if (!library) { error="WebView2 loader is unavailable"; return false; }
    using Create=HRESULT (STDAPICALLTYPE*)(PCWSTR,PCWSTR,ICoreWebView2EnvironmentOptions*,ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);
    const auto address=GetProcAddress(library,"CreateCoreWebView2EnvironmentWithOptions"); Create create=nullptr;
    static_assert(sizeof(create)==sizeof(address)); std::memcpy(&create,&address,sizeof(create));
    if (!create) { error="WebView2 loader is incompatible"; return false; }
    if (!privateProfile.claim(error)) return false;
    // Keep the trusted loader loaded for process lifetime: WebView2 initialization
    // and COM callbacks can outlive cancellation and must not jump into unloaded code.
    auto session=std::make_shared<Session>(); session->process=std::make_shared<WindowsWebPreviewProcessState>(); session->process->profile=profile; impl_->process=session->process; session->assets=std::move(assets); session->completion=std::move(completion); session->parent=static_cast<HWND>(parent); session->width=width; session->height=height; session->url=wide("https://lucid-preview.invalid"+target); session->deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15); impl_->session=session;
    std::weak_ptr<Session> weak=session;
    auto environment=handler<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,HRESULT,ICoreWebView2Environment*>(IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,[weak](HRESULT result,ICoreWebView2Environment* env) -> HRESULT {
        auto s=weak.lock(); if (!s || s->finished) return E_ABORT; if (FAILED(result) || !env) { s->finish({{},"WebView2 runtime is unavailable"}); return S_OK; } s->environment=env;
        if (FAILED(env->QueryInterface(IID_ICoreWebView2Environment5,reinterpret_cast<void**>(s->process->environment.GetAddressOf()))) || !s->process->environment) { s->finish({{},"WebView2 runtime cannot report safe profile release"}); return S_OK; }
        std::weak_ptr<WindowsWebPreviewProcessState> process=s->process;
        auto exited=handler<ICoreWebView2BrowserProcessExitedEventHandler,ICoreWebView2Environment*,ICoreWebView2BrowserProcessExitedEventArgs*>(IID_ICoreWebView2BrowserProcessExitedEventHandler,[process](auto*,auto* args) -> HRESULT {
            auto state=process.lock(); if (!state) return E_ABORT; UINT32 pid=0;
            if (args && SUCCEEDED(args->get_BrowserProcessId(&pid)) && pid && pid==state->pid) state->released.store(true);
            return S_OK;
        });
        if (FAILED(s->process->environment->add_BrowserProcessExited(exited.Get(),&s->process->exitToken))) { s->finish({{},"Cannot monitor WebView2 profile release"}); return S_OK; }
        s->process->subscribed=true;
        auto controller=handler<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,HRESULT,ICoreWebView2Controller*>(IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,[weak](HRESULT status,ICoreWebView2Controller* c) -> HRESULT {
            auto active=weak.lock(); if (!active || active->finished) return E_ABORT; if (FAILED(status) || !c) { active->finish({{},"Cannot create WebView2 controller"}); return S_OK; }
            active->controller=c; if (FAILED(c->get_CoreWebView2(&active->view))) { active->finish({{},"Cannot initialize WebView2"}); return S_OK; }
            UINT32 pid=0; if (SUCCEEDED(active->view->get_BrowserProcessId(&pid)) && pid) { active->process->pid=pid; active->process->handle=OpenProcess(SYNCHRONIZE,FALSE,pid); }
            RECT bounds{0,0,static_cast<LONG>(active->width),static_cast<LONG>(active->height)}; if (FAILED(c->put_Bounds(bounds)) || FAILED(c->put_IsVisible(TRUE)) || !active->install() || FAILED(active->view->Navigate(active->url.c_str()))) active->finish({{},"Cannot start WebView2 snapshot"}); return S_OK;
        });
        if (FAILED(s->environment->CreateCoreWebView2Controller(s->parent,controller.Get()))) s->finish({{},"Cannot start WebView2 controller"});
        return S_OK;
    });
    if (FAILED(create(nullptr,profile.c_str(),nullptr,environment.Get()))) { cancel(); error="Cannot start WebView2 runtime"; return false; }
    return true;
}
}
#else
namespace lucid::platform {
struct WindowsWebPreviewProcessState {};
bool WindowsWebPreviewProcessWatch::observed() const noexcept { return false; }
bool WindowsWebPreviewProcessWatch::exited() const noexcept { return false; }
bool WindowsWebPreviewProcessWatch::profileReleased() const noexcept { return false; }
bool WindowsWebPreviewProcessWatch::matchesProfile(const std::filesystem::path&) const noexcept { return false; }
bool probeWindowsWebView2Runtime(const std::filesystem::path&,std::string& version,std::string& error) { version.clear(); error="WebView2 preview is not available in this build"; return false; }
struct WindowsWebPreview::Impl {};
WindowsWebPreview::WindowsWebPreview():impl_(std::make_unique<Impl>()) {}
WindowsWebPreview::~WindowsWebPreview()=default;
void WindowsWebPreview::cancel() {}
void WindowsWebPreview::poll() {}
WindowsWebPreviewProcessWatch WindowsWebPreview::processWatch() const { return WindowsWebPreviewProcessWatch({}); }
bool WindowsWebPreview::start(void*,const std::filesystem::path&,WindowsWebPreviewProfile&,modules::WebPreviewAssets,std::string,unsigned,unsigned,bool,Completion,std::string& error) { error="WebView2 preview is not available in this build"; return false; }
}
#endif
