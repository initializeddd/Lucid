#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
struct IpsImage { std::string name, uuid; std::uint64_t base=0, size=0; };
struct IpsFrame { std::uint64_t imageOffset=0; std::size_t imageIndex=0; std::string symbol; };
struct IpsThread { bool triggered=false; std::vector<IpsFrame> frames; };
struct AppleIpsReport { std::string error, architecture, exceptionType, signal; std::vector<IpsImage> images; std::vector<IpsThread> threads; bool incomplete=false; };
// Parses a bounded subset of Apple JSON .ips, optionally preceded by the
// standard single-line JSON metadata header. No I/O or symbolication. All
// fields are unverified; process details, file paths and other private data are
// deliberately not returned. Image indexes refer only to returned images.
[[nodiscard]] AppleIpsReport parseAppleIpsReport(std::string_view text, std::size_t maxBytes=4*1024*1024, std::size_t maxThreads=256, std::size_t maxFrames=4096, std::size_t maxImages=4096);
}
