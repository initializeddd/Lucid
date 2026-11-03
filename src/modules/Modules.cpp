#include "modules/Modules.hpp"
#include "editor/json/Json.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>
namespace lucid::modules {
namespace {
using editor::JsonType;
bool validId(std::string_view id) {
    if (id.empty() || id.size() > 80 || id == "." || id == "..") return false;
    for (char c : id) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    return true;
}
bool stringField(const editor::JsonValue& root, const char* key, std::string& value, std::size_t limit) {
    const auto* v = root.find(key); if (!v || v->type != JsonType::String || v->stringValue.empty() || v->stringValue.size() > limit) return false;
    value = v->stringValue; return true;
}
bool boolField(const editor::JsonValue& root, const char* key, bool& value) {
    const auto* v = root.find(key); if (!v || v->type != JsonType::Bool) return false;
    value = v->boolValue; return true;
}
bool safeDirectory(const std::filesystem::path& root, std::string_view id, std::filesystem::path& path, std::string& error) {
    if (!validId(id)) { error = "Invalid module ID"; return false; }
    path = root / std::string(id); std::error_code ec;
    const auto status = std::filesystem::symlink_status(path, ec);
    if (ec || status.type() != std::filesystem::file_type::directory) { error = "Module directory is missing or is a link"; return false; }
    return true;
}
Module readModule(const std::filesystem::path& dir, std::string id) {
    Module m; m.id = std::move(id); m.name = m.id;
    std::error_code ec; auto status = std::filesystem::symlink_status(dir / "module.json", ec);
    if (ec || status.type() != std::filesystem::file_type::regular) { m.error = "Missing or unsafe module.json"; return m; }
    auto size = std::filesystem::file_size(dir / "module.json", ec);
    if (ec || size > 65536) { m.error = "Module manifest is too large"; return m; }
    std::ifstream input(dir / "module.json", std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(input)), {});
    editor::JsonError jsonError; const auto json = editor::parseJson(text, jsonError);
    if (!input || !json || json->type != JsonType::Object || !stringField(*json, "name", m.name, 128) || !stringField(*json, "version", m.version, 64)) { m.error = "Invalid module manifest"; return m; }
    const auto* p = json->find("permissions"); const auto* c = json->find("commands");
    if (!p || p->type != JsonType::Object || p->children.size() != 4 || !boolField(*p, "project", m.project) ||
        !boolField(*p, "network", m.network) || !boolField(*p, "processes", m.processes) || !boolField(*p, "settings", m.settings) ||
        !c || c->type != JsonType::Array || c->children.empty() || c->children.size() > 32) { m.error = "Invalid permissions or commands"; return m; }
    for (const auto& item : c->children) {
        Command command;
        if (item.type != JsonType::Object || !stringField(item, "title", command.title, 128) || !stringField(item, "insert", command.insert, 4096)) { m.error = "Invalid text command"; return m; }
        m.commands.push_back(std::move(command));
    }
    m.valid = true;
    const auto marker = std::filesystem::symlink_status(dir / ".enabled", ec);
    m.enabled = !ec && marker.type() == std::filesystem::file_type::directory && !m.project && !m.network && !m.processes && !m.settings;
    if (m.project || m.network || m.processes || m.settings) m.error = "Requested permissions are not supported";
    return m;
}
}
bool list(const std::filesystem::path& root, std::vector<Module>& result, std::string& error) {
    result.clear(); error.clear(); if (root.empty()) { error = "Module storage unavailable"; return false; }
    std::error_code ec; std::filesystem::create_directories(root, ec);
    if (ec) { error = "Cannot open module storage: " + ec.message(); return false; }
    std::filesystem::directory_iterator it(root, ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        const auto id = it->path().filename().string();
        const auto status = it->symlink_status(ec); if (ec) break;
        if (validId(id) && status.type() == std::filesystem::file_type::directory) result.push_back(readModule(it->path(), id));
    }
    if (ec) { error = "Cannot list modules: " + ec.message(); return false; }
    std::sort(result.begin(), result.end(), [](const Module& a, const Module& b) { return a.id < b.id; }); return true;
}
bool setEnabled(const std::filesystem::path& root, std::string_view id, bool enabled, std::string& error) {
    std::filesystem::path dir; if (!safeDirectory(root, id, dir, error)) return false;
    auto module = readModule(dir, std::string(id));
    if (enabled && (!module.valid || module.project || module.network || module.processes || module.settings)) { error = module.error.empty() ? "Module cannot be enabled" : module.error; return false; }
    std::error_code ec;
    const auto marker = dir / ".enabled";
    if (enabled) {
        if (!std::filesystem::create_directory(marker, ec) && !ec) {
            auto status = std::filesystem::symlink_status(marker, ec);
            if (ec || status.type() != std::filesystem::file_type::directory) { error = "Unsafe module state"; return false; }
        }
    } else { (void)std::filesystem::remove(marker, ec); }
    if (ec) { error = "Could not change module state: " + ec.message(); return false; }
    error.clear(); return true;
}
bool remove(const std::filesystem::path& root, std::string_view id, std::string& error) {
    std::filesystem::path dir; if (!safeDirectory(root, id, dir, error)) return false;
    std::error_code ec; std::filesystem::remove_all(dir, ec);
    if (ec) { error = "Could not remove module: " + ec.message(); return false; }
    error.clear(); return true;
}
}
