#include "platform/browser/WindowsWebPreviewHost.hpp"
#include <utility>
#if defined(_WIN32) && defined(LUCID_ENABLE_WEBVIEW2)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <vector>
namespace lucid::platform {
namespace {
constexpr wchar_t previewWindowMarker[]=L"Lucid.WebPreview.WindowOwner";
std::filesystem::path packagedLoader(std::string& error) {
    std::vector<wchar_t> path(32768); const DWORD length=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
    if (!length || length>=path.size()) { error="Cannot locate the application executable"; return {}; }
    const auto loader=std::filesystem::path(std::wstring(path.data(),length)).parent_path()/L"WebView2Loader.dll";
    const DWORD flags=GetFileAttributesW(loader.c_str());
    if (flags==INVALID_FILE_ATTRIBUTES || (flags&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))) { error="Packaged WebView2 loader is unavailable or unsafe"; return {}; }
    return loader;
}
}
struct WindowsWebPreviewHost::Impl {
    std::unique_ptr<WindowsWebPreviewSession> session=std::make_unique<WindowsWebPreviewSession>();
    HWND window=nullptr; DWORD thread=0; bool comInitialized=false;
    std::string error;
    bool sameThread() const noexcept { return !thread || thread==GetCurrentThreadId(); }
    ~Impl() { session.reset(); if (window && IsWindow(window) && GetPropW(window,previewWindowMarker)==this) RemovePropW(window,previewWindowMarker); if (comInitialized && sameThread()) CoUninitialize(); }
};
WindowsWebPreviewHost::WindowsWebPreviewHost():impl_(std::make_unique<Impl>()) {}
WindowsWebPreviewHost::~WindowsWebPreviewHost() { cancel(); }
bool WindowsWebPreviewHost::start(void* nativeWindow,modules::WebPreviewAssets assets,std::string target,unsigned width,unsigned height,bool trusted,std::string& error) {
    error.clear(); if (!trusted) { error="Web preview requires explicit trust"; return false; }
    if (!impl_->sameThread()) { error="Web preview must remain on its owning UI thread"; return false; }
    const auto hwnd=static_cast<HWND>(nativeWindow); if (!hwnd || !IsWindow(hwnd) || GetWindowThreadProcessId(hwnd,nullptr)!=GetCurrentThreadId()) { error="Preview window must belong to the current UI thread"; return false; }
    if (impl_->session->running() || !impl_->session->retainedProfile().empty()) { error="Previous preview profile has not been released"; return false; }
    const auto loader=packagedLoader(error); if (loader.empty()) return false;
    if (!impl_->comInitialized) { const HRESULT status=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED); if (FAILED(status)) { error="Web preview needs an STA UI thread"; return false; } impl_->comInitialized=true; impl_->thread=GetCurrentThreadId(); }
    const auto owner=GetPropW(hwnd,previewWindowMarker); if (owner && owner!=impl_.get()) { error="Another preview owns this window"; return false; }
    if (!SetPropW(hwnd,previewWindowMarker,impl_.get())) { error="Cannot track preview window lifetime"; return false; }
    if (impl_->window && impl_->window!=hwnd && IsWindow(impl_->window) && GetPropW(impl_->window,previewWindowMarker)==impl_.get()) RemovePropW(impl_->window,previewWindowMarker);
    impl_->window=hwnd;
    if (!impl_->session->start(hwnd,loader,std::move(assets),std::move(target),width,height,true,error)) { if (GetPropW(hwnd,previewWindowMarker)==impl_.get()) RemovePropW(hwnd,previewWindowMarker); impl_->window=nullptr; return false; }
    return true;
}
void WindowsWebPreviewHost::poll() { if (!impl_->sameThread()) return; if (impl_->session->running() && (!impl_->window || !IsWindow(impl_->window) || GetPropW(impl_->window,previewWindowMarker)!=impl_.get())) impl_->session->cancel(); impl_->session->poll(); }
void WindowsWebPreviewHost::cancel() { if (impl_->sameThread()) impl_->session->cancel(); }
bool WindowsWebPreviewHost::running() const noexcept { return impl_->session->running(); }
bool WindowsWebPreviewHost::finished() const noexcept { return impl_->session->finished(); }
const WindowsWebPreviewResult& WindowsWebPreviewHost::result() const noexcept { return impl_->session->result(); }
const std::filesystem::path& WindowsWebPreviewHost::retainedProfile() const noexcept { return impl_->session->retainedProfile(); }
const std::string& WindowsWebPreviewHost::cleanupError() const noexcept { return impl_->session->cleanupError(); }
}
#else
namespace lucid::platform {
struct WindowsWebPreviewHost::Impl { WindowsWebPreviewSession session; };
WindowsWebPreviewHost::WindowsWebPreviewHost():impl_(std::make_unique<Impl>()) {}
WindowsWebPreviewHost::~WindowsWebPreviewHost()=default;
bool WindowsWebPreviewHost::start(void*,modules::WebPreviewAssets,std::string,unsigned,unsigned,bool trusted,std::string& error) { error=trusted?"WebView2 preview is not available in this build":"Web preview requires explicit trust"; return false; }
void WindowsWebPreviewHost::poll() {}
void WindowsWebPreviewHost::cancel() {}
bool WindowsWebPreviewHost::running() const noexcept { return false; }
bool WindowsWebPreviewHost::finished() const noexcept { return false; }
const WindowsWebPreviewResult& WindowsWebPreviewHost::result() const noexcept { return impl_->session.result(); }
const std::filesystem::path& WindowsWebPreviewHost::retainedProfile() const noexcept { return impl_->session.retainedProfile(); }
const std::string& WindowsWebPreviewHost::cleanupError() const noexcept { return impl_->session.cleanupError(); }
}
#endif
