#include "modules/LocalizationWorkspace.hpp"
#include "debug/Protocol.hpp"
#include "editor/json/Json.hpp"
#include "text/Utf8.hpp"
#include <algorithm>
#include <charconv>

namespace lucid::modules {
namespace {
constexpr std::size_t kLimit = 1024U * 1024U;
bool validUtf8(std::string_view text) {
    for (std::size_t i = 0; i < text.size();) {
        const auto start = i; const auto cp = lucid::text::nextUtf8(text, i);
        if (cp == U'\ufffd' && i - start == 1 && static_cast<unsigned char>(text[start]) >= 0x80) return false;
    }
    return true;
}
bool pointerParts(std::string_view pointer, std::vector<std::string>& parts) {
    if (pointer.empty() || pointer.front() != '/' || pointer.size() > 8192) return false;
    std::size_t start = 1;
    for (;;) {
        auto end = pointer.find('/', start); if (end == std::string_view::npos) end = pointer.size();
        std::string part;
        for (std::size_t i = start; i < end; ++i) {
            if (pointer[i] != '~') part += pointer[i];
            else { if (++i >= end || (pointer[i] != '0' && pointer[i] != '1')) return false; part += pointer[i] == '0' ? '~' : '/'; }
        }
        if (!validUtf8(part) || parts.size() >= 64) return false;
        parts.push_back(std::move(part)); if (end == pointer.size()) return true; start = end + 1;
    }
}
const editor::JsonValue* findChild(const editor::JsonValue& parent, std::string_view key) {
    if (parent.type == editor::JsonType::Object) {
        const auto it = std::find_if(parent.children.begin(), parent.children.end(), [key](const auto& item) { return item.key == key; });
        return it == parent.children.end() ? nullptr : &*it;
    }
    if (parent.type == editor::JsonType::Array) {
        if (key.empty() || (key.size() > 1 && key.front() == '0')) return nullptr;
        std::size_t index = 0; const auto parsed = std::from_chars(key.data(), key.data() + key.size(), index);
        return parsed.ec == std::errc{} && parsed.ptr == key.data() + key.size() && index < parent.children.size() ? &parent.children[index] : nullptr;
    }
    return nullptr;
}
bool valueEnd(std::string_view source, std::size_t start, std::size_t& end) {
    if (start >= source.size()) return false;
    if (source[start] == '"') {
        bool escape = false;
        for (std::size_t i = start + 1; i < source.size(); ++i) {
            if (escape) escape = false; else if (source[i] == '\\') escape = true; else if (source[i] == '"') { end = i + 1; return true; }
        }
        return false;
    }
    if (source[start] != '{') return false;
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
}
bool prepareLocalizationEdit(std::string_view source, std::string_view translation, std::string_view pointer,
                             std::string_view value, LocalizationEditPlan& plan, std::string& error) {
    plan = {}; error.clear();
    if (value.size() > kLimit || !validUtf8(value)) { error = "Translation must be valid bounded UTF-8"; return false; }
    std::vector<std::string> parts; if (!pointerParts(pointer, parts)) { error = "Invalid localization key"; return false; }
    std::vector<LocalizationEntry> entries;
    if (!compareLocalizationText(source, translation, entries, error)) return false;
    const auto found = std::find_if(entries.begin(), entries.end(), [pointer](const auto& entry) { return entry.key == pointer; });
    if (found == entries.end() || !found->inSource) { error = "Key is absent from the source snapshot"; return false; }
    editor::JsonError parseError; auto root = editor::parseJson(translation, parseError);
    if (!root) { error = "Invalid translation snapshot"; return false; }
    const editor::JsonValue* parent = &*root;
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) { parent = findChild(*parent, parts[i]); if (!parent) { error = "Translation parent does not exist"; return false; } }
    const auto* target = findChild(*parent, parts.back());
    std::string result(translation); std::size_t end = 0; const auto encoded = debug::jsonString(value); bool inserted = false;
    if (target) {
        if (target->type != editor::JsonType::String || !valueEnd(translation, target->byteOffset, end)) { error = "Translation key is not a string"; return false; }
        result.replace(target->byteOffset, end - target->byteOffset, encoded);
    } else {
        if (parent->type != editor::JsonType::Object || !valueEnd(translation, parent->byteOffset, end) || !end || translation[end - 1] != '}') { error = "Cannot insert this key; create its object parent first"; return false; }
        const auto insertion = std::string(parent->children.empty() ? "" : ",") + debug::jsonString(parts.back()) + ":" + encoded;
        if (insertion.size() > kLimit - result.size()) { error = "Translation exceeds size limit"; return false; }
        result.insert(end - 1, insertion); inserted = true;
    }
    if (result.size() > kLimit || !compareLocalizationText(source, result, entries, error)) { if (error.empty()) error = "Translation exceeds size limit"; return false; }
    const auto preview = std::find_if(entries.begin(), entries.end(), [pointer](const auto& entry) { return entry.key == pointer; });
    if (preview == entries.end() || !preview->inTranslation) { error = "Could not verify the edited key"; return false; }
    plan.sourceSnapshot = source; plan.translationSnapshot = translation; plan.pointer = pointer; plan.value = value; plan.updatedTranslation = std::move(result); plan.preview = *preview; plan.inserted = inserted; return true;
}
bool applyLocalizationEdit(const LocalizationEditPlan& plan, std::string_view currentSource, std::string_view currentTranslation,
                           std::string& updated, std::string& error) {
    updated.clear(); error.clear();
    if (currentSource != plan.sourceSnapshot || currentTranslation != plan.translationSnapshot) { error = "Source or translation changed; compare again"; return false; }
    LocalizationEditPlan fresh;
    if (!prepareLocalizationEdit(currentSource, currentTranslation, plan.pointer, plan.value, fresh, error)) return false;
    if (fresh.updatedTranslation != plan.updatedTranslation || fresh.inserted != plan.inserted) { error = "Edit preview changed; prepare it again"; return false; }
    updated = std::move(fresh.updatedTranslation); return true;
}
}
