#include "editor/syntax/Syntax.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string_view>
#include <vector>

namespace lucid::editor {
namespace {

constexpr std::u32string_view kKeywords[] = {
    U"alignas",      U"alignof",     U"and",          U"and_eq",
    U"asm",          U"bitand",      U"bitor",        U"break",
    U"case",         U"catch",       U"class",        U"compl",
    U"concept",      U"const",       U"consteval",    U"constexpr",
    U"constinit",    U"const_cast",  U"continue",     U"co_await",
    U"co_return",    U"co_yield",    U"decltype",     U"default",
    U"delete",       U"do",          U"dynamic_cast", U"else",
    U"enum",         U"explicit",    U"export",       U"extern",
    U"false",        U"final",       U"for",          U"friend",
    U"goto",         U"if",          U"import",       U"inline",
    U"module",       U"mutable",     U"namespace",    U"new",
    U"noexcept",     U"not",         U"not_eq",       U"nullptr",
    U"operator",     U"or",          U"or_eq",        U"override",
    U"private",      U"protected",   U"public",       U"register",
    U"reinterpret_cast",             U"requires",     U"return",
    U"sizeof",       U"static",      U"static_assert", U"static_cast",
    U"struct",       U"switch",      U"template",     U"this",
    U"throw",        U"true",        U"try",          U"typedef",
    U"typeid",       U"typename",    U"union",        U"using",
    U"virtual",      U"volatile",    U"while",        U"xor",
    U"xor_eq",
};

constexpr std::u32string_view kTypes[] = {
    U"auto",        U"bool",       U"char",       U"char8_t",
    U"char16_t",    U"char32_t",   U"double",     U"float",
    U"int",         U"long",       U"short",      U"signed",
    U"unsigned",    U"void",       U"wchar_t",    U"size_t",
    U"ptrdiff_t",   U"int8_t",     U"int16_t",    U"int32_t",
    U"int64_t",     U"uint8_t",    U"uint16_t",   U"uint32_t",
    U"uint64_t",    U"string",     U"string_view", U"vector",
    U"array",       U"map",        U"unordered_map", U"set",
    U"unordered_set", U"pair",     U"tuple",      U"optional",
    U"variant",     U"unique_ptr", U"shared_ptr", U"weak_ptr",
    U"function",    U"thread",     U"mutex",      U"atomic",
    U"span",        U"filesystem",
};

[[nodiscard]] bool isDigit(char32_t value) noexcept {
    return value >= U'0' && value <= U'9';
}

[[nodiscard]] bool isHexDigit(char32_t value) noexcept {
    return isDigit(value) || (value >= U'a' && value <= U'f') ||
           (value >= U'A' && value <= U'F');
}

[[nodiscard]] bool isIdentifierStart(char32_t value) noexcept {
    return (value >= U'a' && value <= U'z') || (value >= U'A' && value <= U'Z') ||
           value == U'_' || value >= 0x80;
}

[[nodiscard]] bool isIdentifierChar(char32_t value) noexcept {
    return isIdentifierStart(value) || isDigit(value);
}

[[nodiscard]] bool isSpace(char32_t value) noexcept {
    return value == U' ' || value == U'\t';
}

template <std::size_t Size>
[[nodiscard]] bool contains(const std::u32string_view (&words)[Size],
                            std::u32string_view word) noexcept {
    return std::find(std::begin(words), std::end(words), word) != std::end(words);
}

[[nodiscard]] std::u32string_view trimComment(std::u32string_view text) {
    while (!text.empty() && isSpace(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && isSpace(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

[[nodiscard]] bool isCompileStart(std::u32string_view text) {
    const std::u32string_view trimmed = trimComment(text);
    return !trimmed.empty() && (trimmed.front() == U'@' || trimmed.front() == U'$');
}

[[nodiscard]] bool isCompileMarker(std::u32string_view token) {
    return token == U"@compile" || token == U"@end";
}

[[nodiscard]] TokenRole classifyIdentifier(std::u32string_view line, std::size_t start,
                                           std::size_t end) {
    const std::u32string_view word = line.substr(start, end - start);
    if (contains(kKeywords, word)) {
        return TokenRole::Keyword;
    }
    if (contains(kTypes, word)) {
        return TokenRole::Type;
    }

    std::size_t next = end;
    while (next < line.size() && isSpace(line[next])) {
        ++next;
    }
    if (next < line.size() && line[next] == U'(') {
        return TokenRole::Function;
    }
    return TokenRole::Plain;
}

} // namespace

std::vector<TokenSpan> highlightCppLine(std::u32string_view line, CppState& state) {
    std::vector<TokenSpan> spans;
    spans.reserve(line.size() / 3 + 1);

    const auto add = [&spans](std::size_t start, std::size_t length, TokenRole role) {
        if (length == 0) {
            return;
        }
        const auto tokenStart = static_cast<std::uint32_t>(start);
        const auto tokenLength = static_cast<std::uint32_t>(length);
        if (!spans.empty()) {
            TokenSpan& last = spans.back();
            if (last.role == role &&
                last.start + last.length == tokenStart) {
                last.length += tokenLength;
                return;
            }
        }
        spans.push_back(TokenSpan{tokenStart, tokenLength, role});
    };

    const auto addCompileComment = [&add](std::u32string_view text, std::size_t start,
                                           std::size_t end) {
        if (end <= start) {
            return;
        }

        std::size_t contentStart = start;
        std::size_t contentEnd = end;
        if (end - start >= 2 && text.substr(start, 2) == U"/*") {
            add(start, 2, TokenRole::CompileComment);
            contentStart += 2;
        }
        if (contentEnd - contentStart >= 2 &&
            text.substr(contentEnd - 2, 2) == U"*/") {
            contentEnd -= 2;
        }

        const std::u32string_view content =
            text.substr(contentStart, contentEnd - contentStart);
        const std::u32string_view trimmed = trimComment(content);
        if (trimmed.empty()) {
            add(contentStart, contentEnd - contentStart, TokenRole::CompileComment);
        } else {
            const std::size_t leading = content.find(trimmed.front());
            if (leading > 0) {
                add(contentStart, leading, TokenRole::CompileComment);
            }
            const std::size_t directiveStart = contentStart + leading;
            const std::size_t directiveEnd = directiveStart + trimmed.size();

            if (trimmed.front() == U'@') {
                std::size_t tokenEnd = directiveStart;
                while (tokenEnd < directiveEnd && !isSpace(text[tokenEnd])) {
                    ++tokenEnd;
                }
                const std::u32string_view token =
                    text.substr(directiveStart, tokenEnd - directiveStart);
                add(directiveStart, tokenEnd - directiveStart,
                    isCompileMarker(token) ? TokenRole::CompileDirective
                                           : TokenRole::CompileFunction);
                if (tokenEnd < directiveEnd) {
                    add(tokenEnd, directiveEnd - tokenEnd,
                        TokenRole::CompileDirective);
                }
            } else {
                std::size_t index = directiveStart;
                std::size_t segmentStart = index;
                while (index < directiveEnd) {
                    if (text[index] != U'%') {
                        ++index;
                        continue;
                    }
                    std::size_t close = index + 1;
                    while (close < directiveEnd && text[close] != U'%') {
                        ++close;
                    }
                    if (close >= directiveEnd) {
                        ++index;
                        continue;
                    }
                    if (index > segmentStart) {
                        add(segmentStart, index - segmentStart,
                            TokenRole::CompileCommand);
                    }
                    add(index, close - index + 1, TokenRole::CompileVariable);
                    index = close + 1;
                    segmentStart = index;
                }
                if (segmentStart < directiveEnd) {
                    add(segmentStart, directiveEnd - segmentStart,
                        TokenRole::CompileCommand);
                }
            }
        }

        if (contentEnd < end) {
            add(contentEnd, end - contentEnd, TokenRole::CompileComment);
        }
    };

    std::size_t index = 0;
    bool onlySpaceSoFar = true;

    if (state.inBlockComment) {
        const std::size_t close = line.find(U"*/");
        const std::size_t end =
            close == std::u32string_view::npos ? line.size() : close + 2;
        const bool compile = state.inCompileBlock ||
                             isCompileStart(line.substr(0, end));
        if (compile) {
            state.inCompileBlock = true;
            addCompileComment(line, 0, end);
        } else {
            add(0, end, TokenRole::Comment);
        }
        if (close == std::u32string_view::npos) {
            return spans;
        }
        state.inBlockComment = false;
        state.inCompileBlock = false;
        index = end;
        onlySpaceSoFar = false;
    }

    while (index < line.size()) {
        const char32_t character = line[index];

        if (character == U'/' && index + 1 < line.size() && line[index + 1] == U'/') {
            add(index, line.size() - index, TokenRole::Comment);
            break;
        }

        if (character == U'/' && index + 1 < line.size() && line[index + 1] == U'*') {
            const std::size_t close = line.find(U"*/", index + 2);
            if (close == std::u32string_view::npos) {
                const bool compile = isCompileStart(line.substr(index + 2));
                state.inBlockComment = true;
                state.inCompileBlock = compile;
                if (compile) {
                    addCompileComment(line, index, line.size());
                } else {
                    add(index, line.size() - index, TokenRole::Comment);
                }
                break;
            }
            const bool compile = isCompileStart(line.substr(index + 2, close - index - 2));
            if (compile) {
                addCompileComment(line, index, close + 2);
            } else {
                add(index, close + 2 - index, TokenRole::Comment);
            }
            index = close + 2;
            onlySpaceSoFar = false;
            continue;
        }

        if (isSpace(character)) {
            std::size_t end = index + 1;
            while (end < line.size() && isSpace(line[end])) {
                ++end;
            }
            add(index, end - index, TokenRole::Plain);
            index = end;
            continue;
        }

        if (character == U'#' && onlySpaceSoFar) {
            add(index, line.size() - index, TokenRole::Preprocessor);
            break;
        }

        onlySpaceSoFar = false;

        if (character == U'"' || character == U'\'') {
            const char32_t quote = character;
            std::size_t end = index + 1;
            while (end < line.size()) {
                if (line[end] == U'\\' && end + 1 < line.size()) {
                    end += 2;
                    continue;
                }
                if (line[end] == quote) {
                    ++end;
                    break;
                }
                ++end;
            }
            add(index, end - index, TokenRole::String);
            index = end;
            continue;
        }

        if (isDigit(character) ||
            (character == U'.' && index + 1 < line.size() && isDigit(line[index + 1]))) {
            std::size_t end = index;

            if (character == U'0' && index + 1 < line.size() &&
                (line[index + 1] == U'x' || line[index + 1] == U'X')) {
                end = index + 2;
                while (end < line.size() &&
                       (isHexDigit(line[end]) || line[end] == U'\'')) {
                    ++end;
                }
            } else if (character == U'0' && index + 1 < line.size() &&
                       (line[index + 1] == U'b' || line[index + 1] == U'B')) {
                end = index + 2;
                while (end < line.size() &&
                       (line[end] == U'0' || line[end] == U'1' ||
                        line[end] == U'\'')) {
                    ++end;
                }
            } else {
                while (end < line.size() &&
                       (isDigit(line[end]) || line[end] == U'\'')) {
                    ++end;
                }
                if (end < line.size() && line[end] == U'.') {
                    ++end;
                    while (end < line.size() &&
                           (isDigit(line[end]) || line[end] == U'\'')) {
                        ++end;
                    }
                }
                if (end < line.size() &&
                    (line[end] == U'e' || line[end] == U'E')) {
                    ++end;
                    if (end < line.size() &&
                        (line[end] == U'+' || line[end] == U'-')) {
                        ++end;
                    }
                    while (end < line.size() && isDigit(line[end])) {
                        ++end;
                    }
                }
            }

            while (end < line.size() && isIdentifierChar(line[end])) {
                ++end;
            }
            add(index, end - index, TokenRole::Number);
            index = end;
            continue;
        }

        if (isIdentifierStart(character)) {
            std::size_t end = index + 1;
            while (end < line.size() && isIdentifierChar(line[end])) {
                ++end;
            }
            add(index, end - index, classifyIdentifier(line, index, end));
            index = end;
            continue;
        }

        add(index, 1, TokenRole::Punctuation);
        ++index;
    }

    return spans;
}

} // namespace lucid::editor
