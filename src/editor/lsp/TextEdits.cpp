#include "editor/lsp/TextEdits.hpp"
#include "editor/document/Document.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
namespace lucid::editor::lsp {
namespace {
bool number(const JsonValue* value, std::size_t& out) {
    if (!value || value->type != JsonType::Number || !std::isfinite(value->numberValue) || value->numberValue < 0 || value->numberValue > 16777216 || std::floor(value->numberValue) != value->numberValue) return false;
    out = static_cast<std::size_t>(value->numberValue); return true;
}
bool offset(std::string_view source, const JsonValue* position, std::size_t& result) {
    if (!position || position->type != JsonType::Object) return false;
    std::size_t line = 0, target = 0;
    if (!number(position->find("line"), line) || !number(position->find("character"), target)) return false;
    std::size_t start = 0;
    for (std::size_t i = 0; i < line; ++i) { const auto next = source.find('\n', start); if (next == std::string_view::npos) return false; start = next + 1; }
    const auto foundEnd = source.find('\n', start); const auto end = foundEnd == std::string_view::npos ? source.size() : foundEnd;
    std::size_t units = 0, cursor = start;
    while (cursor < end && units < target) {
        const auto first = static_cast<unsigned char>(source[cursor]);
        const std::size_t count = first < 0x80 ? 1 : (first & 0xe0U) == 0xc0U ? 2 : (first & 0xf0U) == 0xe0U ? 3 : 4;
        const auto width = count == 4 ? 2U : 1U;
        if (cursor + count > end || units + width > target) return false;
        cursor += count; units += width;
    }
    if (units != target) return false;
    result = cursor; return true;
}
struct Edit { std::size_t start, end; std::string text; };
}
bool applyTextEdits(std::string_view source, const JsonValue& edits, std::string& output, std::string& error) {
    output.clear(); error.clear();
    if (edits.type != JsonType::Array || edits.children.size() > 10000 || !Document::canEditLosslessly(source, error) || source.find('\r') != std::string_view::npos) { if (error.empty()) error = "Invalid language-server edit list"; return false; }
    std::vector<Edit> parsed;
    for (const auto& value : edits.children) {
        const auto* range = value.find("range"); const auto* text = value.find("newText"); std::size_t start = 0, end = 0;
        if (!range || range->type != JsonType::Object || !text || text->type != JsonType::String || !offset(source, range->find("start"), start) || !offset(source, range->find("end"), end) || start > end) { error = "Invalid language-server edit range"; return false; }
        std::string textError;
        if (!Document::canEditLosslessly(text->stringValue, textError)) { error = "Invalid text returned by language server"; return false; }
        parsed.push_back({start, end, Document::fromUtf8(text->stringValue).toUtf8()});
    }
    std::sort(parsed.begin(), parsed.end(), [](const Edit& a, const Edit& b) { return a.start > b.start || (a.start == b.start && a.end > b.end); });
    std::size_t previous = source.size() + 1;
    output = source;
    for (const auto& edit : parsed) {
        if (edit.end > previous || edit.start == previous) { output.clear(); error = "Overlapping language-server edits"; return false; }
        output.replace(edit.start, edit.end - edit.start, edit.text); previous = edit.start;
        if (output.size() > 16U * 1024U * 1024U) { output.clear(); error = "Language-server edit is too large"; return false; }
    }
    return true;
}
}
