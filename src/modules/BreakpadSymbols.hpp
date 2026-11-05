#pragma once
#include "modules/CrashSymbols.hpp"
#include <cstddef>
#include <string>
#include <string_view>
namespace lucid::modules {
struct BreakpadSymbols { CrashSourceMap sourceMap; std::string module, debugId, architecture, operatingSystem, error; bool incomplete = false; };
enum class BreakpadIdentity { Match, Mismatch, Unavailable };
// Match confirms metadata identity only, not trustworthy symbols or current source.
[[nodiscard]] BreakpadIdentity compareBreakpadIdentity(const BreakpadSymbols& symbols, const CrashModule& module, std::string_view dumpArchitecture);
// Parses a bounded local Breakpad .sym text; no symbol tools, downloads or execution.
// The caller must match module name, architecture and debug ID against trusted dump
// metadata before using this mapping; source contents remain unverified.
[[nodiscard]] BreakpadSymbols parseBreakpadSymbols(std::string_view text, std::size_t maxBytes = 8*1024*1024, std::size_t maxRanges = 10000);
}
