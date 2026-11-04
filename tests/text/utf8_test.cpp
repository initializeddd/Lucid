#include "text/Utf8.hpp"
#include "text/Wrap.hpp"

#include <array>
#include <string_view>

int main() {
    using lucid::text::nextUtf8;
    const std::string_view text = "Gr\xc3\xbc\xc3\x9f" "e \xe2\x80\x94 \xf0\x9f\x8c\x8d";
    constexpr std::array<char32_t, 9> expected{U'G', U'r', U'ü', U'ß', U'e', U' ', U'—', U' ', U'🌍'};
    std::size_t offset = 0;
    for (const auto codepoint : expected)
        if (nextUtf8(text, offset) != codepoint) return 1;
    if (offset != text.size()) return 2;
    const std::string_view invalid = "\xe2" "A" "\xc0\xaf";
    offset = 0;
    if (nextUtf8(invalid, offset) != U'\ufffd' || nextUtf8(invalid, offset) != U'A' ||
        nextUtf8(invalid, offset) != U'\ufffd' || nextUtf8(invalid, offset) != U'\ufffd' || offset != invalid.size()) return 3;
    const auto wrapped = lucid::text::wrapLines("Grüße\n\nWelt", 3);
    if (wrapped.size() != 5 || wrapped[0] != "Grü" || wrapped[1] != "ße" ||
        !wrapped[2].empty() || wrapped[3] != "Wel" || wrapped[4] != "t") return 4;
    const auto words = lucid::text::wrapLines("hello world!", 8);
    if (words.size() != 2 || words[0] != "hello " || words[1] != "world!") return 5;
    const auto indent = lucid::text::wrapLines("  longword", 5);
    if (indent.size() != 2 || indent[0] != "  lon" || indent[1] != "gword") return 6;
    std::vector<std::size_t> offsets;
    (void)lucid::text::wrapLines("Grüße\n\nWelt", 3, &offsets);
    if (offsets != std::vector<std::size_t>{0, 4, 8, 9, 12}) return 7;
    auto trimmed = text;
    for (auto it = expected.rbegin(); it != expected.rend(); ++it) {
        const auto start = lucid::text::lastUtf8Start(trimmed);
        std::size_t cursor = start;
        if (nextUtf8(trimmed, cursor) != *it || cursor != trimmed.size()) return 8;
        trimmed = trimmed.substr(0, start);
    }
    if (!trimmed.empty() || lucid::text::lastUtf8Start(invalid) != invalid.size() - 1) return 9;
    int measured = 0;
    const auto fit = lucid::text::fitUtf8Prefix(text, 3.0F, [&measured](char32_t) { ++measured; return 1.0F; });
    if (text.substr(0, fit) != "Grü" || measured != 4) return 10;
    if (lucid::text::fitUtf8Prefix("abc", 0.0F, [](char32_t) { return 1.0F; }) != 0) return 11;
    return 0;
}
