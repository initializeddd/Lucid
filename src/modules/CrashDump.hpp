#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
enum class CrashFrameSource { Context, FramePointerHeuristic };
struct CrashFrame { std::uint64_t address = 0, stackPointer = 0; CrashFrameSource source = CrashFrameSource::Context; };
struct CrashThread {
    std::uint32_t id = 0; std::uint64_t stackStart = 0; std::uint32_t stackBytes = 0;
    std::vector<CrashFrame> frames;
    // Context and frame-pointer chains cannot prove a complete unwind.
    bool framesIncomplete = false;
};
struct CrashException { bool present = false; std::uint32_t threadId = 0, code = 0, flags = 0; std::uint64_t address = 0; };
struct CrashModule { std::string name; std::uint64_t base = 0; std::uint32_t size = 0; std::string debugId = {}; };
struct CrashDump { std::string format, error, architecture; CrashException exception; std::vector<CrashThread> threads; std::vector<CrashModule> modules; bool incomplete = false; };
// Parses supplied minidump bytes only. x86-64 contexts provide an initial PC
// and up to 63 plausible frame-pointer-chain candidates from the saved stack.
// Candidates are heuristic, may be false, and are not a reliable stack unwind.
// At most 4096 threads and 4096 modules are retained even if larger limits are requested.
// No file access, external tools, unwind metadata or symbolication are used.
[[nodiscard]] CrashDump readCrashDump(std::string_view bytes, std::size_t threadLimit = 4096, std::size_t moduleLimit = 4096);
}
