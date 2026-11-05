#pragma once
#include "workspace/Project.hpp"
#include <atomic>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
struct CoverageLine { std::string path; std::size_t line = 0; bool covered = false; std::size_t branchesCovered = 0, branchesUncovered = 0, branchesUnknown = 0; };
struct CoverageReport { std::vector<CoverageLine> lines; std::size_t skippedFiles = 0; bool incomplete = false; };
[[nodiscard]] CoverageReport parseLcov(const workspace::Project& project, std::string_view report,
                                       const std::atomic_bool* cancel = nullptr, std::size_t limit = 100000);
// Parses a bounded Cobertura subset without loading DTDs or external entities.
[[nodiscard]] CoverageReport parseCobertura(const workspace::Project& project, std::string_view report,
                                            const std::atomic_bool* cancel = nullptr, std::size_t limit = 100000);
}
