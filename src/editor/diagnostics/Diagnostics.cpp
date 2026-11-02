#include "editor/diagnostics/Diagnostics.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::editor {
namespace {

struct OpenBracket {
    char32_t character = U'\0';
    std::uint32_t line = 0;
    std::uint32_t column = 0;
};

struct QuoteState {
    bool active = false;
    char32_t quote = U'"';
    std::uint32_t line = 0;
    std::uint32_t column = 0;
};

[[nodiscard]] bool isSpace(char32_t value) noexcept {
    return value == U' ' || value == U'\t' || value == U'\r';
}

[[nodiscard]] bool isDigit(char32_t value) noexcept {
    return value >= U'0' && value <= U'9';
}

[[nodiscard]] bool isIdentifierChar(char32_t value) noexcept {
    return (value >= U'a' && value <= U'z') ||
           (value >= U'A' && value <= U'Z') || isDigit(value) || value == U'_' ||
           value >= 0x80;
}

[[nodiscard]] std::string bracketText(char32_t value) {
    return std::string(1, static_cast<char>(value));
}

[[nodiscard]] bool isEscaped(std::u32string_view line, std::size_t index) noexcept {
    std::size_t slashes = 0;
    while (index > 0 && line[index - 1] == U'\\') {
        --index;
        ++slashes;
    }
    return slashes % 2 != 0;
}

[[nodiscard]] bool continuesLine(std::u32string_view line) noexcept {
    return !line.empty() && isEscaped(line, line.size());
}

[[nodiscard]] std::size_t skipSpace(std::u32string_view line, std::size_t index) noexcept {
    while (index < line.size() && isSpace(line[index])) {
        ++index;
    }
    return index;
}

void diagnoseInclude(std::u32string_view line, std::size_t hash,
                     std::uint32_t lineNumber,
                     std::vector<Diagnostic>& diagnostics) {
    std::size_t index = skipSpace(line, hash + 1);
    if (index + 7 > line.size() || line.substr(index, 7) != U"include") {
        return;
    }

    const std::size_t keywordEnd = index + 7;
    if (keywordEnd < line.size() && isIdentifierChar(line[keywordEnd])) {
        return;
    }

    index = skipSpace(line, keywordEnd);
    const auto report = [&diagnostics, lineNumber](std::size_t start) {
        diagnostics.push_back(Diagnostic{
            lineNumber, static_cast<std::uint32_t>(start + 1), 1,
            "Malformed #include"});
    };

    if (index >= line.size()) {
        report(index);
        return;
    }

    if (line[index] == U'<') {
        const std::size_t close = line.find(U'>', index + 1);
        if (close == std::u32string_view::npos || close == index + 1) {
            report(index);
        }
        return;
    }

    if (line[index] == U'"') {
        std::size_t close = index + 1;
        while (close < line.size() && line[close] != U'"') {
            ++close;
        }
        if (close == index + 1 || close >= line.size()) {
            report(index);
        }
        return;
    }

    report(index);
}

} // namespace

std::string Diagnostic::key() const {
    return std::to_string(line) + ":" + std::to_string(column) + ":" + message;
}

std::vector<Diagnostic> diagnoseCpp(const std::vector<std::u32string>& lines) {
    std::vector<Diagnostic> diagnostics;
    std::vector<OpenBracket> brackets;
    std::size_t blockLine = 0;
    std::size_t blockColumn = 0;
    bool inBlockComment = false;
    QuoteState quote;

    for (std::size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const std::u32string_view line = lines[lineIndex];
        const auto lineNumber = static_cast<std::uint32_t>(lineIndex);
        std::size_t index = 0;
        bool onlySpace = true;

        if (quote.active) {
            while (index < line.size()) {
                if (line[index] == U'\\' && index + 1 < line.size()) {
                    index += 2;
                    continue;
                }
                if (line[index] == quote.quote) {
                    quote.active = false;
                    ++index;
                    break;
                }
                ++index;
            }
            if (quote.active) {
                if (continuesLine(line)) {
                    continue;
                }
                diagnostics.push_back(Diagnostic{
                    quote.line, quote.column,
                    static_cast<std::uint32_t>(std::max<std::size_t>(1, line.size())),
                    quote.quote == U'"' ? "Unterminated string literal"
                                        : "Unterminated character literal"});
                quote.active = false;
                continue;
            }
        }

        while (index < line.size()) {
            if (inBlockComment) {
                const std::size_t close = line.find(U"*/", index);
                if (close == std::u32string_view::npos) {
                    break;
                }
                inBlockComment = false;
                index = close + 2;
                onlySpace = false;
                continue;
            }

            const char32_t character = line[index];

            if (character == U'/' && index + 1 < line.size() &&
                line[index + 1] == U'/') {
                break;
            }

            if (character == U'/' && index + 1 < line.size() &&
                line[index + 1] == U'*') {
                blockLine = lineIndex;
                blockColumn = index;
                const std::size_t close = line.find(U"*/", index + 2);
                if (close == std::u32string_view::npos) {
                    inBlockComment = true;
                    break;
                }
                index = close + 2;
                onlySpace = false;
                continue;
            }

            if (isSpace(character)) {
                ++index;
                continue;
            }

            if (character == U'#' && onlySpace) {
                diagnoseInclude(line, index, lineNumber, diagnostics);
                break;
            }

            onlySpace = false;

            const bool digitSeparator =
                character == U'\'' && index > 0 &&
                (isDigit(line[index - 1]) || isIdentifierChar(line[index - 1]));
            if (!digitSeparator && (character == U'"' || character == U'\'')) {
                std::size_t end = index + 1;
                bool closed = false;
                while (end < line.size()) {
                    if (line[end] == U'\\' && end + 1 < line.size()) {
                        end += 2;
                        continue;
                    }
                    if (line[end] == character) {
                        closed = true;
                        ++end;
                        break;
                    }
                    ++end;
                }
                if (closed) {
                    index = end;
                    continue;
                }
                if (continuesLine(line)) {
                    quote.active = true;
                    quote.quote = character;
                    quote.line = lineNumber;
                    quote.column = static_cast<std::uint32_t>(index + 1);
                    break;
                }
                diagnostics.push_back(Diagnostic{
                    lineNumber, static_cast<std::uint32_t>(index + 1),
                    static_cast<std::uint32_t>(
                        std::max<std::size_t>(1, line.size() - index)),
                    character == U'"' ? "Unterminated string literal"
                                      : "Unterminated character literal"});
                break;
            }

            if (character == U'{' || character == U'(' || character == U'[') {
                brackets.push_back(OpenBracket{character, lineNumber,
                                               static_cast<std::uint32_t>(index + 1)});
                ++index;
                continue;
            }

            if (character == U'}' || character == U')' || character == U']') {
                if (brackets.empty()) {
                    diagnostics.push_back(Diagnostic{
                        lineNumber, static_cast<std::uint32_t>(index + 1), 1,
                        "Unexpected '" + bracketText(character) + "'"});
                } else {
                    const char32_t open = brackets.back().character;
                    const char32_t expected =
                        open == U'{' ? U'}' : (open == U'(' ? U')' : U']');
                    if (character != expected) {
                        diagnostics.push_back(Diagnostic{
                            lineNumber, static_cast<std::uint32_t>(index + 1), 1,
                            "Mismatched '" + bracketText(character) + "', expected '" +
                                bracketText(expected) + "'"});
                    }
                    brackets.pop_back();
                }
                ++index;
                continue;
            }

            ++index;
        }
    }

    if (quote.active) {
        diagnostics.push_back(Diagnostic{
            quote.line, quote.column, 1,
            quote.quote == U'"' ? "Unterminated string literal"
                                : "Unterminated character literal"});
    }

    if (inBlockComment) {
        diagnostics.push_back(Diagnostic{
            static_cast<std::uint32_t>(blockLine),
            static_cast<std::uint32_t>(blockColumn + 1), 2,
            "Unterminated block comment"});
    }

    for (const OpenBracket& bracket : brackets) {
        diagnostics.push_back(Diagnostic{
            bracket.line, bracket.column, 1,
            "Unclosed '" + bracketText(bracket.character) + "'"});
    }

    return diagnostics;
}

} // namespace lucid::editor
