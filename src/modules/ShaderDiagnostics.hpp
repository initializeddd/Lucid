#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
struct ShaderDiagnostic { std::string path, severity, message; std::size_t line = 0, column = 0; };
struct ShaderDiagnostics { std::vector<ShaderDiagnostic> diagnostics; std::size_t unparsedLines = 0, scannedLines = 0; bool incomplete = false; };
// Parses supplied compiler output only; no compiler is executed. Limits apply
// to scanned bytes, lines and returned diagnostics.
[[nodiscard]] ShaderDiagnostics parseShaderDiagnostics(std::string_view output, std::size_t maxBytes = 1024 * 1024,
    std::size_t maxLines = 10000, std::size_t maxDiagnostics = 1000);
}
