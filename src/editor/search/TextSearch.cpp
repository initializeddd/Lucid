#include "editor/search/TextSearch.hpp"

#include <algorithm>

namespace lucid::editor {
std::optional<TextPosition> findText(const std::vector<std::u32string>& lines,
                                     std::u32string_view query, TextPosition from,
                                     bool backwards) {
    if (lines.empty() || query.empty()) return std::nullopt;
    if (!backwards) {
        const std::size_t startLine = std::min(from.line, lines.size());
        for (std::size_t line = startLine; line < lines.size(); ++line) {
            const auto offset = lines[line].find(query, line == startLine ? from.column : 0);
            if (offset != std::u32string::npos) return TextPosition{line, offset};
        }
        for (std::size_t line = 0; line < startLine; ++line) {
            const auto offset = lines[line].find(query);
            if (offset != std::u32string::npos) return TextPosition{line, offset};
        }
        if (startLine < lines.size()) {
            const auto offset = lines[startLine].find(query);
            if (offset != std::u32string::npos) return TextPosition{startLine, offset};
        }
        return std::nullopt;
    }

    const std::size_t startLine = std::min(from.line, lines.size() - 1);
    for (std::size_t remaining = startLine + 1; remaining > 0; --remaining) {
        const std::size_t line = remaining - 1;
        if (line == from.line && from.column == 0) continue;
        const auto offset = lines[line].rfind(query, line == from.line ? from.column - 1 : std::u32string::npos);
        if (offset != std::u32string::npos) return TextPosition{line, offset};
    }
    for (std::size_t remaining = lines.size(); remaining > startLine; --remaining) {
        const std::size_t line = remaining - 1;
        const auto offset = lines[line].rfind(query);
        if (offset != std::u32string::npos) return TextPosition{line, offset};
    }
    return std::nullopt;
}
}
