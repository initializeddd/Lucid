#pragma once

#include "text/Utf8.hpp"
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::text {
// Wrap display text at UTF-8 boundaries without modifying the original text.
inline std::vector<std::string> wrapLines(std::string_view input, std::size_t columns,
                                          std::vector<std::size_t>* offsets = nullptr) {
    columns = std::max<std::size_t>(columns, 1);
    std::vector<std::string> lines;
    if (offsets) offsets->clear();
    std::size_t start = 0, offset = 0, count = 0, lastBreak = 0;
    bool hasContent = false;
    const auto append = [&](std::size_t end) {
        lines.emplace_back(input.substr(start, end - start));
        if (offsets) offsets->push_back(start);
    };
    while (offset < input.size()) {
        const auto before = offset;
        const auto codepoint = nextUtf8(input, offset);
        if (codepoint == U'\n') {
            append(before);
            start = offset; count = 0; lastBreak = 0; hasContent = false;
        } else {
            if (count == columns) {
                const auto end = lastBreak > start ? lastBreak : before;
                append(end);
                start = end; offset = end; count = 0; lastBreak = 0; hasContent = false;
                continue;
            }
            ++count;
            if (codepoint == U' ' || codepoint == U'\t') {
                if (hasContent) lastBreak = offset;
            } else {
                hasContent = true;
            }
        }
    }
    if (start < input.size()) append(input.size());
    return lines;
}
}
