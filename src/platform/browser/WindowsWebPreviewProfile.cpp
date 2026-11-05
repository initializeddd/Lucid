#include "platform/browser/WindowsWebPreviewProfile.hpp"
#include "platform/browser/WindowsWebPreview.hpp"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <sddl.h>
namespace lucid::platform {
bool WindowsWebPreviewProfile::claim(std::string& error) { if (path_.empty() || claimed_) { error="Preview profile is unavailable or already used"; return false; } claimed_=true; return true; }
bool WindowsWebPreviewProfile::create(std::string& error) {
    error.clear(); if (!path_.empty()) { error="Preview profile already exists"; return false; }
    std::error_code ec; const auto base=std::filesystem::temp_directory_path(ec); if (ec) { error="Cannot locate temporary directory"; return false; }
    PSECURITY_DESCRIPTOR descriptor=nullptr; if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;OICI;FA;;;OW)",SDDL_REVISION_1,&descriptor,nullptr)) { error="Cannot protect preview profile"; return false; }
    SECURITY_ATTRIBUTES attributes{sizeof(attributes),descriptor,FALSE}; constexpr wchar_t digits[]=L"0123456789abcdef";
    for (int attempt=0;attempt<16;++attempt) {
        unsigned char random[16]; if (BCryptGenRandom(nullptr,random,sizeof(random),BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0) break;
        std::wstring name=L"lucid-web-preview-"; for (auto byte:random) { name+=digits[byte>>4]; name+=digits[byte&15]; }
        const auto candidate=base/name; if (CreateDirectoryW(candidate.c_str(),&attributes)) { path_=candidate; LocalFree(descriptor); return true; }
        if (GetLastError()!=ERROR_ALREADY_EXISTS) break;
    }
    LocalFree(descriptor); error="Cannot create private preview profile"; return false;
}
bool WindowsWebPreviewProfile::cleanupAfterBrowserExit(const WindowsWebPreviewProcessWatch& watch, std::string& error) {
    if (!claimed_ || !watch.matchesProfile(path_) || !watch.observed() || !watch.exited() || !watch.profileReleased()) { error="Browser shutdown and profile release are not confirmed; preview profile retained"; return false; }
    return cleanup(error);
}
bool WindowsWebPreviewProfile::discardUnused(std::string& error) { if (claimed_) { error="Preview profile was used; release must be confirmed"; return false; } return cleanup(error); }
bool WindowsWebPreviewProfile::cleanup(std::string& error) {
    error.clear(); if (path_.empty()) return true;
    const DWORD attributes=GetFileAttributesW(path_.c_str());
    if (attributes==INVALID_FILE_ATTRIBUTES) { if (GetLastError()==ERROR_FILE_NOT_FOUND || GetLastError()==ERROR_PATH_NOT_FOUND) { path_.clear(); claimed_=false; return true; } error="Cannot inspect preview profile; retained for manual cleanup"; return false; }
    if (!(attributes&FILE_ATTRIBUTE_DIRECTORY) || (attributes&FILE_ATTRIBUTE_REPARSE_POINT)) { error="Unsafe preview profile; retained for manual cleanup"; return false; }
    std::error_code ec; std::filesystem::recursive_directory_iterator it(path_,std::filesystem::directory_options::none,ec),end; std::size_t count=0;
    while (!ec && it!=end) {
        if (++count>100000 || it.depth()>32) { error="Preview profile exceeds cleanup limits; retained for manual cleanup"; return false; }
        const DWORD flags=GetFileAttributesW(it->path().c_str()); if (flags==INVALID_FILE_ATTRIBUTES || (flags&FILE_ATTRIBUTE_REPARSE_POINT)) { error="Unsafe preview profile entry; retained for manual cleanup"; return false; }
        it.increment(ec);
    }
    if (ec) { error="Cannot inspect preview profile; retained for manual cleanup"; return false; }
    std::filesystem::remove_all(path_,ec); if (ec) { error="Preview profile is still in use or cannot be removed; retained for manual cleanup"; return false; }
    path_.clear(); claimed_=false; return true;
}
}
#else
namespace lucid::platform {
bool WindowsWebPreviewProfile::claim(std::string& error) { error="Windows preview profiles are unavailable on this platform"; return false; }
bool WindowsWebPreviewProfile::create(std::string& error) { error="Windows preview profiles are unavailable on this platform"; return false; }
bool WindowsWebPreviewProfile::cleanupAfterBrowserExit(const WindowsWebPreviewProcessWatch&, std::string& error) { error="Windows preview profiles are unavailable on this platform"; return false; }
bool WindowsWebPreviewProfile::discardUnused(std::string& error) { error.clear(); return true; }
bool WindowsWebPreviewProfile::cleanup(std::string& error) { error.clear(); return true; }
}
#endif
