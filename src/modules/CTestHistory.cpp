#include "modules/CTestHistory.hpp"
#include "workspace/Project.hpp"
#include "editor/json/Json.hpp"
#include "debug/Protocol.hpp"
#include <cstdint>
#include <set>
#include <system_error>
#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace lucid::modules {
namespace fs = std::filesystem;
namespace {
constexpr std::size_t maxBytes = 1024 * 1024, maxRuns = 10, maxTests = 5000;
std::string utf8(const fs::path& p) { auto u = p.generic_u8string(); return {u.begin(), u.end()}; }
std::string filename(const fs::path& project, const fs::path& build) {
    auto input = utf8(project); input.push_back('\0'); input += utf8(build);
    std::uint64_t a = 14695981039346656037ULL, b = 7809847782465536322ULL;
    for (unsigned char c : input) { a = (a ^ c) * 1099511628211ULL; b = (b ^ c) * 14029467366897019727ULL; }
    constexpr char hex[] = "0123456789abcdef"; std::string out = "ctest-history-"; out.resize(out.size() + 32);
    for (int i = 15; i >= 0; --i) { out[14 + i] = hex[a & 15]; a >>= 4; out[30 + i] = hex[b & 15]; b >>= 4; }
    return out + ".json";
}
bool safeSettings(const fs::path& path, workspace::Project& store, std::string& error) {
    std::error_code ec; auto absolute = fs::absolute(path, ec).lexically_normal(); auto canonical = fs::canonical(absolute, ec);
    if (ec || absolute != canonical || !fs::is_directory(canonical, ec) || ec) { error = "Settings directory must exist without symlink components."; return false; }
#if !defined(_WIN32)
    for (auto current = canonical; !current.empty(); current = current.parent_path()) {
        struct stat st{};
        if (::lstat(current.c_str(), &st) || !S_ISDIR(st.st_mode) ||
            (current == canonical && st.st_uid != ::geteuid()) ||
            (current != canonical && st.st_uid != ::geteuid() && st.st_uid != 0) ||
            ((st.st_mode & 0022) && !(current != canonical && st.st_uid == 0 && (st.st_mode & S_ISVTX)))) {
            error = "Unsafe settings directory ownership or permissions."; return false;
        }
        if (current == current.root_path()) break;
    }
#endif
    bool created = false; store = workspace::Project::openOrCreate(canonical, created, error, false, false); return error.empty();
}
bool safeFile(const fs::path& path, bool& exists, std::string& error) {
    std::error_code ec; auto status = fs::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory || (!ec && status.type() == fs::file_type::not_found)) { exists = false; return true; }
    if (ec || status.type() != fs::file_type::regular) { error = "History is not a regular file."; return false; }
#if !defined(_WIN32)
    struct stat st{}; if (::lstat(path.c_str(), &st) || !S_ISREG(st.st_mode) || st.st_uid != ::geteuid() || st.st_nlink != 1 || (st.st_mode & 0022)) { error = "Unsafe history file ownership or permissions."; return false; }
#endif
    exists = true; return true;
}
bool safeText(const std::string& value, std::size_t max) {
    if (value.empty() || value.size() > max) return false;
    for (unsigned char c : value) if (c < 0x20 || c == 0x7f) return false;
    return true;
}
bool safeDuration(const std::string& value) {
    if (value.empty()) return true;
    if (value.size() > 32) return false;
    bool dot = false, digit = false;
    for (char c : value) { if (c >= '0' && c <= '9') digit = true; else if (c == '.' && !dot) dot = true; else return false; }
    return digit;
}
bool validateRun(const TestResults& run, std::string& error) {
    if (run.tests.size() > maxTests) { error = "Too many test results."; return false; }
    for (const auto& test : run.tests) if (!safeText(test.name, 1024) || !safeDuration(test.duration) || (test.status != TestResultStatus::Passed && test.status != TestResultStatus::Failed && test.status != TestResultStatus::Skipped)) { error = "Invalid test history entry."; return false; }
    return true;
}
const editor::JsonValue* field(const editor::JsonValue& v, const char* name, editor::JsonType type) { auto* x = v.find(name); return x && x->type == type ? x : nullptr; }
bool unique(const editor::JsonValue& v, std::size_t depth = 0) {
    if (depth > 8) return false;
    if (v.type == editor::JsonType::Object) { std::set<std::string> keys; for (const auto& c : v.children) if (!keys.insert(c.key).second || !unique(c, depth + 1)) return false; }
    if (v.type == editor::JsonType::Array) for (const auto& c : v.children) if (!unique(c, depth + 1)) return false;
    return true;
}
bool parseHistory(CTestHistorySnapshot& h, std::string& error) {
    editor::JsonError e; auto root = editor::parseJson(h.baseline, e);
    if (!root || root->type != editor::JsonType::Object || root->children.size() != 3 || !unique(*root)) { error = "Invalid history JSON."; return false; }
    auto* project = field(*root, "project", editor::JsonType::String); auto* build = field(*root, "build", editor::JsonType::String); auto* runs = field(*root, "runs", editor::JsonType::Array);
    if (!project || project->stringValue != utf8(h.projectRoot) || !build || build->stringValue != utf8(h.buildDirectory) || !runs || runs->children.size() > maxRuns) { error = "History identity mismatch or invalid runs."; return false; }
    for (const auto& item : runs->children) {
        if (item.type != editor::JsonType::Array || item.children.size() > maxTests) { error = "Invalid history run."; return false; }
        TestResults run;
        for (const auto& entry : item.children) {
            auto* name = field(entry, "name", editor::JsonType::String); auto* status = field(entry, "status", editor::JsonType::String); auto* duration = field(entry, "duration", editor::JsonType::String);
            if (entry.type != editor::JsonType::Object || entry.children.size() != 3 || !name || !status || !duration) { error = "Invalid history entry."; return false; }
            TestResult test; test.name = name->stringValue; test.duration = duration->stringValue;
            if (status->stringValue == "passed") test.status = TestResultStatus::Passed; else if (status->stringValue == "failed") test.status = TestResultStatus::Failed; else if (status->stringValue == "skipped") test.status = TestResultStatus::Skipped; else { error = "Invalid history status."; return false; }
            run.tests.push_back(std::move(test));
        }
        if (!validateRun(run, error)) return false;
        h.runs.push_back(std::move(run));
    }
    return true;
}
std::string serialize(const CTestHistorySnapshot& h) {
    std::string out = "{\"project\":" + debug::jsonString(utf8(h.projectRoot)) + ",\"build\":" + debug::jsonString(utf8(h.buildDirectory)) + ",\"runs\":[";
    bool firstRun = true;
    for (const auto& run : h.runs) {
        if (!firstRun) out += ',';
        firstRun = false; out += '['; bool first = true;
        for (const auto& t : run.tests) { if (!first) out += ','; first = false; const char* status = t.status == TestResultStatus::Passed ? "passed" : t.status == TestResultStatus::Failed ? "failed" : "skipped"; out += "{\"name\":" + debug::jsonString(t.name) + ",\"status\":" + debug::jsonString(status) + ",\"duration\":" + debug::jsonString(t.duration) + "}"; }
        out += ']';
    }
    return out + "]}\n";
}
}
bool loadCTestHistory(const fs::path& settings, const fs::path& project, const fs::path& build, CTestHistorySnapshot& history, std::string& error) {
    history = {}; error.clear(); std::error_code ec; CTestHistorySnapshot h;
    h.projectRoot = fs::canonical(project, ec); if (ec || !fs::is_directory(h.projectRoot, ec) || ec) { error = "Project directory is unavailable."; return false; }
    h.buildDirectory = fs::canonical(build, ec); if (ec || !fs::is_directory(h.buildDirectory, ec) || ec) { error = "Build directory is unavailable."; return false; }
    workspace::Project store; if (!safeSettings(settings, store, error)) return false;
    h.settingsRoot = store.root(); h.filename = filename(h.projectRoot, h.buildDirectory);
    if (!safeFile(h.settingsRoot / h.filename, h.exists, error)) return false;
    if (h.exists && (!store.readFileBounded(h.filename, h.baseline, error, maxBytes) || !parseHistory(h, error))) return false;
    history = std::move(h); return true;
}
bool appendCTestHistory(const fs::path& settings, CTestHistorySnapshot& history, const TestResults& run, std::string& error) {
    error.clear(); if (!validateRun(run, error)) return false;
    std::error_code ec;
    if (history.filename != filename(history.projectRoot, history.buildDirectory) || fs::canonical(history.projectRoot, ec) != history.projectRoot || ec || fs::canonical(history.buildDirectory, ec) != history.buildDirectory || ec) { error = "Invalid history snapshot or directory changed."; return false; }
    workspace::Project store; if (!safeSettings(settings, store, error) || store.root() != history.settingsRoot) { if (error.empty()) error = "History settings directory changed."; return false; }
    bool exists = false; if (!safeFile(store.root() / history.filename, exists, error)) return false;
    if (exists != history.exists) { error = "History changed; reload before saving."; return false; }
    // Reparse the opaque baseline rather than trusting mutable snapshot runs.
    CTestHistorySnapshot next; next.projectRoot = history.projectRoot; next.buildDirectory = history.buildDirectory; next.baseline = history.baseline;
    if (history.exists && !parseHistory(next, error)) return false;
    TestResults sanitized; for (const auto& t : run.tests) { TestResult entry; entry.name = t.name; entry.status = t.status; entry.duration = t.duration; sanitized.tests.push_back(std::move(entry)); }
    next.runs.push_back(std::move(sanitized)); if (next.runs.size() > maxRuns) next.runs.erase(next.runs.begin());
    auto content = serialize(next); if (content.size() > maxBytes) { error = "History exceeds size limit."; return false; }
    if (!store.writeFile(history.filename, content, error, history.exists ? &history.baseline : nullptr, !history.exists)) return false;
    history.runs = std::move(next.runs); history.baseline = std::move(content); history.exists = true; return true;
}
}
