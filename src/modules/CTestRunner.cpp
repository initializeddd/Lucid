#include "modules/CTestRunner.hpp"
#include <algorithm>
#include <fstream>
#include <system_error>
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
namespace fs = std::filesystem;
namespace {
std::string u8(const fs::path& p) { auto value = p.u8string(); return {value.begin(), value.end()}; }
bool validate(const fs::path& executable, const fs::path& build, std::string& error) {
    std::error_code ec;
    if (!executable.is_absolute() || u8(executable).find('\0') != std::string::npos || !fs::is_regular_file(executable, ec) || ec) { error = "CTest executable must be an absolute existing file"; return false; }
    if (!build.is_absolute() || u8(build).find('\0') != std::string::npos || !fs::is_directory(build, ec) || ec) { error = "CTest build directory must be an absolute existing directory"; return false; }
    return true;
}
bool usable(const ModuleProcessResult& p, std::string& error) {
    if (!p.error.empty()) { error = "CTest process could not be started or observed: " + p.error; return false; }
    if (p.cancelled) { error = "CTest cancelled"; return false; }
    if (p.timedOut) { error = "CTest timed out"; return false; }
    if (p.outputTruncated) { error = "CTest output exceeded the limit"; return false; }
    return true;
}
// Limit names to a subset whose literal form has unambiguous CMake regex
// semantics. Names outside this subset are refused rather than widened.
bool expression(const std::string& name, std::string& out) {
    if (!canRunCTestNameExactly(name)) return false;
    out = "^";
    for (unsigned char c : name) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '/' || c == ':' || c == ' ') out += static_cast<char>(c);
        else if (std::string_view(".[]()+*?|^$").find(static_cast<char>(c)) != std::string_view::npos) { out += '\\'; out += static_cast<char>(c); }
        else return false;
    }
    out += '$'; return true;
}
struct TempDirectory {
    fs::path path;
    ~TempDirectory() {
        std::error_code ec; if (!path.empty()) { fs::remove(path / "results.xml", ec); ec.clear(); fs::remove(path, ec); }
    }
};
bool createTemp(TempDirectory& temp, std::string& error) {
    std::error_code ec; auto parent = fs::temp_directory_path(ec); if (ec) { error = "Cannot find temporary directory"; return false; }
#if defined(_WIN32)
    // A random name plus a protected DACL prevents other accounts from
    // replacing the report between creation and reading. CTest runs as the
    // owner and therefore inherits access to the report it creates.
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;OICI;FA;;;OW)", SDDL_REVISION_1, &descriptor, nullptr)) { error = "Cannot protect temporary directory"; return false; }
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), descriptor, FALSE};
    constexpr wchar_t hex[] = L"0123456789abcdef";
    for (int attempt = 0; attempt < 16; ++attempt) {
        unsigned char random[16];
        if (BCryptGenRandom(nullptr, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) break;
        std::wstring name = L"lucid-ctest-";
        for (auto byte : random) { name += hex[byte >> 4]; name += hex[byte & 15]; }
        const auto candidate = parent / name;
        if (CreateDirectoryW(candidate.c_str(), &attributes)) { temp.path = candidate; LocalFree(descriptor); return true; }
        if (GetLastError() != ERROR_ALREADY_EXISTS) break;
    }
    LocalFree(descriptor); error = "Cannot create private temporary directory"; return false;
#else
    std::string pattern = (parent / "lucid-ctest-XXXXXX").string(); std::vector<char> buffer(pattern.begin(), pattern.end()); buffer.push_back('\0');
    const char* directory = ::mkdtemp(buffer.data()); if (!directory) { error = "Cannot create private temporary directory"; return false; }
    temp.path = directory; return true;
#endif
}
bool readReport(const fs::path& report, std::string& xml, std::string& error) {
#if defined(_WIN32)
    HANDLE file = CreateFileW(report.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) { error = "CTest did not produce a readable JUnit report"; return false; }
    BY_HANDLE_FILE_INFORMATION info{}; LARGE_INTEGER size{};
    if (GetFileType(file) != FILE_TYPE_DISK || !GetFileInformationByHandle(file, &info) || (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) ||
        !GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > 8LL * 1024 * 1024) {
        CloseHandle(file); error = "CTest report is unsafe or exceeds the limit"; return false;
    }
    xml.assign(static_cast<std::size_t>(size.QuadPart), '\0'); DWORD actual = 0;
    bool ok = xml.empty() || (ReadFile(file, xml.data(), static_cast<DWORD>(xml.size()), &actual, nullptr) && actual == xml.size());
    char extra; DWORD extraRead = 0;
    ok = ok && ReadFile(file, &extra, 1, &extraRead, nullptr) && extraRead == 0;
    BY_HANDLE_FILE_INFORMATION after{}; LARGE_INTEGER finalSize{};
    ok = ok && GetFileInformationByHandle(file, &after) && GetFileSizeEx(file, &finalSize) && finalSize.QuadPart == size.QuadPart &&
        CompareFileTime(&info.ftLastWriteTime, &after.ftLastWriteTime) == 0;
    CloseHandle(file);
    if (!ok) { xml.clear(); error = "CTest report changed while reading"; return false; }
    return true;
#else
    const int file = ::open(report.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (file < 0) { error = "CTest did not produce a regular JUnit report"; return false; }
    struct stat info{};
    if (::fstat(file, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0 || info.st_size > 8LL * 1024 * 1024) {
        ::close(file); error = "CTest report is unsafe or exceeds the limit"; return false;
    }
    const auto size = static_cast<std::size_t>(info.st_size); xml.assign(size + 1, '\0');
    std::size_t total = 0; bool ok = true;
    while (total < xml.size()) {
        const auto n = ::read(file, xml.data() + total, xml.size() - total);
        if (n > 0) total += static_cast<std::size_t>(n);
        else if (n == 0) break;
        else if (errno != EINTR) { ok = false; break; }
    }
    struct stat after{};
    ok = ok && ::fstat(file, &after) == 0 && after.st_dev == info.st_dev && after.st_ino == info.st_ino && after.st_size == info.st_size && after.st_mtime == info.st_mtime && total == size;
#if defined(__APPLE__)
    ok = ok && after.st_mtimespec.tv_nsec == info.st_mtimespec.tv_nsec;
#else
    ok = ok && after.st_mtim.tv_nsec == info.st_mtim.tv_nsec;
#endif
    ::close(file);
    if (!ok) { xml.clear(); error = "CTest report changed while reading"; return false; }
    xml.resize(size); return true;
#endif
}
}
bool ctestSingleRunSupported() noexcept { return true; }
std::vector<fs::path> findCTestOnPath(std::string_view pathVariable) {
    std::vector<fs::path> result;
    if (pathVariable.size() > 65536) return result;
#if defined(_WIN32)
    constexpr char delimiter = ';'; constexpr const char* executable = "ctest.exe";
#else
    constexpr char delimiter = ':'; constexpr const char* executable = "ctest";
#endif
    std::size_t start = 0;
    while (start <= pathVariable.size() && result.size() < 256) {
        auto end = pathVariable.find(delimiter, start); if (end == pathVariable.npos) end = pathVariable.size();
        auto part = pathVariable.substr(start, end - start);
        if (!part.empty() && part.find('\0') == part.npos) {
            auto directory = fs::path(std::u8string(part.begin(), part.end()));
            if (directory.is_absolute()) {
                std::error_code ec; auto candidate = directory / executable;
                if (fs::is_regular_file(candidate, ec) && !ec && std::find(result.begin(), result.end(), candidate) == result.end()) result.push_back(std::move(candidate));
            }
        }
        if (end == pathVariable.size()) break;
        start = end + 1;
    }
    return result;
}
CTestDiscoveryResult discoverCTest(const fs::path& executable, const fs::path& build, const std::atomic_bool* cancel) {
    CTestDiscoveryResult result;
    if (!validate(executable, build, result.error)) return result;
    result.process = runModuleProcess(executable, {"--test-dir", u8(build), "--show-only=json-v1"}, std::chrono::seconds(30), 8U*1024U*1024U, cancel);
    if (!usable(result.process, result.error)) return result;
    if (result.process.exitCode != 0) { result.error = "CTest discovery failed"; return result; }
    result.ok = parseCTestDiscovery(result.process.output, result.discovery, result.error); return result;
}
CTestRunResult runCTestSingle(const fs::path& executable, const fs::path& build, const std::string& name, std::chrono::milliseconds timeout, const std::atomic_bool* cancel) {
    CTestRunResult result; std::string regex;
    if (!ctestSingleRunSupported()) { result.error = "Single-test execution is unavailable on this platform"; return result; }
    if (!validate(executable, build, result.error)) return result;
    if (timeout.count() <= 0 || timeout > std::chrono::hours(1)) { result.error = "CTest timeout must be positive and at most one hour"; return result; }
    if (!expression(name, regex)) { result.error = "Test name contains unsupported regex characters"; return result; }
    auto discovery = discoverCTest(executable, build, cancel);
    if (!discovery.ok) { result.process = std::move(discovery.process); result.error = "Cannot verify test selection: " + discovery.error; return result; }
    if (std::count_if(discovery.discovery.tests.begin(), discovery.discovery.tests.end(), [&](const auto& t){return t.name == name;}) != 1) { result.error = "Selected test was not uniquely discovered"; return result; }
    TempDirectory cleanup; if (!createTemp(cleanup, result.error)) return result;
    const auto report = cleanup.path / "results.xml";
    result.process = runModuleProcess(executable, {"--test-dir", u8(build), "--tests-regex", regex, "--output-junit", u8(report), "--output-on-failure", "--no-tests=error", "--fixture-exclude-any", ".*"}, timeout, 1024U*1024U, cancel);
    if (!usable(result.process, result.error)) return result;
    std::string xml; if (!readReport(report, xml, result.error)) return result;
    if (!parseCTestJUnit(xml, result.results, result.error)) return result;
    if (result.results.tests.size() != 1 || result.results.tests[0].name != name) { result.error = "CTest report does not match selected test"; return result; }
    result.ok = true; return result; // Nonzero exit denotes a completed failing test.
}
}
