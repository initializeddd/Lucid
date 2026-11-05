#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::modules {
struct DesignToken { std::string name, value, type; std::size_t line = 0, byteOffset = 0; };
struct DesignTokenReport {
    std::vector<DesignToken> tokens;
    std::vector<std::string> warnings;
    bool warningsTruncated = false;
};
// Parses a selected document supplied by the caller; no file discovery or I/O.
// JSON accepts nested primitive leaves and DTCG-style objects with primitive
// $value. CSS accepts simple, one-line custom property declarations only.
// Returns false on invalid/oversized input and clears the report on failure.
[[nodiscard]] bool extractJsonDesignTokens(std::string_view text, DesignTokenReport& report, std::string& error);
[[nodiscard]] bool extractCssDesignTokens(std::string_view text, DesignTokenReport& report, std::string& error);

struct CssTokenUsage { std::string name; std::size_t line = 0, column = 0, byteOffset = 0; };
struct CssTokenUsageReport { std::vector<CssTokenUsage> usages; bool truncated = false; };
// Finds simple var(--name) references in supplied CSS, including multiline
// whitespace, ignoring comments and strings. Line and byte column are one-based. Escaped identifiers and dynamic
// syntax are intentionally unsupported; this is not a CSS parser.
[[nodiscard]] bool findCssTokenUsages(std::string_view text, CssTokenUsageReport& report, std::string& error);
}
