#pragma once
#include "modules/Coverage.hpp"
#include <string>
#include <vector>
namespace lucid::modules {
struct CoverageCounts {
    std::size_t linesCovered = 0, linesUncovered = 0;
    std::size_t branchesCovered = 0, branchesUncovered = 0, branchesUnknown = 0;
};
struct CoverageFileSummary { std::string path; CoverageCounts counts; };
struct CoverageSummary { CoverageCounts total; std::vector<CoverageFileSummary> files; bool incomplete = false; std::size_t skippedFiles = 0; };
// Groups by exact project-relative path and preserves incompleteness from import.
// Unknown branches remain separate and must not be treated as uncovered or
// included in a known-branch percentage. Returns false on duplicates/overflow.
[[nodiscard]] bool summarizeCoverage(const CoverageReport& report, CoverageSummary& summary, std::string& error);

enum class CoverageChangeKind { Regressed, Improved, BranchCountsChanged, BaselineOnly, CurrentOnly };
struct CoverageChange {
    std::string path;
    std::size_t line = 0; // Zero-based, as in CoverageLine.
    CoverageChangeKind kind = CoverageChangeKind::BranchCountsChanged;
    CoverageLine baseline, current; // Only the matching side is meaningful for one-sided entries.
};
struct CoverageComparison {
    std::vector<CoverageChange> changes;
    bool baselineIncomplete = false, currentIncomplete = false;
    std::size_t baselineSkippedFiles = 0, currentSkippedFiles = 0;
};
// Exact path and line matching only; source edits can move lines or change their
// meaning. One-sided entries are not classified as regressions. Unknown branch
// counts are retained separately. No file operations are performed.
[[nodiscard]] bool compareCoverage(const CoverageReport& baseline, const CoverageReport& current,
                                   CoverageComparison& comparison, std::string& error);
}
