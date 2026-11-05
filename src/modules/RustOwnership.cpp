#include "modules/RustOwnership.hpp"
#include "editor/json/Json.hpp"
#include <cmath>
#include <set>
namespace lucid::modules {
namespace {
using editor::JsonType; using editor::JsonValue;
constexpr std::size_t maxInput = 8 * 1024 * 1024, maxLine = 1024 * 1024, maxDiagnostics = 5000, maxSpans = 200, maxNotes = 200;
const JsonValue* field(const JsonValue& v, const char* key, JsonType type) { auto* p = v.find(key); return p && p->type == type ? p : nullptr; }
bool unique(const JsonValue& value, std::size_t depth = 0) {
    if (depth > 32) return false;
    if (value.type == JsonType::Object) { std::set<std::string> keys; for (const auto& c : value.children) if (!keys.insert(c.key).second || !unique(c, depth + 1)) return false; }
    else if (value.type == JsonType::Array) for (const auto& c : value.children) if (!unique(c, depth + 1)) return false;
    return true;
}
bool known(std::string_view code) {
    static const std::set<std::string_view> codes = {"E0382", "E0499", "E0500", "E0501", "E0502", "E0503", "E0504", "E0505", "E0506", "E0507", "E0508", "E0509", "E0515", "E0521", "E0594", "E0596", "E0597", "E0716"};
    return codes.count(code) != 0;
}
bool textField(const JsonValue& v, const char* key, std::string& out, bool nullable = false) {
    auto* p = v.find(key);
    if (nullable && (!p || p->type == JsonType::Null)) return true;
    if (!p || p->type != JsonType::String || p->stringValue.size() > 65536 || p->stringValue.find('\0') != std::string::npos) return false;
    out = p->stringValue; return true;
}
bool position(const JsonValue& v, const char* key, std::size_t& out) {
    auto* p = field(v, key, JsonType::Number);
    if (!p || !std::isfinite(p->numberValue) || p->numberValue < 1 || p->numberValue > 1000000000 || std::floor(p->numberValue) != p->numberValue) return false;
    out = static_cast<std::size_t>(p->numberValue); return true;
}
std::size_t sourceCharacters(std::string_view text) {
    std::size_t count = 0;
    for (unsigned char c : text) if ((c & 0xc0U) != 0x80U) ++count;
    return count;
}
bool spans(const JsonValue& parent, std::vector<RustOwnershipSpan>& out, std::string& error) {
    auto* list = field(parent, "spans", JsonType::Array);
    if (!list || list->children.size() > maxSpans) { error = "Invalid diagnostic spans."; return false; }
    for (const auto& item : list->children) {
        RustOwnershipSpan span; auto* primary = field(item, "is_primary", JsonType::Bool);
        if (item.type != JsonType::Object || !primary || !textField(item, "file_name", span.file) || span.file.empty() ||
            !position(item, "line_start", span.lineStart) || !position(item, "column_start", span.columnStart) ||
            !position(item, "line_end", span.lineEnd) || !position(item, "column_end", span.columnEnd) ||
            span.lineEnd < span.lineStart || (span.lineEnd == span.lineStart && span.columnEnd < span.columnStart) ||
            !textField(item, "label", span.label, true) || !textField(item, "suggestion_applicability", span.applicability, true)) { error = "Invalid diagnostic span."; return false; }
        auto* replacement = item.find("suggested_replacement");
        if (replacement && replacement->type != JsonType::Null) {
            if (!textField(item, "suggested_replacement", span.replacement)) { error = "Invalid suggested replacement."; return false; }
            span.hasReplacement = true;
        }
        auto* source = item.find("text");
        if (source) {
            if (source->type != JsonType::Array || source->children.size() > 200) { error = "Invalid diagnostic source lines."; return false; }
            for (const auto& line : source->children) {
                RustOwnershipSourceLine itemLine;
                if (line.type != JsonType::Object || !textField(line, "text", itemLine.text) ||
                    !position(line, "highlight_start", itemLine.highlightStart) || !position(line, "highlight_end", itemLine.highlightEnd) ||
                    itemLine.highlightEnd < itemLine.highlightStart || itemLine.highlightEnd > sourceCharacters(itemLine.text) + 1) { error = "Invalid diagnostic source highlight."; return false; }
                span.sourceLines.push_back(std::move(itemLine));
            }
        }
        span.primary = primary->boolValue; out.push_back(std::move(span));
    }
    return true;
}
bool notes(const JsonValue& value, std::vector<RustOwnershipNote>& out, std::string& error, unsigned depth = 0) {
    if (depth > 8) { error = "Diagnostic notes are too deeply nested."; return false; }
    auto* children = field(value, "children", JsonType::Array);
    if (!children) { error = "Invalid diagnostic children."; return false; }
    for (const auto& child : children->children) {
        if (out.size() >= maxNotes || child.type != JsonType::Object) { error = "Too many or invalid diagnostic notes."; return false; }
        RustOwnershipNote note;
        if (!textField(child, "level", note.level) || !textField(child, "message", note.message) || !spans(child, note.spans, error)) { if (error.empty()) error = "Invalid diagnostic note."; return false; }
        out.push_back(std::move(note));
        if (!notes(child, out, error, depth + 1)) return false;
    }
    return true;
}
}
bool parseRustOwnershipDiagnostics(std::string_view input, std::vector<RustOwnershipDiagnostic>& diagnostics, std::string& error) {
    diagnostics.clear(); error.clear();
    if (input.size() > maxInput) { error = "Rust diagnostics exceed size limit."; return false; }
    std::vector<RustOwnershipDiagnostic> result;
    while (!input.empty()) {
        auto end = input.find('\n'); auto line = input.substr(0, end); if (end == input.npos) input = {}; else input.remove_prefix(end + 1);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.empty()) continue;
        if (line.size() > maxLine) { error = "Rust diagnostic line exceeds size limit."; return false; }
        editor::JsonError parseError; auto root = editor::parseJson(line, parseError);
        if (!root || root->type != JsonType::Object || !unique(*root)) { error = "Invalid Rust diagnostic JSON."; return false; }
        const JsonValue* message = &*root;
        if (auto* reason = root->find("reason")) {
            if (reason->type != JsonType::String) { error = "Invalid cargo message reason."; return false; }
            if (reason->stringValue != "compiler-message") continue;
            message = field(*root, "message", JsonType::Object);
            if (!message) { error = "Invalid cargo compiler message."; return false; }
        }
        auto* code = field(*message, "code", JsonType::Object);
        if (!code) continue;
        auto* id = field(*code, "code", JsonType::String);
        if (!id || !known(id->stringValue)) continue;
        if (result.size() >= maxDiagnostics) { error = "Too many ownership diagnostics."; return false; }
        RustOwnershipDiagnostic d; d.code = id->stringValue;
        if (!textField(*code, "explanation", d.explanation, true) || !textField(*message, "rendered", d.rendered, true)) { error = "Invalid compiler explanation."; return false; }
        if (!textField(*message, "level", d.level) || !textField(*message, "message", d.message) || !spans(*message, d.spans, error) || !notes(*message, d.notes, error)) { if (error.empty()) error = "Invalid ownership diagnostic."; return false; }
        result.push_back(std::move(d));
    }
    diagnostics = std::move(result); return true;
}
}
