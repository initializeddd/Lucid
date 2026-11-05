#include "modules/ApiRequests.hpp"
#include "editor/json/Json.hpp"
#include "workspace/Project.hpp"
#include <filesystem>
#include <set>
namespace lucid::modules {
namespace {
using editor::JsonType; using editor::JsonValue;
constexpr std::size_t maxInput = 1024 * 1024, maxOutput = 2 * 1024 * 1024, maxRequests = 100, maxHeaders = 100;
const JsonValue* field(const JsonValue& v, const char* name, JsonType type) {
    auto* child = v.find(name); return child && child->type == type ? child : nullptr;
}
bool validName(std::string_view s) {
    if (s.empty() || s.size() > 128) return false;
    for (unsigned char c : s) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false;
    return true;
}
bool substitute(std::string_view input, const std::map<std::string, std::string>& variables, std::string& output, std::string& error,
                const std::set<std::string>* secretNames = nullptr,
                std::vector<std::size_t>* secretStarts = nullptr) {
    output.clear();
    for (std::size_t i = 0; i < input.size();) {
        if (input.substr(i, 2) == "{{") {
            auto end = input.find("}}", i + 2);
            if (end == std::string_view::npos) { error = "Unclosed environment variable."; return false; }
            auto key = input.substr(i + 2, end - i - 2);
            if (!validName(key)) { error = "Invalid environment variable name."; return false; }
            auto it = variables.find(std::string(key));
            if (it == variables.end()) { error = "Unresolved environment variable: " + std::string(key); return false; }
            if (it->second.size() > maxOutput - output.size()) { error = "Resolved request is too large."; return false; }
            const auto start = output.size(); output += it->second;
            if (secretNames && secretStarts && secretNames->count(it->first)) secretStarts->push_back(start);
            i = end + 2;
        } else {
            if (input.substr(i, 2) == "}}") { error = "Unexpected variable closing marker."; return false; }
            if (output.size() == maxOutput) { error = "Resolved request is too large."; return false; }
            output += input[i++];
        }
    }
    return true;
}
bool noControls(std::string_view text) { for (unsigned char c : text) if (c < 0x20 || c == 0x7f) return false; return true; }
bool headerName(std::string_view text) {
    if (text.empty() || text.size() > 256) return false;
    for (unsigned char c : text) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || std::string_view("!#$%&'*+-.^_`|~").find(c) != std::string_view::npos)) return false;
    return true;
}
bool uniqueKeys(const JsonValue& v, std::size_t depth = 0) {
    if (depth > 32) return false;
    if (v.type == JsonType::Object) {
        std::set<std::string> keys;
        for (const auto& child : v.children) if (!keys.insert(child.key).second || !uniqueKeys(child, depth + 1)) return false;
    } else if (v.type == JsonType::Array) for (const auto& child : v.children) if (!uniqueKeys(child, depth + 1)) return false;
    return true;
}
bool validNumber(std::string_view text) {
    if (text.empty() || text.size() > 128) return false;
    std::size_t i = 0;
    if (text[i] == '-') { if (++i == text.size()) return false; }
    if (text[i] == '0') ++i;
    else {
        if (text[i] < '1' || text[i] > '9') return false;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9') ++i;
    }
    if (i < text.size() && text[i] == '.') {
        ++i; const auto start = i;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9') ++i;
        if (start == i) return false;
    }
    if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
        ++i; if (i < text.size() && (text[i] == '+' || text[i] == '-')) ++i;
        const auto start = i;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9') ++i;
        if (start == i) return false;
    }
    return i == text.size();
}
}
bool parseApiEnvironments(std::string_view json, std::vector<ApiEnvironment>& environments, std::string& error) {
    environments.clear(); error.clear();
    if (json.size() > maxInput) { error = "Environment file exceeds size limit."; return false; }
    editor::JsonError jsonError; const auto root = editor::parseJson(json, jsonError);
    if (!root || root->type != JsonType::Object || !uniqueKeys(*root) || root->children.size() != 1) {
        error = "Invalid environment file."; return false;
    }
    const auto* list = field(*root, "environments", JsonType::Array);
    if (!list || list->children.size() > 32) { error = "Invalid environment list or too many environments."; return false; }
    std::vector<ApiEnvironment> result; std::set<std::string> names; std::size_t total = 0;
    for (const auto& item : list->children) {
        const auto* name = field(item, "name", JsonType::String); const auto* variables = field(item, "variables", JsonType::Object);
        if (item.type != JsonType::Object || item.children.size() != 2 || !name || name->stringValue.empty() ||
            name->stringValue.size() > 128 || !noControls(name->stringValue) || !names.insert(name->stringValue).second ||
            !variables || variables->children.size() > 128) { error = "Invalid or duplicate environment."; return false; }
        ApiEnvironment environment; environment.name = name->stringValue;
        for (const auto& entry : variables->children) {
            const auto* type = field(entry, "type", JsonType::String); const auto* value = entry.find("value"); const auto* secret = entry.find("secret");
            if (!validName(entry.key) || entry.type != JsonType::Object || !type || !value ||
                (entry.children.size() != 2 && entry.children.size() != 3) ||
                (secret && secret->type != JsonType::Bool) || (entry.children.size() == 3 && !secret)) {
                error = "Invalid environment variable."; return false;
            }
            ApiEnvironmentVariable variable;
            variable.secret = secret && secret->boolValue;
            if (type->stringValue == "string" && value->type == JsonType::String) variable.value = value->stringValue;
            else if (type->stringValue == "number" && value->type == JsonType::String && validNumber(value->stringValue)) variable.value = value->stringValue;
            else if (type->stringValue == "boolean" && value->type == JsonType::Bool) variable.value = value->boolValue ? "true" : "false";
            else { error = "Invalid environment variable type or value."; return false; }
            if (variable.value.size() > 256U * 1024U || variable.value.size() > maxInput - total) {
                error = "Environment values exceed size limit."; return false;
            }
            total += variable.value.size(); environment.variables.emplace(entry.key, std::move(variable));
        }
        result.push_back(std::move(environment));
    }
    environments = std::move(result); return true;
}
static bool parseApiRequestsImpl(std::string_view json, const std::map<std::string, std::string>& variables,
                                 const std::set<std::string>* secretNames, std::vector<ApiRequest>& requests, std::string& error) {
    requests.clear(); error.clear();
    if (json.size() > maxInput) { error = "Collection exceeds size limit."; return false; }
    editor::JsonError jsonError; auto root = editor::parseJson(json, jsonError);
    if (!root || root->type != JsonType::Object || !uniqueKeys(*root)) { error = "Invalid collection JSON."; return false; }
    auto* list = field(*root, "requests", JsonType::Array);
    if (!list || list->children.size() > maxRequests) { error = "Invalid request list or too many requests."; return false; }
    std::vector<ApiRequest> result; std::size_t total = 0;
    for (const auto& item : list->children) {
        auto* name = field(item, "name", JsonType::String); auto* method = field(item, "method", JsonType::String); auto* url = field(item, "url", JsonType::String);
        if (item.type != JsonType::Object || !name || name->stringValue.empty() || name->stringValue.size() > 256 || !noControls(name->stringValue) || !method || !url) { error = "Invalid request fields."; return false; }
        ApiRequest request; request.name = name->stringValue; request.method = method->stringValue;
        if (request.method.empty() || request.method.size() > 16) { error = "Invalid HTTP method."; return false; }
        for (char c : request.method) if (c < 'A' || c > 'Z') { error = "Invalid HTTP method."; return false; }
        std::vector<std::size_t> secretStarts;
        if (!substitute(url->stringValue, variables, request.url, error, secretNames, &secretStarts)) return false;
        if ((!request.url.starts_with("https://") && !request.url.starts_with("http://")) || !noControls(request.url) || request.url.find(' ') != std::string::npos || request.url.size() > 8192) { error = "Invalid HTTP URL."; return false; }
        auto authorityStart = request.url.find("://") + 3;
        auto authorityEnd = request.url.find_first_of("/?#", authorityStart);
        if (authorityStart >= request.url.size() || authorityEnd == authorityStart || request.url.substr(authorityStart, authorityEnd - authorityStart).find('@') != std::string::npos) { error = "Invalid HTTP URL."; return false; }
        if (authorityEnd == std::string::npos) authorityEnd = request.url.size();
        for (const auto start : secretStarts) if (start <= authorityEnd) { error = "Secret variables cannot be used in URL scheme or authority."; return false; }
        if (auto* headers = item.find("headers")) {
            if (headers->type != JsonType::Object || headers->children.size() > maxHeaders) { error = "Invalid headers."; return false; }
            std::set<std::string> names;
            for (const auto& h : headers->children) {
                if (!headerName(h.key) || h.type != JsonType::String) { error = "Invalid header."; return false; }
                std::string lower = h.key; for (char& c : lower) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
                if (!names.insert(lower).second) { error = "Duplicate header name."; return false; }
                std::string value; if (!substitute(h.stringValue, variables, value, error)) return false;
                if (!noControls(value)) { error = "Invalid header value."; return false; }
                total += h.key.size() + value.size();
                if (total > maxOutput) { error = "Resolved collection is too large."; return false; }
                request.headers.emplace(h.key, std::move(value));
            }
        }
        if (auto* body = item.find("body")) {
            if (body->type != JsonType::String) { error = "Body must be a string."; return false; }
            if (!substitute(body->stringValue, variables, request.body, error)) return false;
        }
        total += request.name.size() + request.method.size() + request.url.size() + request.body.size();
        if (total > maxOutput) { error = "Resolved collection is too large."; return false; }
        result.push_back(std::move(request));
    }
    requests = std::move(result); return true;
}
bool parseApiRequests(std::string_view json, const std::map<std::string, std::string>& variables,
                      std::vector<ApiRequest>& requests, std::string& error) {
    return parseApiRequestsImpl(json, variables, nullptr, requests, error);
}
bool parseApiRequestsFromEnvironment(std::string_view json, const ApiEnvironment& environment,
                                    std::vector<ApiRequest>& requests, std::string& error) {
    requests.clear(); error.clear();
    if (environment.variables.size() > 128) { error = "Too many environment variables."; return false; }
    std::map<std::string, std::string> variables; std::set<std::string> secretNames; std::size_t total = 0;
    for (const auto& [name, variable] : environment.variables) {
        if (!validName(name) || variable.value.size() > 256U * 1024U || variable.value.size() > maxInput - total) {
            error = "Invalid environment variable."; return false;
        }
        total += variable.value.size(); variables.emplace(name, variable.value);
        if (variable.secret) secretNames.insert(name);
    }
    return parseApiRequestsImpl(json, variables, &secretNames, requests, error);
}
bool saveApiResponseBody(workspace::Project& project, std::string_view relativePath,
                         const ApiResponse& response, std::string& error, const std::atomic_bool* cancel) {
    error.clear();
    if (cancel && cancel->load()) { error = "Response export cancelled"; return false; }
    if (!response.error.empty() || response.status < 100 || response.status > 599 || response.body.size() > 4U * 1024U * 1024U) {
        error = "Only complete bounded HTTP response bodies can be exported"; return false;
    }
    if (relativePath.empty() || relativePath.size() > 4096 || relativePath.find('\0') != std::string_view::npos) { error = "Invalid export path"; return false; }
    std::filesystem::path input;
    try { input = std::filesystem::path(std::u8string(relativePath.begin(), relativePath.end())); }
    catch (const std::exception&) { error = "Invalid export path"; return false; }
    if (input.is_absolute() || input.has_root_name() || input.has_root_directory() || input.filename().empty()) { error = "Export path must name a new project file"; return false; }
    std::filesystem::path current = project.root();
    for (auto it = input.begin(); it != input.end(); ++it) {
        if (*it == "." || *it == ".." || it->empty()) { error = "Invalid export path component"; return false; }
        current /= *it; auto next = it; ++next; const bool last = next == input.end();
        std::error_code ec; const auto status = std::filesystem::symlink_status(current, ec);
        if (last) {
            if (ec && ec != std::errc::no_such_file_or_directory) { error = "Could not inspect export path"; return false; }
            if (status.type() != std::filesystem::file_type::not_found) { error = "Export destination already exists"; return false; }
        } else if (ec || status.type() != std::filesystem::file_type::directory) {
            error = "Export parent must be an existing directory without symlinks"; return false;
        }
    }
    return project.writeFile(relativePath, response.body, error, nullptr, true, cancel);
}

}
