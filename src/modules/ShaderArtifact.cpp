#include "modules/ShaderCompiler.hpp"
#include <algorithm>
#include <array>
#include <system_error>
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
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace lucid::modules {
namespace {
namespace fs = std::filesystem;
constexpr std::size_t maxShaderBytes = 1024U * 1024U, maxArtifactBytes = 16U * 1024U * 1024U;
std::string u8(const fs::path& path) { auto bytes = path.u8string(); return {bytes.begin(), bytes.end()}; }
struct Temporary {
    fs::path path;
    ~Temporary() { std::error_code ec; if (!path.empty()) { fs::remove(path / "shader.spv", ec); ec.clear(); fs::remove(path, ec); } }
};
bool createTemporary(Temporary& temp, std::string& error) {
    std::error_code ec; const auto base = fs::temp_directory_path(ec);
    if (ec) { error = "Cannot find temporary directory"; return false; }
#if defined(_WIN32)
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;OICI;FA;;;OW)", SDDL_REVISION_1, &descriptor, nullptr)) { error = "Cannot protect temporary directory"; return false; }
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), descriptor, FALSE};
    constexpr wchar_t hex[] = L"0123456789abcdef";
    for (int attempt = 0; attempt < 16; ++attempt) {
        unsigned char random[16];
        if (BCryptGenRandom(nullptr, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) break;
        std::wstring name = L"lucid-shader-";
        for (auto byte : random) { name += hex[byte >> 4]; name += hex[byte & 15]; }
        const auto candidate = base / name;
        if (CreateDirectoryW(candidate.c_str(), &attributes)) { temp.path = candidate; LocalFree(descriptor); return true; }
        if (GetLastError() != ERROR_ALREADY_EXISTS) break;
    }
    LocalFree(descriptor); error = "Cannot create private temporary directory"; return false;
#else
    const auto pattern = (base / "lucid-shader-XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end()); name.push_back('\0');
    const auto* path = ::mkdtemp(name.data());
    if (!path) { error = "Cannot create private temporary directory"; return false; }
    temp.path = path; return true;
#endif
}
bool readArtifact(const fs::path& path, std::string& bytes, std::string& error) {
#if defined(_WIN32)
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) { error = "Compiler did not produce a readable artifact"; return false; }
    BY_HANDLE_FILE_INFORMATION info{}, after{}; LARGE_INTEGER size{}, finalSize{};
    if (GetFileType(file) != FILE_TYPE_DISK || !GetFileInformationByHandle(file, &info) || (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) ||
        !GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > static_cast<LONGLONG>(maxArtifactBytes)) {
        CloseHandle(file); error = "Compiler artifact is unsafe or too large"; return false;
    }
    bytes.assign(static_cast<std::size_t>(size.QuadPart), '\0'); DWORD actual = 0, extraRead = 0; char extra;
    bool ok = (bytes.empty() || (ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &actual, nullptr) && actual == bytes.size())) &&
        ReadFile(file, &extra, 1, &extraRead, nullptr) && extraRead == 0 && GetFileInformationByHandle(file, &after) &&
        GetFileSizeEx(file, &finalSize) && finalSize.QuadPart == size.QuadPart && CompareFileTime(&info.ftLastWriteTime, &after.ftLastWriteTime) == 0;
    CloseHandle(file);
#else
    const int file = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (file < 0) { error = "Compiler did not produce a regular artifact"; return false; }
    struct stat info{}, after{};
    if (::fstat(file, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0 || static_cast<std::uintmax_t>(info.st_size) > maxArtifactBytes) {
        ::close(file); error = "Compiler artifact is unsafe or too large"; return false;
    }
    const auto size = static_cast<std::size_t>(info.st_size); bytes.assign(size + 1, '\0'); std::size_t total = 0; bool ok = true;
    while (total < bytes.size()) { const auto n = ::read(file, bytes.data() + total, bytes.size() - total); if (n > 0) total += static_cast<std::size_t>(n); else if (!n) break; else if (errno != EINTR) { ok = false; break; } }
    ok = ok && ::fstat(file, &after) == 0 && after.st_dev == info.st_dev && after.st_ino == info.st_ino && after.st_size == info.st_size && after.st_mtime == info.st_mtime && total == size;
#if defined(__APPLE__)
    ok = ok && after.st_mtimespec.tv_nsec == info.st_mtimespec.tv_nsec;
#else
    ok = ok && after.st_mtim.tv_nsec == info.st_mtim.tv_nsec;
#endif
    ::close(file); bytes.resize(size);
#endif
    if (!ok) { bytes.clear(); error = "Compiler artifact changed while reading"; return false; }
    constexpr std::array<unsigned char, 4> magic{0x03, 0x02, 0x23, 0x07};
    if (bytes.size() < 20 || bytes.size() % 4 != 0 || !std::equal(magic.begin(), magic.end(), bytes.begin(), [](unsigned char a, char b) { return a == static_cast<unsigned char>(b); })) {
        bytes.clear(); error = "Compiler output is not a SPIR-V binary"; return false;
    }
    return true;
}
}
ShaderCompilation compileGlslArtifact(workspace::Project& project, const fs::path& executable,
    std::string_view relativeFile, std::string_view stage, std::string_view relativeOutput,
    std::optional<std::string_view> expectedOutput, std::chrono::milliseconds timeout,
    const std::atomic_bool* cancel, std::optional<std::string_view> expectedSource) {
    ShaderCompilation result;
    constexpr std::array<std::string_view, 6> stages{"vert", "frag", "geom", "tesc", "tese", "comp"};
    std::error_code ec;
    if (std::find(stages.begin(), stages.end(), stage) == stages.end()) { result.error = "Unsupported shader stage"; return result; }
    if (!executable.is_absolute() || !fs::is_regular_file(executable, ec) || ec || timeout.count() <= 0 || timeout > std::chrono::hours(1)) { result.error = "Select an absolute compiler and a timeout up to one hour"; return result; }
    // Project::resolve below rejects rooted/escaping paths after converting
    // their UTF-8 spelling to the native platform representation.
    if (relativeFile.empty() || relativeOutput.empty() || relativeFile.find('\0') != std::string_view::npos || relativeOutput.find('\0') != std::string_view::npos) { result.error = "Select project-relative source and output files"; return result; }
    std::string error, before, after;
    const auto source = project.resolve(relativeFile, error); if (source.empty()) { result.error = "Cannot resolve shader: " + error; return result; }
    const auto output = project.resolve(relativeOutput, error); if (output.empty()) { result.error = "Cannot resolve output: " + error; return result; }
    if (source == output || (fs::exists(output, ec) && !ec && fs::equivalent(source, output, ec) && !ec)) { result.error = "Output must differ from shader source"; return result; }
    if (!project.readFileBounded(relativeFile, before, error, maxShaderBytes, cancel)) { result.error = "Cannot read shader: " + error; return result; }
    if (expectedSource && before != *expectedSource) { result.error = "Shader changed on disk; reload before compiling"; return result; }
    if (cancel && cancel->load()) { result.error = "Shader compilation cancelled"; return result; }
    Temporary temp; if (!createTemporary(temp, result.error)) return result;
    const auto artifact = temp.path / "shader.spv";
    result.process = runModuleProcess(executable, {"-V", "-S", std::string(stage), "-o", u8(artifact), u8(source)}, timeout, 1024U * 1024U, cancel);
    result.diagnostics = parseShaderDiagnostics(result.process.output);
    if (!result.process.error.empty()) result.error = "Compiler process failed: " + result.process.error;
    else if (result.process.cancelled || (cancel && cancel->load())) result.error = "Shader compilation cancelled";
    else if (result.process.timedOut) result.error = "Shader compilation timed out";
    else if (result.process.outputTruncated) result.error = "Shader compiler output exceeded the limit";
    else if (!project.readFileBounded(relativeFile, after, error, maxShaderBytes, cancel) || before != after) result.error = "Shader changed or could not be read after compilation";
    else if (result.process.exitCode != 0) result.completed = true;
    else {
        std::string bytes; if (!readArtifact(artifact, bytes, result.error)) return result;
        if (cancel && cancel->load()) { result.error = "Shader compilation cancelled"; return result; }
        std::string expected; if (expectedOutput) expected = std::string(*expectedOutput);
        if (!project.writeFile(relativeOutput, bytes, result.error, expectedOutput ? &expected : nullptr, !expectedOutput, cancel)) return result;
        result.completed = true; result.succeeded = true;
    }
    return result;
}
}
