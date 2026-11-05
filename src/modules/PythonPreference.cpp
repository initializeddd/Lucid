#include "modules/PythonPreference.hpp"
#include "workspace/Project.hpp"
#include "editor/json/Json.hpp"
#include "debug/Protocol.hpp"
#include <cstdint>
#include <system_error>
#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace lucid::modules {
namespace fs = std::filesystem;
namespace {
std::string utf8(const fs::path& p) { auto s = p.generic_u8string(); return std::string(s.begin(), s.end()); }
std::string hashPath(const std::string& path) {
    std::uint64_t a = 14695981039346656037ULL, b = 7809847782465536322ULL;
    for (unsigned char c : path) { a = (a ^ c) * 1099511628211ULL; b = (b ^ c) * 14029467366897019727ULL; }
    static constexpr char hex[] = "0123456789abcdef"; std::string out(32, '0');
    for (int i=15;i>=0;--i) { out[i]=hex[a&15]; a>>=4; out[16+i]=hex[b&15]; b>>=4; } return out;
}
bool safeSettings(const fs::path& directory, std::string& error) {
    std::error_code ec; auto absolute = fs::absolute(directory, ec).lexically_normal();
    if (ec || !absolute.is_absolute()) { error = "Invalid settings directory"; return false; }
    auto canonical = fs::canonical(absolute, ec);
    if (ec || canonical != absolute || !fs::is_directory(canonical, ec) || ec) { error = "Settings directory must exist and contain no symlink components"; return false; }
#if !defined(_WIN32)
    for (auto current = canonical; !current.empty(); current = current.parent_path()) {
        struct stat st{};
        if (::lstat(current.c_str(), &st) || !S_ISDIR(st.st_mode) ||
            (current == canonical && st.st_uid != ::geteuid()) ||
            (current != canonical && st.st_uid != ::geteuid() && st.st_uid != 0) ||
            ((st.st_mode & 0022) && !(current != canonical && st.st_uid == 0 && (st.st_mode & S_ISVTX)))) {
            error = "Settings directory ownership or permissions are unsafe"; return false;
        }
        if (current == current.root_path()) break;
    }
#endif
    return true;
}
bool safeFile(const fs::path& path, bool& exists, std::string& error) {
    std::error_code ec; const auto status = fs::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory || status.type() == fs::file_type::not_found) { exists = false; return true; }
    if (ec || status.type() != fs::file_type::regular) { error = "Preference file is not a regular file"; return false; }
#if !defined(_WIN32)
    struct stat st{}; if (::lstat(path.c_str(), &st) || !S_ISREG(st.st_mode) || st.st_uid != ::geteuid() || st.st_nlink != 1 || (st.st_mode & 0022)) { error = "Preference file ownership or permissions are unsafe"; return false; }
#endif
    exists = true; return true;
}
bool openStore(const fs::path& directory, workspace::Project& project, std::string& error) {
    if (!safeSettings(directory,error)) return false;
    bool created = false; project = workspace::Project::openOrCreate(directory, created, error, false, false); return error.empty();
}
}
bool loadPythonPreference(const fs::path& settingsDirectory, const fs::path& root, PythonPreference& preference, std::string& error) {
    preference = {}; error.clear(); std::error_code ec;
    auto canonical = fs::canonical(root, ec);
    if (ec || !fs::is_directory(canonical, ec) || ec) { error = "Project directory is unavailable"; return false; }
    workspace::Project store; if (!openStore(settingsDirectory, store, error)) return false;
    PythonPreference result; result.projectRoot = canonical; result.settingsRoot = store.root(); result.filename = "python-interpreter-" + hashPath(utf8(canonical)) + ".json";
    if (!safeFile(store.root() / result.filename, result.exists, error)) return false;
    if (result.exists) {
        if (!store.readFileBounded(result.filename, result.baseline, error, 65536)) return false;
        editor::JsonError parseError; auto json = editor::parseJson(result.baseline, parseError);
        if (!json || json->type != editor::JsonType::Object || json->children.size() != 2 ||
            !((json->children[0].key == "project" && json->children[1].key == "interpreter") ||
              (json->children[1].key == "project" && json->children[0].key == "interpreter"))) {
            error = "Invalid preference fields"; return false;
        }
        const auto* path = json->find("project");
        const auto* interpreter = json && json->type == editor::JsonType::Object ? json->find("interpreter") : nullptr;
        if (!path || path->type != editor::JsonType::String || path->stringValue != utf8(canonical) || !interpreter || interpreter->type != editor::JsonType::String) { error = "Invalid preference or project hash collision"; return false; }
        try { result.interpreter = fs::path(std::u8string(interpreter->stringValue.begin(), interpreter->stringValue.end())); }
        catch (const std::exception&) { error = "Invalid interpreter path in preference"; return false; }
        if (interpreter->stringValue.find('\0') != std::string::npos || (!result.interpreter.empty() && !result.interpreter.is_absolute())) { error = "Invalid interpreter path in preference"; return false; }
    }
    preference = std::move(result); return true;
}
bool savePythonPreference(const fs::path& settingsDirectory, PythonPreference& preference, const fs::path& interpreter, std::string& error) {
    error.clear();
    if (preference.projectRoot.empty() || preference.filename != "python-interpreter-" + hashPath(utf8(preference.projectRoot)) + ".json" || (!interpreter.empty() && !interpreter.is_absolute()) || utf8(interpreter).find('\0') != std::string::npos) { error = "Invalid preference snapshot or interpreter path"; return false; }
    std::error_code ec; if (fs::canonical(preference.projectRoot, ec) != preference.projectRoot || ec || !fs::is_directory(preference.projectRoot, ec) || ec) { error = "Project directory changed or became unavailable"; return false; }
    workspace::Project store; if (!openStore(settingsDirectory, store, error)) return false;
    if (store.root() != preference.settingsRoot) { error = "Preference belongs to a different settings directory"; return false; }
    bool exists = false; if (!safeFile(store.root() / preference.filename, exists, error)) return false;
    if (exists != preference.exists) { error = "Preference changed or became unavailable"; return false; }
    auto contents = "{\"project\":" + debug::jsonString(utf8(preference.projectRoot)) + ",\"interpreter\":" + debug::jsonString(utf8(interpreter)) + "}\n";
    if (contents.size() > 65536) { error = "Preference exceeds size limit"; return false; }
    if (!store.writeFile(preference.filename, contents, error, preference.exists ? &preference.baseline : nullptr, !preference.exists)) return false;
    preference.interpreter = interpreter; preference.baseline = std::move(contents); preference.exists = true; return true;
}
}
