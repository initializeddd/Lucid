#pragma once

#include <filesystem>
#include <atomic>
#include <string>

namespace lucid::ai {
// Downloads and verifies the pinned model if necessary. Run on a worker thread.
[[nodiscard]] bool ensureModel(const std::filesystem::path& directory,
                               std::filesystem::path& model, std::string& error,
                               const std::atomic_bool* cancel = nullptr);
}
