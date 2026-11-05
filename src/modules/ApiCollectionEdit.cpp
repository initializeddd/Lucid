#include "modules/ApiRequests.hpp"
#include "debug/Protocol.hpp"
#include "editor/json/Json.hpp"
#include "text/Utf8.hpp"
#include <set>

namespace lucid::modules {
namespace {
bool unique(const editor::JsonValue& value, std::size_t depth = 0) {
    if (depth > 32) return false;
    if (value.type == editor::JsonType::Object) {
        std::set<std::string> keys;
        for (const auto& child : value.children) if (!keys.insert(child.key).second || !unique(child, depth + 1)) return false;
    } else if (value.type == editor::JsonType::Array) {
        for (const auto& child : value.children) if (!unique(child, depth + 1)) return false;
    }
    return true;
}
bool validName(std::string_view name) {
    if (name.empty() || name.size() > 256) return false;
    for (std::size_t i = 0; i < name.size();) {
        const auto start = i; const auto cp = text::nextUtf8(name, i);
        if (cp < 32 || cp == 127 || (cp == U'\ufffd' && i - start == 1 && static_cast<unsigned char>(name[start]) >= 0x80)) return false;
    }
    return true;
}
// JSON is parsed before this is called, so this scanner only locates its end.
bool valueEnd(std::string_view source, std::size_t start, std::size_t& end) {
    if (start >= source.size()) return false;
    if (source[start] == '"') {
        bool escaped = false;
        for (std::size_t i = start + 1; i < source.size(); ++i) {
            if (escaped) escaped = false;
            else if (source[i] == '\\') escaped = true;
            else if (source[i] == '"') { end = i + 1; return true; }
        }
        return false;
    }
    if (source[start] != '{' && source[start] != '[') return false;
    std::size_t depth = 0; bool quoted = false, escaped = false;
    for (std::size_t i = start; i < source.size(); ++i) {
        const char c = source[i];
        if (quoted) { if (escaped) escaped = false; else if (c == '\\') escaped = true; else if (c == '"') quoted = false; }
        else if (c == '"') quoted = true;
        else if (c == '{' || c == '[') ++depth;
        else if (c == '}' || c == ']') { if (!depth) return false; if (--depth == 0) { end = i + 1; return true; } }
    }
    return false;
}
bool collectionRoot(std::string_view text, editor::JsonValue& root, std::string& error) {
    if (text.size() > 1024U * 1024U) { error = "Collection exceeds size limit."; return false; }
    editor::JsonError parseError; auto json = editor::parseJson(text, parseError);
    if (!json || json->type != editor::JsonType::Object || !unique(*json)) { error = "Invalid collection JSON."; return false; }
    const auto* requests = json->find("requests");
    if (!requests || requests->type != editor::JsonType::Array || requests->children.size() > 100) { error = "Invalid request list."; return false; }
    for (const auto& item : requests->children) if (item.type != editor::JsonType::Object) { error = "Invalid request entry."; return false; }
    root = std::move(*json); return true;
}
}
bool listApiRequestTemplates(std::string_view collection, std::vector<std::string>& names, std::string& error) {
    names.clear(); error.clear(); editor::JsonValue root; if (!collectionRoot(collection, root, error)) return false;
    std::vector<std::string> result;
    for (const auto& request : root.find("requests")->children) {
        const auto* name = request.find("name");
        if (!name || name->type != editor::JsonType::String || !validName(name->stringValue)) { error = "Every request needs a valid name."; return false; }
        result.push_back(name->stringValue);
    }
    names = std::move(result); return true;
}
bool renameApiRequest(std::string_view collection, std::size_t index, std::string_view newName, ApiCollectionEdit& edit, std::string& error) {
    edit = {}; error.clear();
    if (!validName(newName)) { error = "Request name must be valid text of 1–256 bytes without controls."; return false; }
    editor::JsonValue root; if (!collectionRoot(collection, root, error)) return false;
    const auto& requests = root.find("requests")->children;
    if (index >= requests.size()) { error = "Request no longer exists."; return false; }
    const auto* name = requests[index].find("name"); std::size_t end = 0;
    if (!name || name->type != editor::JsonType::String || !valueEnd(collection, name->byteOffset, end)) { error = "Request has no editable name."; return false; }
    edit = {name->byteOffset, end - name->byteOffset, debug::jsonString(newName)}; return true;
}
bool removeApiRequest(std::string_view collection, std::size_t index, ApiCollectionEdit& edit, std::string& error) {
    edit = {}; error.clear(); editor::JsonValue root; if (!collectionRoot(collection, root, error)) return false;
    const auto& requests = root.find("requests")->children;
    if (index >= requests.size()) { error = "Request no longer exists."; return false; }
    std::size_t start = requests[index].byteOffset, end = 0;
    if (!valueEnd(collection, start, end)) { error = "Cannot locate request."; return false; }
    if (index + 1 < requests.size()) end = requests[index + 1].byteOffset;
    else if (index > 0 && !valueEnd(collection, requests[index - 1].byteOffset, start)) { error = "Cannot locate previous request."; return false; }
    if (end < start || end > collection.size()) { error = "Invalid request boundaries."; return false; }
    edit = {start, end - start, {}}; return true;
}
bool appendApiRequestTemplate(std::string_view collection, std::string_view name, ApiCollectionEdit& edit, std::string& error) {
    edit = {}; error.clear();
    if (!validName(name)) { error = "Request name must be valid text of 1–256 bytes without controls."; return false; }
    editor::JsonValue root; if (!collectionRoot(collection, root, error)) return false;
    const auto* requests = root.find("requests");
    if (requests->children.size() >= 100) { error = "Collection already has 100 requests."; return false; }
    std::size_t end = 0;
    if (!valueEnd(collection, requests->byteOffset, end) || !end || collection[end - 1] != ']') { error = "Cannot locate request list."; return false; }
    std::size_t position = end - 1; std::string prefix;
    if (!requests->children.empty()) {
        if (!valueEnd(collection, requests->children.back().byteOffset, position)) { error = "Cannot locate final request."; return false; }
        prefix = ",";
        const auto line = collection.rfind('\n', requests->children.back().byteOffset);
        if (line != std::string_view::npos) {
            auto indent = collection.substr(line + 1, requests->children.back().byteOffset - line - 1);
            if (indent.find_first_not_of(" \t") == std::string_view::npos) prefix += "\n" + std::string(indent);
        }
    }
    const std::string value = prefix + "{\"name\":" + debug::jsonString(name) + ",\"method\":\"GET\",\"url\":\"https://example.com/\"}";
    if (value.size() > 1024U * 1024U - collection.size()) { error = "Collection exceeds size limit."; return false; }
    edit = {position, 0, value}; return true;
}
}
