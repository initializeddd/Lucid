#include "modules/OpenOcdFlash.hpp"
#include <picosha2.h>
#include <algorithm>
#include <cctype>
#include <system_error>
#include <vector>
#include <sstream>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <sddl.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace lucid::modules {
namespace fs = std::filesystem;
namespace {
constexpr std::size_t maxFirmware = 16U * 1024U * 1024U;
bool validProfile(const BoardProfile& profile) {
    // Round-trip through the strict profile parser rather than passing raw
    // user-provided text to OpenOCD's Tcl interpreter.
    if (profile.incomplete || !profile.error.empty() || !profile.warnings.empty() ||
        profile.interfaceConfig.empty() || profile.targetConfig.empty() || profile.firmware.empty()) return false;
    const auto text = "source " + profile.interfaceConfig + "\nsource " + profile.targetConfig + "\nfirmware " + profile.firmware + "\n" +
        (profile.adapterKhz ? "adapter_khz " + std::to_string(profile.adapterKhz) + "\n" : "");
    const auto parsed = parseBoardProfile(text);
    return parsed.error.empty() && !parsed.incomplete && parsed.warnings.empty() && parsed.interfaceConfig == profile.interfaceConfig &&
        parsed.targetConfig == profile.targetConfig && parsed.firmware == profile.firmware && parsed.adapterKhz == profile.adapterKhz;
}
std::string utf8(const fs::path& path) { const auto value = path.generic_u8string(); return {value.begin(), value.end()}; }
struct Temporary {
    fs::path path;
#if !defined(_WIN32)
    int directory = -1;
    dev_t device = 0;
    ino_t inode = 0;
#endif
    ~Temporary() {
        if (path.empty()) return;
#if defined(_WIN32)
        // Never recursively remove files made by the selected program. Refuse
        // cleanup through a replacement link; a same-user race remains possible.
        std::error_code ec;
        if (fs::symlink_status(path, ec).type() != fs::file_type::directory || ec) return;
        fs::remove(path / "firmware.elf", ec); ec.clear();
        fs::remove(path / "firmware.hex", ec); ec.clear();
        fs::remove(path / "firmware.bin", ec); ec.clear();
        fs::remove(path, ec);
#else
        if (directory < 0) return;
        // The held descriptor keeps cleanup inside the directory we created,
        // even if its pathname was replaced while the external tool ran.
        (void)::unlinkat(directory, "firmware.elf", 0);
        (void)::unlinkat(directory, "firmware.hex", 0);
        (void)::unlinkat(directory, "firmware.bin", 0);
        struct stat current{};
        if (::lstat(path.c_str(), &current) == 0 && S_ISDIR(current.st_mode) &&
            current.st_dev == device && current.st_ino == inode) (void)::rmdir(path.c_str());
        ::close(directory);
#endif
    }
};
bool createTemporary(Temporary& temp, std::string& error) {
    std::error_code ec; const auto base = fs::temp_directory_path(ec);
    if (ec) { error = "Cannot locate a temporary directory"; return false; }
#if defined(_WIN32)
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;OICI;FA;;;OW)", SDDL_REVISION_1, &descriptor, nullptr)) { error = "Cannot protect temporary directory"; return false; }
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), descriptor, FALSE}; constexpr wchar_t hex[] = L"0123456789abcdef";
    for (int attempt = 0; attempt < 16; ++attempt) {
        unsigned char bytes[16]; if (BCryptGenRandom(nullptr, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) break;
        std::wstring name = L"lucid-firmware-"; for (const auto b : bytes) { name += hex[b >> 4]; name += hex[b & 15]; }
        const auto candidate = base / name;
        if (CreateDirectoryW(candidate.c_str(), &attributes)) { temp.path = candidate; LocalFree(descriptor); return true; }
        if (GetLastError() != ERROR_ALREADY_EXISTS) break;
    }
    LocalFree(descriptor); error = "Cannot create private temporary directory"; return false;
#else
    const auto pattern = (base / "lucid-firmware-XXXXXX").string(); std::vector<char> name(pattern.begin(), pattern.end()); name.push_back('\0');
    const auto* path = ::mkdtemp(name.data()); if (!path) { error = "Cannot create private temporary directory"; return false; }
    temp.path = path;
    temp.directory = ::open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    struct stat info{};
    if (temp.directory < 0 || ::fstat(temp.directory, &info) != 0 || !S_ISDIR(info.st_mode)) {
        if (temp.directory >= 0) ::close(temp.directory);
        temp.directory = -1; (void)::rmdir(path); temp.path.clear();
        error = "Cannot safely open temporary directory"; return false;
    }
    temp.device = info.st_dev; temp.inode = info.st_ino; return true;
#endif
}
std::string reviewDigest(std::string_view sha, const std::optional<std::uint32_t>& address) {
    const auto text = std::string(sha) + (address ? ":bin:" + std::to_string(*address) : ":image");
    return picosha2::hash256_hex_string(text);
}
bool tclQuote(std::string_view value, std::string& quoted) {
    quoted = "\"";
    for (unsigned char c : value) {
        if (c < 32 || c == 127) return false;
        if (c == '\\' || c == '"' || c == '$' || c == '[' || c == ']') quoted.push_back('\\');
        quoted.push_back(static_cast<char>(c));
    }
    quoted += '"'; return true;
}
}
bool prepareFirmware(const workspace::Project& project, const BoardProfile& profile, FirmwareSnapshot& snapshot, std::string& error, const std::atomic_bool* cancel, std::optional<std::uint32_t> binaryAddress) {
    snapshot = {}; error.clear();
    if (!validProfile(profile)) { error = "Profile must contain one interface, target and firmware with no unsupported directives"; return false; }
    auto extension = fs::path(profile.firmware).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extension != ".elf" && extension != ".hex" && extension != ".bin") { error = "Choose ELF, Intel HEX or raw BIN firmware"; return false; }
    if ((extension == ".bin") != binaryAddress.has_value()) { error = "Raw BIN requires an explicit address; ELF and HEX must not specify one"; return false; }
    std::string bytes;
    if (!project.readFileBounded(profile.firmware, bytes, error, maxFirmware, cancel)) return false;
    if (bytes.empty()) { error = "Firmware file is empty"; return false; }
    if (binaryAddress && bytes.size() > std::uint64_t{0x100000000} - *binaryAddress) { error = "Firmware exceeds the 32-bit address space"; return false; }
    snapshot.profile = profile; snapshot.sha256 = picosha2::hash256_hex_string(bytes); snapshot.bytes = std::move(bytes); snapshot.extension = extension; snapshot.binaryAddress=binaryAddress; snapshot.reviewDigest=reviewDigest(snapshot.sha256,binaryAddress); return true;
}
ModuleProcessResult flashFirmware(const fs::path& executable, const FirmwareSnapshot& snapshot, const std::atomic_bool* cancel) {
    ModuleProcessResult result; std::error_code ec;
    if (cancel && cancel->load()) { result.cancelled = true; result.error = "Flash cancelled before starting"; return result; }
    auto profileExtension = fs::path(snapshot.profile.firmware).extension().string();
    std::transform(profileExtension.begin(), profileExtension.end(), profileExtension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (!executable.is_absolute() || !fs::is_regular_file(executable, ec) || ec || !validProfile(snapshot.profile) ||
        (snapshot.extension != ".elf" && snapshot.extension != ".hex" && snapshot.extension != ".bin") || (snapshot.extension == ".bin") != snapshot.binaryAddress.has_value() || (snapshot.binaryAddress && snapshot.bytes.size() > std::uint64_t{0x100000000} - *snapshot.binaryAddress) || snapshot.reviewDigest != reviewDigest(snapshot.sha256,snapshot.binaryAddress) || snapshot.bytes.empty() || snapshot.bytes.size() > maxFirmware ||
        snapshot.extension != profileExtension || snapshot.sha256 != picosha2::hash256_hex_string(snapshot.bytes)) { result.error = "Invalid executable or reviewed firmware snapshot"; return result; }
    Temporary temp; if (!createTemporary(temp, result.error)) return result;
    bool created = false; auto store = workspace::Project::openOrCreate(temp.path, created, result.error, false, false);
    if (!result.error.empty()) return result;
    const auto name = "firmware" + snapshot.extension;
    if (!store.writeFile(name, snapshot.bytes, result.error, nullptr, true, cancel)) {
        if (cancel && cancel->load()) result.cancelled = true;
        return result;
    }
    std::string quoted;
    if (!tclQuote(utf8(temp.path / name), quoted)) { result.error = "Temporary firmware path is not supported"; return result; }
    std::vector<std::string> args{"-f", snapshot.profile.interfaceConfig, "-f", snapshot.profile.targetConfig};
    if (snapshot.profile.adapterKhz) { args.emplace_back("-c"); args.emplace_back("adapter speed " + std::to_string(snapshot.profile.adapterKhz)); }
    args.emplace_back("-c"); std::string program = "program " + quoted + " verify reset exit";
    if (snapshot.binaryAddress) { std::ostringstream address; address << " 0x" << std::hex << *snapshot.binaryAddress; program += address.str(); }
    args.emplace_back(std::move(program));
    return runModuleProcess(executable, args, std::chrono::minutes(5), 1024U * 1024U, cancel);
}
}
