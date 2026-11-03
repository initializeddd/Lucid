#pragma once

#include <filesystem>
#include <atomic>
#include <string>
#include <string_view>

namespace lucid::ai {

// Runs a short request locally. Call on a worker thread, never on the UI thread.
[[nodiscard]] bool generate(const std::filesystem::path& modelPath,
                            std::string_view request, std::string& output,
                            std::string& error, const std::atomic_bool* cancel = nullptr);

} // namespace lucid::ai
