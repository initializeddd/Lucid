#pragma once
#include <chrono>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>
namespace lucid::modules {
struct ModuleProcessResult { std::string output, error; int exitCode = -1; bool timedOut = false, outputTruncated = false, cancelled = false; };
// Runs the explicitly named executable without a shell; stdout and stderr are
// combined, stdin is closed, and output/time are bounded. No PATH lookup.
[[nodiscard]] ModuleProcessResult runModuleProcess(const std::filesystem::path& executable, const std::vector<std::string>& args,
    std::chrono::milliseconds timeout = std::chrono::seconds(30), std::size_t outputLimit = 1024 * 1024,
    const std::atomic_bool* cancel = nullptr, const std::filesystem::path& workingDirectory = {});
}
