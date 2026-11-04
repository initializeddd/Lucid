#pragma once

#include <cstddef>
#include <string_view>

namespace lucid::text {

// Decode one scalar value, consuming one byte for malformed input so callers
// can render a replacement and continue without skipping valid text.
[[nodiscard]] inline char32_t nextUtf8(std::string_view text, std::size_t& offset) noexcept {
    if (offset >= text.size()) return U'\0';
    const auto first = static_cast<unsigned char>(text[offset]);
    if (first < 0x80U) { ++offset; return first; }
    unsigned length = 0;
    char32_t value = 0, minimum = 0;
    if ((first & 0xe0U) == 0xc0U) { length = 2; value = first & 0x1fU; minimum = 0x80U; }
    else if ((first & 0xf0U) == 0xe0U) { length = 3; value = first & 0x0fU; minimum = 0x800U; }
    else if ((first & 0xf8U) == 0xf0U) { length = 4; value = first & 0x07U; minimum = 0x10000U; }
    else { ++offset; return U'\ufffd'; }
    if (length > text.size() - offset) { ++offset; return U'\ufffd'; }
    for (unsigned i = 1; i < length; ++i) {
        const auto byte = static_cast<unsigned char>(text[offset + i]);
        if ((byte & 0xc0U) != 0x80U) { ++offset; return U'\ufffd'; }
        value = static_cast<char32_t>((value << 6U) | (byte & 0x3fU));
    }
    if (value < minimum || value > 0x10ffffU || (value >= 0xd800U && value <= 0xdfffU)) {
        ++offset; return U'\ufffd';
    }
    offset += length;
    return value;
}

// Byte offset before the final complete scalar; malformed trailing bytes are
// removed one at a time, just as nextUtf8 consumes malformed input.
[[nodiscard]] inline std::size_t lastUtf8Start(std::string_view text) noexcept {
    if (text.empty()) return 0;
    std::size_t start = text.size() - 1;
    while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xc0U) == 0x80U) --start;
    std::size_t end = start;
    (void)nextUtf8(text, end);
    return end == text.size() ? start : text.size() - 1;
}

// Find the largest whole-character prefix fitting a measured width, without
// repeatedly measuring progressively shorter versions of a potentially long line.
template <typename Advance>
[[nodiscard]] inline std::size_t fitUtf8Prefix(std::string_view text, float maxWidth, Advance advance) {
    std::size_t offset = 0;
    float width = 0.0F;
    while (offset < text.size()) {
        const std::size_t start = offset;
        const char32_t codepoint = nextUtf8(text, offset);
        width += advance(codepoint);
        if (width > maxWidth) return start;
    }
    return offset;
}

} // namespace lucid::text
