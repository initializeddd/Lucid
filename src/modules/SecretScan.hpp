#pragma once
#include "workspace/Project.hpp"
#include "workspace/git/GitReview.hpp"
#include <atomic>
#include <cstddef>
#include <string>
#include <vector>

namespace lucid::modules {
struct SecretFinding { std::string path, kind; std::size_t line = 0, column = 0; std::string contentSha256; };
struct SecretScanResult {
    std::vector<SecretFinding> findings;
    std::size_t skipped = 0;
    bool incomplete = false;
};
// Results intentionally omit secret values. The caller controls indexing and
// must make clear that this pattern-based scan cannot prove a project is safe.
[[nodiscard]] SecretScanResult scanSecrets(const workspace::Project& project, const std::atomic_bool* cancel = nullptr,
                                           std::size_t limit = 1000);
[[nodiscard]] SecretScanResult scanSecretText(std::string_view path, std::string_view contents,
                                              const std::atomic_bool* cancel = nullptr, std::size_t limit = 1000);
[[nodiscard]] SecretScanResult scanStagedSecrets(const std::filesystem::path& root, const workspace::GitCommitPreview& preview,
                                                 const std::atomic_bool* cancel = nullptr, std::size_t limit = 1000);
}
