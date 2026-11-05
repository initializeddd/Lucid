#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
struct LinkerRegion { std::string name, attributes; std::uint64_t origin = 0, length = 0, used = 0, loadUsed = 0; };
struct LinkerSection { std::string name; std::uint64_t address = 0, size = 0; int region = -1; bool hasLoadAddress = false; std::uint64_t loadAddress = 0; int loadRegion = -1; };
struct LinkerMap { std::vector<LinkerRegion> regions; std::vector<LinkerSection> sections; bool incomplete = false, loadIncomplete = false; };
// GNU ld subset: "Memory Configuration" table and top-level output sections
// following "Linker script and memory map". Usage is the union of mapped output
// section address ranges, not a linker-reported allocation total. Load usage counts
// only sections with an explicit "load address"; missing or ambiguous data marks
// loadIncomplete. VMA and LMA totals are independent and must not be summed. No I/O occurs.
[[nodiscard]] bool parseGnuLinkerMap(std::string_view text, LinkerMap& result, std::string& error);
}
