#pragma once

#include "editor/compile/CompileScript.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::workspace {

class Project;

struct ProjectFunction {
    std::string name;
    std::string file;
    editor::CompilePlatform platform = editor::CompilePlatform::Any;
    int line = 0;
    int endLine = 0;
    int directiveColumn = 0;
    int directiveLength = 0;
    std::vector<editor::CompileCommand> commands;

    [[nodiscard]] bool runsHere() const noexcept;
};

struct FunctionIndex {
    std::vector<ProjectFunction> functions;
    bool usedCache = false;
};

[[nodiscard]] FunctionIndex loadFunctionIndex(Project& project, std::string& error);
[[nodiscard]] std::vector<std::size_t>
searchFunctions(const std::vector<ProjectFunction>& functions, std::string_view query,
                std::size_t limit);
[[nodiscard]] std::string_view functionPlatformName(editor::CompilePlatform platform) noexcept;

} // namespace lucid::workspace
