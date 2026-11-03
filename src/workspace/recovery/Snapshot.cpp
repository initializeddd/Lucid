#include "workspace/recovery/Snapshot.hpp"
#include "editor/json/Json.hpp"
#include "workspace/Project.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif
namespace lucid::workspace::recovery {
namespace {
std::string quote(std::string_view value) {
    static constexpr char hex[] = "0123456789abcdef"; std::string out = "\"";
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
        else if (c < 32) { out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
        else out += static_cast<char>(c);
    }
    return out + '"';
}
bool text(const editor::JsonValue& json, const char* name, std::string& out) {
    const auto* v = json.find(name); if (!v || v->type != editor::JsonType::String) return false;
    out = v->stringValue; return true;
}
bool number(const editor::JsonValue& json, const char* name, std::size_t& out) {
    const auto* v = json.find(name);
    if (!v || v->type != editor::JsonType::Number || v->numberValue < 0 || v->numberValue > 100000000 || std::floor(v->numberValue) != v->numberValue) return false;
    out = static_cast<std::size_t>(v->numberValue); return true;
}
bool context(const editor::JsonValue& value, FileContext& out) {
    std::size_t first = 0; if (value.type != editor::JsonType::Object || !text(value, "path", out.path) || out.path.empty() ||
        !number(value, "line", out.line) || !number(value, "column", out.column) || !number(value, "firstLine", first) || first > static_cast<std::size_t>(std::numeric_limits<int>::max())) return false;
    out.firstLine = static_cast<int>(first); return true;
}
}
bool write(const std::filesystem::path& path, const Snapshot& s, std::string& error) {
    error.clear();
    if (s.project.empty() || s.file.empty() || s.contents.size() > 16U * 1024U * 1024U || s.baseline.size() > 16U * 1024U * 1024U || s.openFiles.size() > 1024) { error = "Recovery snapshot is too large or incomplete"; return false; }
    std::error_code ec; std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) { error = "Cannot create recovery folder: " + ec.message(); return false; }
#if !defined(_WIN32)
    std::filesystem::permissions(path.parent_path(), std::filesystem::perms::owner_all, std::filesystem::perm_options::replace, ec);
    if (ec) { error = "Cannot secure recovery folder: " + ec.message(); return false; }
#endif
    std::ostringstream out;
    out << "{\"version\":1,\"project\":" << quote(s.project) << ",\"file\":" << quote(s.file) << ",\"baseline\":" << quote(s.baseline) << ",\"contents\":" << quote(s.contents)
        << ",\"line\":" << s.line << ",\"column\":" << s.column << ",\"firstLine\":" << std::max(s.firstLine, 0) << ",\"openFiles\":[";
    for (std::size_t i = 0; i < s.openFiles.size(); ++i) { const auto& f = s.openFiles[i]; if (i) out << ','; out << "{\"path\":" << quote(f.path) << ",\"line\":" << f.line << ",\"column\":" << f.column << ",\"firstLine\":" << std::max(f.firstLine,0) << '}'; }
    out << "]}";
    if (out.tellp() > 200 * 1024 * 1024) { error = "Recovery snapshot is too large"; return false; }
    bool created = false;
    auto directory = workspace::Project::openOrCreate(path.parent_path(), created, error, false);
    if (!error.empty()) return false;
    if (!directory.writeFile(path.filename().generic_string(), out.str(), error)) { error = "Could not safely write recovery snapshot: " + error; return false; }
    return true;
}
bool read(const std::filesystem::path& path, Snapshot& snapshot, std::string& error) {
    std::error_code ec; const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > 200U * 1024U * 1024U) { error = "Recovery snapshot is unavailable or too large"; return false; }
    std::ifstream input(path, std::ios::binary); if (!input) { error = "Cannot read recovery snapshot"; return false; }
    std::string data((std::istreambuf_iterator<char>(input)), {}); editor::JsonError parseError; const auto json = editor::parseJson(data, parseError);
    Snapshot next; std::size_t first = 0;
    const auto* version = json ? json->find("version") : nullptr;
    if (!json || json->type != editor::JsonType::Object || !version || version->type != editor::JsonType::Number || version->numberValue != 1 ||
        !text(*json, "project", next.project) || !text(*json, "file", next.file) || !text(*json, "baseline", next.baseline) || !text(*json, "contents", next.contents) ||
        next.project.empty() || next.file.empty() || next.baseline.size() > 16U * 1024U * 1024U || next.contents.size() > 16U * 1024U * 1024U ||
        !number(*json, "line", next.line) || !number(*json, "column", next.column) || !number(*json, "firstLine", first)) { error = "Invalid recovery snapshot"; return false; }
    next.firstLine = static_cast<int>(first);
    const auto* files = json->find("openFiles"); if (!files || files->type != editor::JsonType::Array || files->children.size() > 1024) { error = "Invalid recovery context"; return false; }
    for (const auto& item : files->children) { FileContext f; if (!context(item, f)) { error = "Invalid recovery context"; return false; } next.openFiles.push_back(std::move(f)); }
    snapshot = std::move(next); error.clear(); return true;
}
}
