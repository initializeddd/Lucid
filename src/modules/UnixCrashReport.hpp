#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
struct UnixCrashFrame { std::uint32_t index=0; std::optional<std::uint64_t> address; std::string text; };
struct UnixCrashThread { std::uint32_t number=0; std::string heading; std::vector<UnixCrashFrame> frames; };
struct UnixCrashReport { std::string error, signal; std::vector<UnixCrashThread> threads; std::size_t ignoredLines=0; bool incomplete=false; };
// Parses a conservative subset of text saved from GDB's "thread apply all bt".
// All content, addresses and thread numbers are untrusted text, not verified debug data.
// No path navigation, file access, command execution, or symbolication is performed.
[[nodiscard]] UnixCrashReport parseGdbBacktrace(std::string_view text, std::size_t maxBytes=4*1024*1024, std::size_t maxThreads=256, std::size_t maxFrames=4096);
// Parses saved LLDB `thread backtrace all` output; all text is unverified.
[[nodiscard]] UnixCrashReport parseLldbBacktrace(std::string_view text, std::size_t maxBytes=4*1024*1024, std::size_t maxThreads=256, std::size_t maxFrames=4096);
}
