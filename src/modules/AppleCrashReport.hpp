#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
struct AppleCrashFrame { std::uint32_t index=0; std::string image, text; std::uint64_t address=0; };
struct AppleCrashThread { std::uint32_t number=0; bool crashed=false; std::vector<AppleCrashFrame> frames; };
struct AppleCrashReport { std::string error, exceptionType, architecture; std::vector<AppleCrashThread> threads; std::size_t ignoredLines=0; bool incomplete=false; };
// Conservative parser for legacy, plain-text macOS crash reports. Supplied
// content is untrusted, including addresses and symbols. No I/O or symbolication.
[[nodiscard]] AppleCrashReport parseAppleCrashReport(std::string_view text, std::size_t maxBytes=4*1024*1024, std::size_t maxThreads=256, std::size_t maxFrames=4096);
}
