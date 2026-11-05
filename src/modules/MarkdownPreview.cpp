#include "modules/MarkdownPreview.hpp"
#include <md4c.h>
extern "C" {
#include <entity.h>
}
#include <charconv>
#include <limits>
#include <utility>

namespace lucid::modules {
namespace {
struct Context { MarkdownPreview result; MarkdownBlock current; bool active = false, html = false; int listDepth = 0, quoteDepth = 0; std::size_t bytes = 0; };
int enterBlock(MD_BLOCKTYPE type, void* detail, void* data) {
    auto& c = *static_cast<Context*>(data);
    if (type == MD_BLOCK_HTML) { c.html = true; return 0; }
    if (type == MD_BLOCK_QUOTE) { ++c.quoteDepth; return 0; }
    if (type == MD_BLOCK_LI) {
        ++c.listDepth;
        if (c.active && !c.current.text.empty()) c.result.blocks.push_back(std::move(c.current));
        if (c.result.blocks.size() >= 10000) { c.result.truncated = true; return 1; }
        c.current = {}; c.current.kind = MarkdownBlockKind::ListItem; c.active = true; return 0;
    }
    if (type != MD_BLOCK_H && type != MD_BLOCK_P && type != MD_BLOCK_CODE && type != MD_BLOCK_HR) return 0;
    if (type == MD_BLOCK_P && c.active && c.current.kind == MarkdownBlockKind::ListItem) return 0;
    if (c.active && !c.current.text.empty()) c.result.blocks.push_back(std::move(c.current));
    if (c.result.blocks.size() >= 10000) { c.result.truncated = true; return 1; }
    c.current = {}; c.active = true;
    if (type == MD_BLOCK_H) { c.current.kind = MarkdownBlockKind::Heading; c.current.level = static_cast<int>(static_cast<MD_BLOCK_H_DETAIL*>(detail)->level); }
    else if (type == MD_BLOCK_CODE) c.current.kind = MarkdownBlockKind::Code;
    else if (type == MD_BLOCK_HR) c.current.kind = MarkdownBlockKind::Rule;
    else if (c.listDepth > 0) c.current.kind = MarkdownBlockKind::ListItem;
    else if (c.quoteDepth) c.current.kind = MarkdownBlockKind::Quote;
    return 0;
}
int leaveBlock(MD_BLOCKTYPE type, void*, void* data) {
    auto& c = *static_cast<Context*>(data);
    if (type == MD_BLOCK_HTML) { c.html = false; return 0; }
    if (type == MD_BLOCK_QUOTE) { --c.quoteDepth; return 0; }
    if (type == MD_BLOCK_LI) {
        if (c.active && c.current.kind == MarkdownBlockKind::ListItem) c.result.blocks.push_back(std::move(c.current));
        c.active = false; if (c.listDepth > 0) --c.listDepth; return 0;
    }
    if (type == MD_BLOCK_H || type == MD_BLOCK_P || type == MD_BLOCK_CODE || type == MD_BLOCK_HR) {
        if (c.active) c.result.blocks.push_back(std::move(c.current));
        c.active = false;
    }
    return 0;
}
int text(MD_TEXTTYPE type, const MD_CHAR* input, MD_SIZE size, void* data) {
    auto& c = *static_cast<Context*>(data);
    if (!c.active || c.html || type == MD_TEXT_HTML) return 0;
    std::string_view value(input, size);
    std::string decoded;
    const auto appendUtf8 = [&decoded](unsigned cp) {
        if (cp == 0 || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) cp = 0xfffd;
        if (cp < 0x80) decoded.push_back(static_cast<char>(cp));
        else if (cp < 0x800) { decoded.push_back(static_cast<char>(0xc0 | (cp >> 6))); decoded.push_back(static_cast<char>(0x80 | (cp & 63))); }
        else if (cp < 0x10000) { decoded.push_back(static_cast<char>(0xe0 | (cp >> 12))); decoded.push_back(static_cast<char>(0x80 | ((cp >> 6) & 63))); decoded.push_back(static_cast<char>(0x80 | (cp & 63))); }
        else { decoded.push_back(static_cast<char>(0xf0 | (cp >> 18))); decoded.push_back(static_cast<char>(0x80 | ((cp >> 12) & 63))); decoded.push_back(static_cast<char>(0x80 | ((cp >> 6) & 63))); decoded.push_back(static_cast<char>(0x80 | (cp & 63))); }
    };
    if (type == MD_TEXT_ENTITY && value.size() > 3 && value[1] == '#') {
        auto digits = value.substr(2, value.size() - 3); int base = 10;
        if (!digits.empty() && (digits[0] == 'x' || digits[0] == 'X')) { base = 16; digits.remove_prefix(1); }
        unsigned cp = 0; const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), cp, base);
        if (parsed.ec == std::errc{} && parsed.ptr == digits.data() + digits.size()) { appendUtf8(cp); value = decoded; }
    } else if (type == MD_TEXT_ENTITY) {
        if (const auto* entity = entity_lookup(value.data(), value.size())) { appendUtf8(entity->codepoints[0]); if (entity->codepoints[1]) appendUtf8(entity->codepoints[1]); value = decoded; }
    }
    if (type == MD_TEXT_BR) value = "\n";
    else if (type == MD_TEXT_SOFTBR) value = " ";
    else if (type == MD_TEXT_NULLCHAR) value = "\xef\xbf\xbd";
    if (value.size() > 1024U * 1024U - c.bytes) { c.result.truncated = true; return 1; }
    c.current.text.append(value); c.bytes += value.size(); return 0;
}
}
bool parseMarkdownPreview(std::string_view source, MarkdownPreview& result, std::string& error) {
    result = {}; error.clear();
    if (source.size() > 16U * 1024U * 1024U || source.size() > std::numeric_limits<MD_SIZE>::max()) { error = "Markdown document exceeds preview limit"; return false; }
    // Until table layout is supported, retain table source as text instead of
    // parsing table cells that the current display model cannot represent.
    Context context; MD_PARSER parser{}; parser.flags = MD_DIALECT_GITHUB & ~MD_FLAG_TABLES;
    parser.enter_block = enterBlock; parser.leave_block = leaveBlock;
    parser.enter_span = +[](MD_SPANTYPE, void*, void*) { return 0; };
    parser.leave_span = +[](MD_SPANTYPE, void*, void*) { return 0; };
    parser.text = text;
    const int status = md_parse(source.empty() ? "" : source.data(), static_cast<MD_SIZE>(source.size()), &parser, &context);
    if (status != 0 && !context.result.truncated) { error = "Could not parse Markdown"; return false; }
    result = std::move(context.result); return true;
}
}
