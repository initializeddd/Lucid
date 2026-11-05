#pragma once
#include "modules/CrashDump.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
struct CrashSourceRange { std::string module, path, symbol; std::uint64_t start = 0, end = 0; std::size_t line = 0; };
struct CrashSourceMap { std::vector<CrashSourceRange> ranges; std::string error; bool incomplete = false; };
struct CrashLocation { std::string module, path, symbol; std::uint64_t moduleOffset = 0; std::size_t line = 0; bool moduleAmbiguous = false, sourceAmbiguous = false; };
// TSV: exact module name, hexadecimal start RVA, exclusive end RVA,
// project-relative source path, 1-based line, symbol. UTF-8 text is not decoded.
// Caller must independently verify source files before navigation.
[[nodiscard]] CrashSourceMap parseCrashSourceMap(std::string_view text, std::size_t maxBytes = 1024*1024, std::size_t maxRanges = 10000);
[[nodiscard]] CrashLocation locateCrashAddress(std::uint64_t address, const std::vector<CrashModule>& modules, const CrashSourceMap& map);
}
