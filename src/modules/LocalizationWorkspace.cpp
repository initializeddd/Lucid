#include "modules/LocalizationWorkspace.hpp"
#include "editor/json/Json.hpp"
#include <fstream>
#include <algorithm>
#include <iterator>
#include <map>
#include <set>
#include <system_error>

namespace lucid::modules {
namespace fs = std::filesystem;
namespace {
constexpr std::uintmax_t maxFileSize = 1024 * 1024;
constexpr std::size_t maxKeys = 10000, maxDepth = 64;
struct TextInfo { std::string text; std::size_t length = 0, offset = 0; bool hasRtl = false; };
using Lengths = std::map<std::string, TextInfo>;
bool utf8Length(const std::string& s, std::size_t& length, bool* hasRtl = nullptr) {
    length = 0;
    if (hasRtl) *hasRtl = false;
    for (std::size_t i = 0; i < s.size(); ++length) {
        auto c = static_cast<unsigned char>(s[i++]);
        if (c < 0x80) continue;
        unsigned n = 0; unsigned value = 0, min = 0;
        if (c >= 0xc2 && c <= 0xdf) { n = 1; value = c & 31; min = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { n = 2; value = c & 15; min = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { n = 3; value = c & 7; min = 0x10000; }
        else return false;
        if (n > s.size() - i) return false;
        while (n--) { auto b = static_cast<unsigned char>(s[i++]); if ((b & 0xc0) != 0x80) return false; value = (value << 6) | (b & 63); }
        if (value < min || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return false;
        if (hasRtl && ((value >= 0x0590 && value <= 0x08ff) || (value >= 0xfb1d && value <= 0xfdff) || (value >= 0xfe70 && value <= 0xfefc))) *hasRtl = true;
    }
    return true;
}
std::string pointerPart(const std::string& key) {
    std::string part;
    for (char c : key) { if (c == '~') part += "~0"; else if (c == '/') part += "~1"; else part += c; }
    return part;
}
bool flatten(const editor::JsonValue& value, const std::string& path, std::size_t depth, Lengths& out, std::string& error) {
    if (depth > maxDepth) { error = "Localization JSON is too deeply nested."; return false; }
    if (value.type == editor::JsonType::String) {
        TextInfo info;
        if (!utf8Length(value.stringValue, info.length, &info.hasRtl)) { error = "Localization text is not valid UTF-8."; return false; }
        if (out.size() >= maxKeys) { error = "Localization file has too many keys."; return false; }
        info.text = value.stringValue; info.offset = value.byteOffset;
        out.emplace(path, std::move(info)); return true;
    }
    if (value.type == editor::JsonType::Array) {
        for (std::size_t i = 0; i < value.children.size(); ++i)
            if (!flatten(value.children[i], path + "/" + std::to_string(i), depth + 1, out, error)) return false;
        return true;
    }
    if (value.type != editor::JsonType::Object) { error = "Localization values must be strings, arrays or objects."; return false; }
    std::set<std::string> keys;
    for (const auto& child : value.children) {
        std::size_t ignored;
        if (!utf8Length(child.key, ignored) || !keys.insert(child.key).second) { error = "Invalid or duplicate localization key."; return false; }
        if (!flatten(child, path + "/" + pointerPart(child.key), depth + 1, out, error)) return false;
    }
    return true;
}
bool parseText(std::string_view text, Lengths& out, std::string& error);
bool load(const fs::path& root, const fs::path& input, Lengths& out, std::string& error) {
    std::error_code ec;
    auto file = input.is_absolute() ? input : root / input;
    if (fs::symlink_status(file, ec).type() != fs::file_type::regular || ec) { error = "Localization file is missing or unsafe."; return false; }
    const auto resolved = fs::canonical(file, ec);
    if (ec) { error = "Cannot resolve localization file."; return false; }
    const auto relative = resolved.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") { error = "Localization file must be inside the project."; return false; }
    auto size = fs::file_size(resolved, ec);
    if (ec || size > maxFileSize) { error = "Localization file exceeds size limit or cannot be read."; return false; }
    std::ifstream in(resolved, std::ios::binary);
    std::string text(static_cast<std::size_t>(size) + 1, '\0');
    in.read(text.data(), static_cast<std::streamsize>(text.size()));
    if (in.bad() || static_cast<std::uintmax_t>(in.gcount()) != size) { error = "Localization file changed or could not be read."; return false; }
    text.resize(static_cast<std::size_t>(size));
    return parseText(text, out, error);
}
bool parseText(std::string_view text, Lengths& out, std::string& error) {
    if (text.size() > maxFileSize) { error = "Localization text exceeds size limit."; return false; }
    editor::JsonError parseError; auto json = editor::parseJson(text, parseError);
    if (!json || json->type != editor::JsonType::Object) { error = "Invalid localization JSON object."; return false; }
    return flatten(*json, "", 0, out, error);
}
std::map<std::string, std::size_t> placeholders(std::string_view text) {
    std::map<std::string, std::size_t> found;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '{') continue;
        if (i + 1 < text.size() && text[i + 1] == '{') { ++i; continue; }
        const auto end = text.find('}', i + 1);
        if (end == std::string_view::npos) break;
        const auto name = text.substr(i + 1, end - i - 1);
        const auto validFirst = [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
        const auto validRest = [&](unsigned char c) { return validFirst(c) || (c >= '0' && c <= '9'); };
        const auto digit = [](unsigned char c) { return c >= '0' && c <= '9'; };
        if (!name.empty() && name.size() <= 128 &&
            ((validFirst(static_cast<unsigned char>(name.front())) && std::all_of(name.begin() + 1, name.end(), validRest)) ||
             std::all_of(name.begin(), name.end(), digit))) ++found[std::string(name)];
        i = end;
    }
    return found;
}
void merge(const Lengths& left, const Lengths& right, std::vector<LocalizationEntry>& entries) {
    std::map<std::string, LocalizationEntry> merged;
    for (const auto& [key, info] : left) { auto& entry = merged[key]; entry.key = key; entry.inSource = true; entry.sourceText = info.text; entry.sourceOffset = info.offset; entry.sourceLength = info.length; entry.sourceHasRtl = info.hasRtl; entry.sourceLong = info.length > 120; }
    for (const auto& [key, info] : right) { auto& entry = merged[key]; entry.key = key; entry.inTranslation = true; entry.translationText = info.text; entry.translationOffset = info.offset; entry.translationLength = info.length; entry.translationHasRtl = info.hasRtl; entry.translationLong = info.length > 120; }
    for (auto& [key, entry] : merged) {
        if (entry.inSource && entry.inTranslation) {
            const auto source = placeholders(entry.sourceText), translation = placeholders(entry.translationText);
            for (const auto& [name, count] : source) {
                const auto it = translation.find(name);
                if (it == translation.end()) entry.missingPlaceholders.push_back(name);
                else if (count != it->second) entry.differingPlaceholderCounts.push_back({name, count, it->second});
            }
            for (const auto& [name, count] : translation) {
                (void)count;
                if (!source.contains(name)) entry.extraPlaceholders.push_back(name);
            }
        }
        entries.push_back(std::move(entry));
    }
}
}
bool compareLocalizationText(std::string_view source, std::string_view translation,
                             std::vector<LocalizationEntry>& entries, std::string& error) {
    entries.clear(); error.clear(); Lengths left, right;
    if (!parseText(source, left, error) || !parseText(translation, right, error)) return false;
    merge(left, right, entries); return true;
}
bool compareLocalizationFiles(const fs::path& projectRoot, const fs::path& source, const fs::path& translation,
                              std::vector<LocalizationEntry>& entries, std::string& error) {
    entries.clear(); error.clear(); std::error_code ec;
    const auto root = fs::canonical(projectRoot, ec);
    if (ec || !fs::is_directory(root, ec) || ec) { error = "Project directory is unavailable."; return false; }
    Lengths left, right;
    if (!load(root, source, left, error) || !load(root, translation, right, error)) return false;
    merge(left, right, entries);
    return true;
}
}
