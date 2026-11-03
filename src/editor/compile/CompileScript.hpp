#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace lucid::editor {

enum class CompilePlatform {
    Any,
    MacOS,
    Windows,
    Linux,
    FreeBSD,
    OpenBSD,
};

struct CompileCommand {
    std::string text;
    int line = 0;
};

struct CompileFunction {
    std::string name;
    CompilePlatform platform = CompilePlatform::Any;
    int definitionLine = 0;
    int endLine = 0;
    int directiveColumn = 0;
    int directiveLength = 0;
    std::vector<CompileCommand> commands;

    [[nodiscard]] bool runsHere() const noexcept;
};

struct CompileScript {
    std::vector<CompileFunction> functions;
};

[[nodiscard]] CompilePlatform currentCompilePlatform() noexcept;
[[nodiscard]] CompilePlatform parseCompilePlatform(std::string_view text) noexcept;
[[nodiscard]] CompileScript parseCompileScript(const std::vector<std::u32string>& lines);
[[nodiscard]] const CompileFunction* findCompileFunctionAt(const CompileScript& script,
                                                           int line) noexcept;
[[nodiscard]] std::string
expandCompileVariables(std::string_view command,
                       const std::unordered_map<std::string, std::string>& variables);
[[nodiscard]] std::string shellQuote(std::string_view value);

} // namespace lucid::editor
