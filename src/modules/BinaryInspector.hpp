#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::modules {
struct BinarySection { std::string name; std::uint64_t offset = 0, size = 0; };
struct BinaryPeSymbol { std::string name, library, forwarder; std::uint32_t ordinal = 0, rva = 0; bool byOrdinal = false; };
struct BinarySlice { std::string architecture; std::uint64_t offset = 0, size = 0; };
struct BinaryInspection {
    std::string format, architecture, error;
    std::vector<BinarySlice> slices;
    std::vector<BinarySection> sections;
    std::vector<std::string> symbols, libraries;
    std::vector<BinaryPeSymbol> peImports, peExports;
    bool incomplete = false;
};
// Parses untrusted bytes without running external tools. Universal Mach-O defaults to the first slice; offsets are relative to the whole file.
[[nodiscard]] BinaryInspection inspectBinary(std::string_view bytes, std::size_t itemLimit = 10000);
// Selects a universal Mach-O slice by index. Invalid indices return an error.
[[nodiscard]] BinaryInspection inspectBinarySlice(std::string_view bytes, std::size_t sliceIndex, std::size_t itemLimit = 10000);
}
