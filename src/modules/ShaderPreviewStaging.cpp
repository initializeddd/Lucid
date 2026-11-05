#include "modules/ShaderPreview.hpp"
#include "modules/ShaderPreviewInterface.hpp"
#include <filesystem>
#include <cstdlib>
#include <vector>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <sddl.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif
namespace lucid::modules {
namespace {
struct Temporary { std::filesystem::path dir; ~Temporary() { std::error_code ec; if (!dir.empty()) { std::filesystem::remove(dir/"shader.spv",ec); ec.clear(); std::filesystem::remove(dir,ec); } } };
bool makeDirectory(Temporary& temp,std::string& error) {
    std::error_code ec; const auto base=std::filesystem::temp_directory_path(ec); if (ec) { error="Cannot find temporary directory"; return false; }
#if defined(_WIN32)
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;OICI;FA;;;OW)",SDDL_REVISION_1,&descriptor,nullptr)) { error="Cannot protect temporary directory"; return false; }
    SECURITY_ATTRIBUTES attributes{sizeof(attributes),descriptor,FALSE}; constexpr wchar_t digits[]=L"0123456789abcdef";
    for (int attempt=0;attempt<16;++attempt) {
        unsigned char random[16]; if (BCryptGenRandom(nullptr,random,sizeof(random),BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0) break;
        std::wstring name=L"lucid-preview-"; for (auto byte:random) { name+=digits[byte>>4]; name+=digits[byte&15]; }
        const auto candidate=base/name; if (CreateDirectoryW(candidate.c_str(),&attributes)) { temp.dir=candidate; LocalFree(descriptor); return true; }
        if (GetLastError()!=ERROR_ALREADY_EXISTS) break;
    }
    LocalFree(descriptor); error="Cannot create private preview directory"; return false;
#else
    const auto pattern=(base/"lucid-preview-XXXXXX").string(); std::vector<char> name(pattern.begin(),pattern.end()); name.push_back('\0');
    const auto* path=::mkdtemp(name.data()); if (!path) { error="Cannot create private preview directory"; return false; } temp.dir=path; return true;
#endif
}
bool writeSnapshot(const std::filesystem::path& path,std::string_view bytes,std::string& error) {
#if defined(_WIN32)
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    if (file==INVALID_HANDLE_VALUE) { error="Cannot stage shader"; return false; }
    DWORD written=0; const bool ok=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr) && written==bytes.size() && FlushFileBuffers(file); CloseHandle(file);
#else
    const int file=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if (file<0) { error="Cannot stage shader"; return false; }
    bool ok=true; std::size_t total=0;
    while (total<bytes.size()) { const auto n=::write(file,bytes.data()+total,bytes.size()-total); if (n>0) total+=static_cast<std::size_t>(n); else if (n<0 && errno==EINTR) continue; else { ok=false; break; } }
    if (::close(file)!=0) ok=false;
#endif
    if (!ok) error="Cannot stage shader";
    return ok;
}
}
bool runStagedShaderPreview(const std::filesystem::path& helper,std::string_view bytes,std::uint32_t width,std::uint32_t height,bool confirmed,ShaderPreviewImage& output,std::string& error,std::chrono::milliseconds timeout,const std::atomic_bool* cancel) {
    output={}; error.clear();
    if (!confirmed) { error="Shader execution requires explicit confirmation"; return false; }
    if (cancel && cancel->load()) { error="Shader preview cancelled"; return false; }
    if (bytes.size()>1024U*1024U) { error="Shader artifact exceeds preview limit"; return false; }
    if (!validatePreviewFragmentInterface(bytes,error)) return false;
    if (!helper.is_absolute() || !width || !height || width>512 || height>512 || timeout.count()<=0 || timeout>std::chrono::seconds(30)) { error="Invalid shader preview request"; return false; }
    Temporary temp; if (!makeDirectory(temp,error) || !writeSnapshot(temp.dir/"shader.spv",bytes,error)) return false;
    return runShaderPreviewHelper(helper,temp.dir/"shader.spv",width,height,true,output,error,timeout,cancel);
}
}
