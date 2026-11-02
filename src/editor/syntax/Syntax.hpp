#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace lucid::editor {

enum class TokenRole {
    Plain,
    Keyword,
    Type,
    String,
    Number,
    Comment,
    Preprocessor,
    Function,
    Punctuation,
    Key,
    CompileComment,
    CompileDirective,
    CompileFunction,
    CompileCommand,
    CompileVariable,
};

struct TokenSpan {
    std::uint32_t start = 0;
    std::uint32_t length = 0;
    TokenRole role = TokenRole::Plain;
};

struct CppState {
    bool inBlockComment = false;
    bool inCompileBlock = false;
};

[[nodiscard]] std::vector<TokenSpan> highlightCppLine(std::u32string_view line,
                                                      CppState& state);

} // namespace lucid::editor
