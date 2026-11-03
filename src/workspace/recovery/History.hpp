#pragma once
#include "workspace/recovery/Snapshot.hpp"
#include <filesystem>
#include <string>
#include <vector>
namespace lucid::workspace::recovery {
// Save a bounded history of the pre-save disk contents, separate from crash snapshots.
[[nodiscard]] bool recordHistory(const std::filesystem::path& root, const Snapshot& snapshot, std::string& error);
[[nodiscard]] bool listHistory(const std::filesystem::path& root, std::string_view project,
                               std::vector<std::filesystem::path>& paths, std::string& error);
}
