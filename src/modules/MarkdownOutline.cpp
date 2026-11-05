#include "modules/MarkdownOutline.hpp"
#include <string_view>
#include <md4c.h>
#include <algorithm>
#include <cstdint>
#include <limits>
namespace lucid::modules {
namespace {
constexpr std::size_t maxHeadingCount = 10000, maxTitleBytes = 4096;
void appendBounded(std::string& target, std::string_view text, bool& truncated) {
    if (truncated) return;
    const auto available = maxTitleBytes - target.size();
    if (text.size() <= available) { target.append(text); return; }
    std::size_t end = available;
    // Stop before an incomplete UTF-8 sequence; Document produces valid UTF-8.
    while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) --end;
    target.append(text.substr(0, end)); truncated = true;
}
struct SetextContext {
    const std::string* source = nullptr;
    std::vector<std::size_t> starts;
    std::vector<MarkdownHeading> headings;
    std::string title;
    std::size_t first = 0, last = 0;
    int level = 0;
    std::size_t cap = 0;
    bool inHeading = false, hasLocation = false, truncated = false, titleTruncated = false;
};
std::size_t lineAt(const SetextContext& ctx, std::size_t offset) {
    return static_cast<std::size_t>(std::upper_bound(ctx.starts.begin(), ctx.starts.end(), offset) - ctx.starts.begin() - 1);
}
int beginBlock(MD_BLOCKTYPE type, void* detail, void* data) {
    auto& c = *static_cast<SetextContext*>(data);
    if (type == MD_BLOCK_H) { c.inHeading = true; c.title.clear(); c.hasLocation = false; c.titleTruncated = false; c.level = static_cast<int>(static_cast<MD_BLOCK_H_DETAIL*>(detail)->level); }
    return 0;
}
int headingText(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* data) {
    auto& c = *static_cast<SetextContext*>(data); if (!c.inHeading) return 0;
    auto ptr = reinterpret_cast<std::uintptr_t>(text), base = reinterpret_cast<std::uintptr_t>(c.source->data());
    if (ptr >= base && ptr - base < c.source->size()) {
        auto line = lineAt(c, ptr - base);
        if (!c.hasLocation) { c.first = line; c.hasLocation = true; }
        c.last = line;
    }
    if (c.headings.size() < c.cap) {
        if (type == MD_TEXT_SOFTBR || type == MD_TEXT_BR) appendBounded(c.title, " ", c.titleTruncated);
        else if (type != MD_TEXT_HTML && type != MD_TEXT_NULLCHAR) appendBounded(c.title, std::string_view(text, size), c.titleTruncated);
    }
    return 0;
}
int endBlock(MD_BLOCKTYPE type, void*, void* data) {
    auto& c = *static_cast<SetextContext*>(data);
    if (type != MD_BLOCK_H) return 0;
    c.inHeading = false;
    if (!c.hasLocation || c.last + 1 >= c.starts.size()) return 0;
    // An ATX heading followed by a thematic break is not a Setext heading.
    auto firstStart = c.starts[c.first];
    auto firstEnd = c.source->find('\n', firstStart);
    auto firstLine = std::string_view(*c.source).substr(firstStart, firstEnd == std::string::npos ? firstEnd : firstEnd - firstStart);
    std::size_t markerStart = 0;
    while (markerStart < firstLine.size() && markerStart < 4 && firstLine[markerStart] == ' ') ++markerStart;
    if (markerStart <= 3 && markerStart < firstLine.size() && firstLine[markerStart] == '#') return 0;
    auto start = c.starts[c.last + 1], end = c.source->find('\n', start);
    auto underline = std::string_view(*c.source).substr(start, end == std::string::npos ? end : end - start);
    std::size_t i = 0; while (i < underline.size() && i < 4 && underline[i] == ' ') ++i;
    if (i > 3 || i == underline.size()) return 0;
    const char marker = underline[i]; if ((c.level == 1 && marker != '=') || (c.level == 2 && marker != '-')) return 0;
    while (i < underline.size() && underline[i] == marker) ++i;
    while (i < underline.size() && (underline[i] == ' ' || underline[i] == '\t' || underline[i] == '\r')) ++i;
    if (i == underline.size()) {
        if (c.headings.size() >= c.cap) c.truncated = true;
        else { c.headings.push_back({c.title, c.first, c.level}); c.truncated = c.truncated || c.titleTruncated; }
    }
    return 0;
}
}
MarkdownOutline markdownOutline(const editor::Document& document, std::size_t limit) {
    MarkdownOutline result; const auto cap = std::min(limit, maxHeadingCount); char32_t fence = 0; std::size_t fenceLength = 0;
    for (std::size_t line = 0; line < document.lineCount(); ++line) {
        const auto& text = document.line(line); std::size_t pos = 0;
        while (pos < text.size() && pos < 4 && text[pos] == U' ') ++pos;
        if (pos > 3 || pos == text.size()) continue;
        if (text[pos] == U'`' || text[pos] == U'~') {
            const auto marker = text[pos]; auto end = pos;
            while (end < text.size() && text[end] == marker) ++end;
            if (end - pos >= 3) {
                if (!fence) { if (marker == U'`' && text.find(U'`', end) != std::u32string::npos) continue; fence = marker; fenceLength = end - pos; }
                else if (marker == fence && end - pos >= fenceLength && text.find_first_not_of(U" \t", end) == std::u32string::npos) fence = 0;
                continue;
            }
        }
        if (fence || text[pos] != U'#') continue;
        auto end = pos; while (end < text.size() && text[end] == U'#') ++end;
        const auto level = end - pos;
        if (level > 6 || (end < text.size() && text[end] != U' ' && text[end] != U'\t')) continue;
        auto start = end; while (start < text.size() && (text[start] == U' ' || text[start] == U'\t')) ++start;
        auto finish = text.size(); while (finish > start && (text[finish - 1] == U' ' || text[finish - 1] == U'\t')) --finish;
        auto hashes = finish; while (hashes > start && text[hashes - 1] == U'#') --hashes;
        if (hashes > start && hashes < finish && (text[hashes - 1] == U' ' || text[hashes - 1] == U'\t')) { finish = hashes - 1; while (finish > start && (text[finish - 1] == U' ' || text[finish - 1] == U'\t')) --finish; }
        if (result.headings.size() >= cap) { result.truncated = true; break; }
        // Preserve UTF-8 using the editor's conversion, rather than byte slicing.
        auto title = editor::Document::fromUtf8(document.lineUtf8(line));
        title.setCursor(0, start); title.extendSelectionTo(0, finish);
        auto textUtf8 = title.selectionUtf8(); std::string bounded; bool titleTruncated = false;
        appendBounded(bounded, textUtf8, titleTruncated);
        result.truncated = result.truncated || titleTruncated;
        result.headings.push_back({std::move(bounded), line, static_cast<int>(level)});
    }
    // Use MD4C's block parser to distinguish Setext headings from thematic breaks,
    // fenced/indented code, HTML blocks, and other non-heading text.
    auto source = document.toUtf8();
    if (source.size() <= 16U * 1024U * 1024U && source.size() <= std::numeric_limits<MD_SIZE>::max()) {
        SetextContext context; context.source = &source; context.cap = cap; context.starts.push_back(0);
        for (std::size_t i = 0; i < source.size(); ++i) if (source[i] == '\n') context.starts.push_back(i + 1);
        MD_PARSER parser{}; parser.flags = MD_DIALECT_GITHUB & ~MD_FLAG_TABLES;
        parser.enter_block = beginBlock; parser.leave_block = endBlock;
        parser.enter_span = +[](MD_SPANTYPE, void*, void*) { return 0; };
        parser.leave_span = +[](MD_SPANTYPE, void*, void*) { return 0; }; parser.text = headingText;
        if (md_parse(source.data(), static_cast<MD_SIZE>(source.size()), &parser, &context) == 0) {
            result.headings.insert(result.headings.end(), context.headings.begin(), context.headings.end());
            result.truncated = result.truncated || context.truncated;
        } else result.truncated = true;
    } else result.truncated = true;
    std::stable_sort(result.headings.begin(), result.headings.end(), [](const auto& a, const auto& b) { return a.line < b.line; });
    if (result.headings.size() > cap) { result.headings.resize(cap); result.truncated = true; }
    return result;
}
}
