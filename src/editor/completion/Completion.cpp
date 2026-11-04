#include "editor/completion/Completion.hpp"
#include "text/Utf8.hpp"
#include <array>
#include <algorithm>

namespace lucid::editor {
bool isCompletionIdentifier(char32_t c) noexcept {
    // Unicode spaces and C1 control characters must never be included in the
    // text that accepting a completion replaces.
    if ((c >= 0x80 && c <= 0x9f) || c == 0x00a0 || c == 0x1680 ||
        (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 ||
        c == 0x202f || c == 0x205f || c == 0x3000) return false;
    return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') ||
           (c >= U'0' && c <= U'9') || c == U'_' ||
           (c >= 0x80 && c <= 0x10ffff && c != 0xfffd && !(c >= 0xd800 && c <= 0xdfff));
}

bool isCompletionIdentifierUtf8(std::string_view value) noexcept {
    if (value.empty()) return false;
    for (std::size_t offset = 0; offset < value.size();) {
        if (!isCompletionIdentifier(text::nextUtf8(value, offset))) return false;
    }
    return true;
}

namespace {
void appendIdentifier(std::string& out, char32_t c) {
    if (c < 0x80) out.push_back(static_cast<char>(c));
    else if (c < 0x800) {
        out.push_back(static_cast<char>(0xc0 | (c >> 6)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3f)));
    } else if (c < 0x10000) {
        out.push_back(static_cast<char>(0xe0 | (c >> 12)));
        out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3f)));
    } else {
        out.push_back(static_cast<char>(0xf0 | (c >> 18)));
        out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3f)));
    }
}
}

void collectFunctionNames(std::u32string_view text, std::set<std::string>& names) {
    for (std::size_t i = 0; i < text.size();) {
        if (!isCompletionIdentifier(text[i]) || (text[i] >= U'0' && text[i] <= U'9')) { ++i; continue; }
        std::string word;
        while (i < text.size() && isCompletionIdentifier(text[i])) appendIdentifier(word, text[i++]);
        auto next = i;
        while (next < text.size() && (text[next] == U' ' || text[next] == U'\t')) ++next;
        static constexpr std::array<std::string_view, 13> keywords = {
            "if", "for", "while", "switch", "catch", "return", "sizeof",
            "typeof", "new", "delete", "with", "assert", "match"};
        if (next < text.size() && text[next] == U'(' &&
            std::find(keywords.begin(), keywords.end(), word) == keywords.end())
            names.insert(std::move(word));
    }
}
}
