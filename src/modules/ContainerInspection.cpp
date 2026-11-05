#include "modules/ContainerInspection.hpp"
#include "editor/json/Json.hpp"
#include <algorithm>
#include <set>
namespace lucid::modules {
namespace {
constexpr std::size_t maxBytes = 8U * 1024U * 1024U, maxEntries = 10000, maxWarnings = 1000;
void warn(ContainerListing& r, std::string s) { if (r.warnings.size() < maxWarnings) r.warnings.push_back(std::move(s)); }
bool stringField(const editor::JsonValue& node, const char* key, std::string& out, ContainerListing& report) {
    const auto* field = node.find(key); if (!field) return true;
    if (field->type == editor::JsonType::String) { out = field->stringValue; return true; }
    if (std::string_view(key) == "Names" && field->type == editor::JsonType::Array) {
        for (const auto& item : field->children) { if (item.type != editor::JsonType::String) { warn(report, "Unsupported Names field"); out.clear(); return true; } if (!out.empty()) out += ", "; out += item.stringValue; }
        return true;
    }
    warn(report, std::string("Unsupported field type: ") + key); return true;
}
bool add(const editor::JsonValue& value, ContainerListing& out, std::string& error) {
    if (value.type != editor::JsonType::Object || out.containers.size() >= maxEntries) { error = "Invalid or oversized container listing"; return false; }
    ContainerInfo c;
    const auto* id = value.find("ID"); const auto* podmanId = value.find("Id");
    if (id && podmanId) { error = "Ambiguous container ID"; return false; }
    stringField(value, id ? "ID" : "Id", c.id, out);
    stringField(value, "Image", c.image, out); stringField(value, "Names", c.names, out);
    stringField(value, "Status", c.status, out); stringField(value, "State", c.state, out);
    if (c.id.empty()) { error = "Container ID is missing or unsupported"; return false; }
    static const std::set<std::string> supported{"ID", "Id", "Image", "Names", "Status", "State"};
    for (const auto& child : value.children) if (!supported.count(child.key)) warn(out, "Unsupported container field: " + child.key);
    out.containers.push_back(std::move(c)); return true;
}
}
bool parseContainerListing(std::string_view input, ContainerListing& output, std::string& error) {
    output = {}; error.clear(); if (input.size() > maxBytes) { error = "Container listing exceeds size limit"; return false; }
    ContainerListing result; editor::JsonError parseError; auto root = editor::parseJson(input, parseError);
    if (root && root->type == editor::JsonType::Array) {
        for (const auto& item : root->children) if (!add(item, result, error)) return false;
    } else {
        std::size_t pos = 0;
        while (pos < input.size()) {
            auto end = input.find('\n', pos); if (end == input.npos) end = input.size();
            auto line = input.substr(pos, end-pos); pos = end == input.size() ? end : end + 1;
            if (line.find_first_not_of(" \t\r") == line.npos) continue;
            auto item = editor::parseJson(line, parseError); if (!item || !add(*item, result, error)) { if (error.empty()) error = "Invalid container JSON line"; return false; }
        }
    }
    output = std::move(result); return true;
}
ContainerLog parseContainerLog(std::string_view input, std::size_t limit) {
    limit = std::min(limit, maxBytes); ContainerLog result;
    result.text.assign(input.substr(0, limit)); result.truncated = input.size() > limit; return result;
}
}
