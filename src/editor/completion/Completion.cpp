#include "editor/completion/Completion.hpp"
#include <array>
#include <algorithm>

namespace lucid::editor {
void collectFunctionNames(std::u32string_view text, std::set<std::string>& names) {
    const auto identifier = [](char32_t c) {
        return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') ||
               (c >= U'0' && c <= U'9') || c == U'_';
    };
    for (std::size_t i = 0; i < text.size();) {
        if (!identifier(text[i]) || (text[i] >= U'0' && text[i] <= U'9')) { ++i; continue; }
        std::string word;
        while (i < text.size() && identifier(text[i])) word.push_back(static_cast<char>(text[i++]));
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
