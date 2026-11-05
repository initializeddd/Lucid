#include "modules/DesignTokens.hpp"
#include "editor/json/Json.hpp"
#include <charconv>
#include <cmath>
#include <set>

namespace lucid::modules {
namespace {
constexpr std::size_t maxSize = 4U * 1024U * 1024U, maxTokens = 10000, maxWarnings = 1000;
void warn(DesignTokenReport& r, std::string message) { if (r.warnings.size() < maxWarnings) r.warnings.push_back(std::move(message)); else r.warningsTruncated = true; }
std::string_view trim(std::string_view s) {
    const auto white = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!s.empty() && white(s.front())) s.remove_prefix(1);
    while (!s.empty() && white(s.back())) s.remove_suffix(1);
    return s;
}
bool primitive(const editor::JsonValue& v, std::string_view source, std::string& value, std::string& type) {
    using editor::JsonType;
    if (v.type == JsonType::String) { if (v.stringValue.size() > 4096) return false; value = v.stringValue; type = "string"; return true; }
    if (v.type == JsonType::Bool) { value = v.boolValue ? "true" : "false"; type = "boolean"; return true; }
    if (v.type == JsonType::Number && std::isfinite(v.numberValue)) {
        auto end = v.byteOffset;
        while (end < source.size() && ((source[end] >= '0' && source[end] <= '9') || source[end] == '-' || source[end] == '+' || source[end] == '.' || source[end] == 'e' || source[end] == 'E')) ++end;
        if (end == v.byteOffset || end - v.byteOffset > 4096) return false;
        value.assign(source.substr(v.byteOffset, end-v.byteOffset)); type = "number"; return true;
    }
    return false;
}
bool uniqueJsonKeys(const editor::JsonValue& value, std::size_t depth = 0) {
    if (depth > 64) return false;
    std::set<std::string> keys;
    for (const auto& child : value.children) {
        if (value.type == editor::JsonType::Object && !keys.insert(child.key).second) return false;
        if (!uniqueJsonKeys(child, depth + 1)) return false;
    }
    return true;
}
bool walk(const editor::JsonValue& node, std::string_view source, const std::string& path, DesignTokenReport& report, std::size_t depth, std::string_view inheritedType = {}) {
    using editor::JsonType;
    if (depth > 64) { warn(report, "JSON nesting exceeds 64 levels at " + path); return true; }
    if (node.type == JsonType::Object) {
        if (const auto* declaredType = node.find("$type")) {
            if (declaredType->type != JsonType::String || declaredType->stringValue.size() > 4096) { warn(report, "Unsupported token type at " + path); return true; }
            inheritedType = declaredType->stringValue;
        }
        if (const auto* tokenValue = node.find("$value")) {
            std::string value, type;
            if (path.empty() || !primitive(*tokenValue, source, value, type)) { warn(report, "Unsupported token value at " + path); return true; }
            if (!inheritedType.empty()) type = inheritedType;
            if (report.tokens.size() >= maxTokens) return false;
            report.tokens.push_back({path, value, type, 0, tokenValue->byteOffset}); return true;
        }
        for (const auto& child : node.children) {
            if (child.key == "$type") continue;
            if (child.key.empty() || child.key.front() == '$' || child.key.find('.') != std::string::npos) {
                warn(report, "Unsupported JSON key at " + path); continue;
            }
            if (child.key.size() > 4096 || path.size() > 4096 - child.key.size() || (!path.empty() && path.size() + child.key.size() == 4096)) { warn(report, "JSON token path exceeds 4096 bytes"); continue; }
            if (!walk(child, source, path.empty() ? child.key : path + "." + child.key, report, depth + 1, inheritedType)) return false;
        }
        return true;
    }
    std::string value, type;
    if (path.empty() || !primitive(node, source, value, type)) { warn(report, "Unsupported JSON value at " + path); return true; }
    if (report.tokens.size() >= maxTokens) return false;
    report.tokens.push_back({path, value, type, 0, node.byteOffset}); return true;
}
}
bool extractJsonDesignTokens(std::string_view text, DesignTokenReport& report, std::string& error) {
    report = {}; error.clear();
    if (text.size() > maxSize) { error = "Design token document exceeds size limit"; return false; }
    editor::JsonError parseError; auto json = editor::parseJson(text, parseError);
    if (!json || json->type != editor::JsonType::Object || !uniqueJsonKeys(*json)) { error = "Invalid design token JSON object, duplicate key, or excessive nesting"; return false; }
    DesignTokenReport result;
    if (!walk(*json, text, "", result, 0)) { error = "Too many design tokens"; return false; }
    report = std::move(result); return true;
}
bool extractCssDesignTokens(std::string_view text, DesignTokenReport& report, std::string& error) {
    report = {}; error.clear();
    if (text.size() > maxSize) { error = "Design token document exceeds size limit"; return false; }
    DesignTokenReport result; std::size_t line = 0, pos = 0; bool comment = false; int braces = 0;
    std::set<std::string> names;
    while (pos < text.size()) {
        ++line; const auto lineStart = pos; auto end = text.find('\n', pos); if (end == std::string_view::npos) end = text.size();
        const auto raw = text.substr(pos, end - pos); pos = end == text.size() ? end : end + 1;
        // Comments and strings make declaration boundaries ambiguous. Skip the
        // whole line, but track block comments so their contents are never tokens.
        std::string clean; bool hadComment = comment;
        for (std::size_t i = 0; i < raw.size(); ++i) {
            if (comment) { hadComment = true; if (raw[i] == '*' && i + 1 < raw.size() && raw[i + 1] == '/') { comment = false; ++i; } }
            else if (raw[i] == '/' && i + 1 < raw.size() && raw[i + 1] == '*') { hadComment = true; comment = true; ++i; }
            else clean.push_back(raw[i]);
        }
        auto s = trim(clean);
        if (hadComment && (s.find('{') != std::string_view::npos || s.find('}') != std::string_view::npos)) warn(result, "CSS brace with comment not interpreted on line " + std::to_string(line));
        if (s.find_first_of("\"'\\") != std::string_view::npos) {
            if (s.find("--") != std::string_view::npos) warn(result, "Unsupported CSS syntax on line " + std::to_string(line));
            continue;
        }
        // Handle simple selector and closing-brace lines, but no inline rules.
        if (!hadComment && s.size() && s.back() == '{' && s.find_first_of("{}") == s.size() - 1) { ++braces; continue; }
        if (!hadComment && (s == "}" || s == "};")) { if (braces > 0) --braces; else warn(result, "Unmatched CSS brace on line " + std::to_string(line)); continue; }
        if (s.find("--") == std::string_view::npos) continue;
        if (hadComment || braces <= 0 || !s.starts_with("--") || s.back() != ';' || s.find_first_of("{}") != std::string_view::npos) {
            warn(result, "Unsupported CSS syntax on line " + std::to_string(line)); continue;
        }
        auto colon = s.find(':');
        if (colon == std::string_view::npos || colon <= 2 || s.find(';') != s.size() - 1) { warn(result, "Unsupported CSS declaration on line " + std::to_string(line)); continue; }
        auto name = trim(s.substr(0, colon)); auto value = trim(s.substr(colon + 1, s.size() - colon - 2));
        bool valid = !value.empty();
        for (char c : name.substr(2)) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) valid = false;
        if (!valid) { warn(result, "Unsupported CSS declaration on line " + std::to_string(line)); continue; }
        if (result.tokens.size() >= maxTokens) { error = "Too many design tokens"; return false; }
        if (!names.insert(std::string(name)).second) warn(result, "Duplicate CSS custom property " + std::string(name) + "; cascade not resolved");
        result.tokens.push_back({std::string(name), std::string(value), "css", line, lineStart + raw.find(name)});
    }
    if (comment) warn(result, "Unterminated CSS comment");
    if (braces) warn(result, "Unclosed CSS block");
    report = std::move(result); return true;
}
bool findCssTokenUsages(std::string_view text, CssTokenUsageReport& report, std::string& error) {
    report = {}; error.clear();
    if (text.size() > maxSize) { error = "CSS document exceeds size limit"; return false; }
    std::size_t line = 1, column = 1;
    bool comment = false, escape = false; char quote = 0;
    const auto ident = [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_'; };
    for (std::size_t i = 0; i < text.size();) {
        const char c = text[i];
        if (!comment && !quote && i + 4 < text.size() && text.substr(i, 4) == "var(" &&
            (i == 0 || (static_cast<unsigned char>(text[i - 1]) < 0x80 && !ident(static_cast<unsigned char>(text[i - 1])) && text[i - 1] != '\\'))) {
            std::size_t p = i + 4;
            while (p < text.size() && (text[p] == ' ' || text[p] == '\t')) ++p;
            const std::size_t start = p;
            if (text.substr(p, 2) == "--") {
                p += 2; while (p < text.size() && ident(static_cast<unsigned char>(text[p]))) ++p;
                const auto end = p;
                while (p < text.size() && (text[p] == ' ' || text[p] == '\t')) ++p;
                if (end > start + 2 && p < text.size() && (text[p] == ')' || text[p] == ',')) {
                    if (report.usages.size() == maxTokens) { report.truncated = true; return true; }
                    report.usages.push_back({std::string(text.substr(start, end - start)), line, column + start - i});
                }
            }
        }
        if (comment) {
            if (c == '*' && i + 1 < text.size() && text[i + 1] == '/') { comment = false; i += 2; column += 2; continue; }
        } else if (quote) {
            if (escape) escape = false;
            else if (c == '\\') escape = true;
            else if (c == quote) quote = 0;
        } else if (c == '/' && i + 1 < text.size() && text[i + 1] == '*') { comment = true; i += 2; column += 2; continue; }
        else if (c == '\'' || c == '"') quote = c;
        ++i; if (c == '\n') { ++line; column = 1; } else ++column;
    }
    return true;
}
}
