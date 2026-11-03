#include "editor/search/TextSearch.hpp"

namespace lucid::editor {
std::optional<TextPosition> findText(const std::vector<std::u32string>& lines,
                                     std::u32string_view query, TextPosition from,
                                     bool backwards) {
    if (lines.empty() || query.empty()) return std::nullopt;
    std::optional<TextPosition> first;
    std::optional<TextPosition> last;
    std::optional<TextPosition> before;
    for (std::size_t line = 0; line < lines.size(); ++line) {
        std::size_t offset = 0;
        while ((offset = lines[line].find(query, offset)) != std::u32string::npos) {
            const TextPosition position{line, offset};
            if (!first) first = position;
            last = position;
            const bool isBefore = line < from.line || (line == from.line && offset < from.column);
            if (isBefore) before = position;
            else if (!backwards) return position;
            ++offset;
        }
    }
    return backwards ? (before ? before : last) : first;
}
}
