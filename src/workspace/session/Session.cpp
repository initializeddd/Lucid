#include "workspace/session/Session.hpp"
#include "workspace/Project.hpp"
#include "debug/Protocol.hpp"
#include "editor/json/Json.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace lucid::workspace::session {
namespace {
using editor::JsonType; using editor::JsonValue;
bool stringField(const JsonValue& value, const char* key, std::string& result) {
    const auto* item = value.find(key); if (!item || item->type != JsonType::String || item->stringValue.size() > 4096) return false;
    result = item->stringValue; return result.find('\0') == std::string::npos;
}
bool numberField(const JsonValue& value, const char* key, std::size_t& result) {
    const auto* item = value.find(key); if (!item || item->type != JsonType::Number || !std::isfinite(item->numberValue) || item->numberValue < 0 || item->numberValue > 100000000 || std::floor(item->numberValue) != item->numberValue) return false;
    result = static_cast<std::size_t>(item->numberValue); return true;
}
bool safeRelative(std::string_view value) {
    if (value.empty() || value.find('\0') != std::string_view::npos) return false;
    const std::filesystem::path path{std::string(value)};
    if (path.is_absolute() || path.has_root_name()) return false;
    for (const auto& part : path) if (part == "..") return false;
    return path.lexically_normal() != ".";
}
}
bool write(const std::filesystem::path& directory, const State& state, std::string& error) {
    if (directory.empty() || state.recentProjects.size() > 8 || state.files.size() > 128 || state.project.size() > 4096) { error = "Session data is invalid or too large"; return false; }
    std::string data = "{\"version\":1,\"project\":" + debug::jsonString(state.project) + ",\"recent\":[";
    for (std::size_t i = 0; i < state.recentProjects.size(); ++i) {
        if (state.recentProjects[i].size() > 4096) { error = "Project path is too long"; return false; }
        if (i) data += ',';
        data += debug::jsonString(state.recentProjects[i]);
    }
    data += "],\"files\":[";
    for (std::size_t i = 0; i < state.files.size(); ++i) {
        const auto& file = state.files[i]; if (!safeRelative(file.path) || file.path.size() > 4096 || file.line > 100000000 || file.column > 100000000) { error = "Invalid session file"; return false; }
        if (i) data += ',';
        data += "{\"path\":" + debug::jsonString(file.path) + ",\"line\":" + std::to_string(file.line) + ",\"column\":" + std::to_string(file.column) + ",\"scroll\":" + std::to_string(std::max(file.firstLine, 0)) + "}";
    }
    data += "]}";
    bool created = false; auto project = Project::openOrCreate(directory, created, error);
    return error.empty() && project.writeFile("session.json", data, error);
}
bool read(const std::filesystem::path& directory, State& state, std::string& error) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(directory / "session.json", ec)) { error = "No saved session"; return false; }
    bool created = false; auto project = Project::openOrCreate(directory, created, error);
    std::string data; if (!error.empty() || !project.readFile("session.json", data, error)) return false;
    editor::JsonError parseError; auto root = editor::parseJson(data, parseError);
    if (!root || root->type != JsonType::Object) { error = "Invalid session file"; return false; }
    const auto* version = root->find("version"); const auto* recent = root->find("recent"); const auto* files = root->find("files");
    State next;
    if (!version || version->type != JsonType::Number || version->numberValue != 1 || !stringField(*root, "project", next.project) || !recent || recent->type != JsonType::Array || recent->children.size() > 8 || !files || files->type != JsonType::Array || files->children.size() > 128) { error = "Invalid session file"; return false; }
    for (const auto& item : recent->children) {
        if (item.type != JsonType::String || item.stringValue.size() > 4096 || item.stringValue.find('\0') != std::string::npos) { error = "Invalid recent project"; return false; }
        next.recentProjects.push_back(item.stringValue);
    }
    for (const auto& item : files->children) {
        recovery::FileContext file; std::size_t scroll = 0;
        if (item.type != JsonType::Object || !stringField(item, "path", file.path) || !safeRelative(file.path) || !numberField(item, "line", file.line) || !numberField(item, "column", file.column) || !numberField(item, "scroll", scroll)) { error = "Invalid session file context"; return false; }
        file.firstLine = static_cast<int>(scroll); next.files.push_back(std::move(file));
    }
    state = std::move(next); error.clear(); return true;
}
}
