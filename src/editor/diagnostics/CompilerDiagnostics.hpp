#pragma once

#include "editor/diagnostics/Diagnostics.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace lucid::editor {

struct CompilerRequest {
    std::string filePath;
    std::string projectRoot;
    std::string content;
    bool cpp = true;
};

struct CompilerResult {
    std::vector<Diagnostic> diagnostics;
    std::string error;
};

[[nodiscard]] std::vector<Diagnostic>
parseCompilerDiagnostics(std::string_view output, std::string_view sourcePath);
[[nodiscard]] CompilerResult runCompilerDiagnostics(const CompilerRequest& request);

} // namespace lucid::editor
