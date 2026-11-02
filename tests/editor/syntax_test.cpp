#include "editor/syntax/Syntax.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace {

using lucid::editor::CppState;
using lucid::editor::highlightCppLine;
using lucid::editor::TokenRole;
using lucid::editor::TokenSpan;

[[nodiscard]] bool hasToken(const std::vector<TokenSpan>& spans,
                            std::u32string_view line, std::u32string_view token,
                            TokenRole role) {
    const std::size_t tokenStart = line.find(token);
    if (tokenStart == std::u32string_view::npos) {
        return false;
    }
    const std::size_t tokenEnd = tokenStart + token.size();
    for (const TokenSpan& span : spans) {
        if (span.role == role && span.start <= tokenStart &&
            static_cast<std::size_t>(span.start) + span.length >= tokenEnd) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool coversLine(const std::vector<TokenSpan>& spans,
                              std::u32string_view line) {
    std::size_t position = 0;
    for (const TokenSpan& span : spans) {
        if (span.start != position || span.length == 0) {
            return false;
        }
        position += span.length;
    }
    return position == line.size();
}

} // namespace

int main() {
    {
        CppState state;
        const std::u32string_view line = U"int main() {";
        const std::vector<TokenSpan> spans = highlightCppLine(line, state);
        if (!coversLine(spans, line)) {
            return 1;
        }
        if (!hasToken(spans, line, U"int", TokenRole::Type) ||
            !hasToken(spans, line, U"main", TokenRole::Function) ||
            !hasToken(spans, line, U"{", TokenRole::Punctuation)) {
            return 2;
        }
        if (state.inBlockComment) {
            return 3;
        }
    }

    {
        CppState state;
        const std::u32string_view line = U"const char* text = \"hello\"; // note";
        const std::vector<TokenSpan> spans = highlightCppLine(line, state);
        if (!coversLine(spans, line)) {
            return 4;
        }
        if (!hasToken(spans, line, U"const", TokenRole::Keyword) ||
            !hasToken(spans, line, U"char", TokenRole::Type) ||
            !hasToken(spans, line, U"\"hello\"", TokenRole::String) ||
            !hasToken(spans, line, U"// note", TokenRole::Comment)) {
            return 5;
        }
    }

    {
        CppState state;
        const std::u32string_view include = U"#include <string>";
        const std::vector<TokenSpan> spans = highlightCppLine(include, state);
        if (!hasToken(spans, include, include, TokenRole::Preprocessor)) {
            return 6;
        }

        const std::u32string_view notDirective = U"value # other";
        const std::vector<TokenSpan> mixed = highlightCppLine(notDirective, state);
        if (!hasToken(mixed, notDirective, U"value", TokenRole::Plain) ||
            !hasToken(mixed, notDirective, U"#", TokenRole::Punctuation) ||
            hasToken(mixed, notDirective, U"# other", TokenRole::Preprocessor)) {
            return 7;
        }
    }

    {
        CppState state;
        const std::u32string_view numbers = U"0xFF 1.5e-3 0b1010 42";
        const std::vector<TokenSpan> spans = highlightCppLine(numbers, state);
        if (!hasToken(spans, numbers, U"0xFF", TokenRole::Number) ||
            !hasToken(spans, numbers, U"1.5e-3", TokenRole::Number) ||
            !hasToken(spans, numbers, U"0b1010", TokenRole::Number) ||
            !hasToken(spans, numbers, U"42", TokenRole::Number)) {
            return 8;
        }
    }

    {
        CppState state;
        const std::u32string_view first = U"int x = 1; /* comment";
        const std::vector<TokenSpan> firstSpans = highlightCppLine(first, state);
        if (!state.inBlockComment ||
            !hasToken(firstSpans, first, U"/* comment", TokenRole::Comment)) {
            return 9;
        }

        const std::u32string_view second = U"still comment */ int y = 2;";
        const std::vector<TokenSpan> secondSpans = highlightCppLine(second, state);
        if (state.inBlockComment) {
            return 10;
        }
        if (!coversLine(secondSpans, second) ||
            !hasToken(secondSpans, second, U"still comment */", TokenRole::Comment) ||
            !hasToken(secondSpans, second, U"int", TokenRole::Type)) {
            return 11;
        }
    }

    {
        CppState state;
        const std::u32string_view line = U"auto value = compute(1, \"text\");";
        const std::vector<TokenSpan> spans = highlightCppLine(line, state);
        if (!hasToken(spans, line, U"auto", TokenRole::Type) ||
            !hasToken(spans, line, U"compute", TokenRole::Function) ||
            !hasToken(spans, line, U"\"text\"", TokenRole::String) ||
            !hasToken(spans, line, U"1", TokenRole::Number)) {
            return 12;
        }
    }

    {
        CppState state;
        const std::u32string_view open = U"/* @build:macOS";
        const std::vector<TokenSpan> openSpans = highlightCppLine(open, state);
        if (!coversLine(openSpans, open) || !state.inBlockComment ||
            !state.inCompileBlock ||
            !hasToken(openSpans, open, U"@build:macOS", TokenRole::CompileFunction) ||
            !hasToken(openSpans, open, U"/*", TokenRole::CompileComment)) {
            return 13;
        }

        const std::u32string_view command = U"$gcc %project%/main.cpp -o app";
        const std::vector<TokenSpan> commandSpans = highlightCppLine(command, state);
        if (!coversLine(commandSpans, command) ||
            !hasToken(commandSpans, command, U"$gcc ", TokenRole::CompileCommand) ||
            !hasToken(commandSpans, command, U"%project%",
                      TokenRole::CompileVariable)) {
            return 14;
        }

        const std::u32string_view end = U"@end";
        const std::vector<TokenSpan> endSpans = highlightCppLine(end, state);
        if (!coversLine(endSpans, end) ||
            !hasToken(endSpans, end, U"@end", TokenRole::CompileDirective)) {
            return 15;
        }

        const std::u32string_view close = U"*/";
        const std::vector<TokenSpan> closeSpans = highlightCppLine(close, state);
        if (state.inBlockComment || state.inCompileBlock ||
            !coversLine(closeSpans, close)) {
            return 16;
        }
    }

    return 0;
}
