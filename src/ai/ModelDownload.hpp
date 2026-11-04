#pragma once

#include <filesystem>
#include <atomic>
#include <string>

namespace lucid::ai {
inline constexpr int kModelGenerating = -1;
inline constexpr int kModelVerifying = -2;
inline constexpr int kModelInstalling = -3;
// Downloads and verifies the pinned model if necessary. Run on a worker thread.
[[nodiscard]] bool ensureModel(const std::filesystem::path& directory,
                               std::filesystem::path& model, std::string& error,
                               const std::atomic_bool* cancel = nullptr,
                               std::atomic<int>* progress = nullptr);
}
