#include "modules/CoverageSummary.hpp"
#include <limits>
#include <map>
#include <set>
#include <utility>
namespace lucid::modules {
namespace {
bool add(std::size_t& a, std::size_t b) { if (b > std::numeric_limits<std::size_t>::max() - a) return false; a += b; return true; }
bool record(CoverageCounts& c, const CoverageLine& line) {
    return add(line.covered ? c.linesCovered : c.linesUncovered,1) && add(c.branchesCovered,line.branchesCovered) &&
        add(c.branchesUncovered,line.branchesUncovered) && add(c.branchesUnknown,line.branchesUnknown);
}
}
bool summarizeCoverage(const CoverageReport& report, CoverageSummary& summary, std::string& error) {
    summary = {}; error.clear();
    if (report.lines.size() > 100000) { error = "Coverage summary exceeds line limit"; return false; }
    CoverageSummary result; result.incomplete=report.incomplete; result.skippedFiles=report.skippedFiles;
    std::map<std::string,CoverageCounts> files; std::set<std::pair<std::string,std::size_t>> seen;
    for (const auto& line : report.lines) {
        if (line.path.empty() || !seen.emplace(line.path,line.line).second) { error = "Invalid or duplicate coverage line"; return false; }
        if (!record(files[line.path],line) || !record(result.total,line)) { error = "Coverage count overflow"; return false; }
    }
    for (auto& [path,counts] : files) result.files.push_back({path,counts});
    summary = std::move(result); return true;
}
bool compareCoverage(const CoverageReport& baseline, const CoverageReport& current,
                     CoverageComparison& comparison, std::string& error) {
    comparison = {}; error.clear();
    CoverageSummary ignored;
    if (!summarizeCoverage(baseline, ignored, error)) return false;
    if (!summarizeCoverage(current, ignored, error)) return false;
    using Key = std::pair<std::string, std::size_t>;
    struct Pair { const CoverageLine* baseline = nullptr; const CoverageLine* current = nullptr; };
    std::map<Key, Pair> entries;
    for (const auto& line : baseline.lines) entries[{line.path, line.line}].baseline = &line;
    for (const auto& line : current.lines) entries[{line.path, line.line}].current = &line;
    CoverageComparison result;
    result.baselineIncomplete = baseline.incomplete; result.currentIncomplete = current.incomplete;
    result.baselineSkippedFiles = baseline.skippedFiles; result.currentSkippedFiles = current.skippedFiles;
    for (const auto& [key, pair] : entries) {
        CoverageChangeKind kind;
        if (!pair.baseline) kind = CoverageChangeKind::CurrentOnly;
        else if (!pair.current) kind = CoverageChangeKind::BaselineOnly;
        else if (pair.baseline->covered != pair.current->covered) kind = pair.current->covered ? CoverageChangeKind::Improved : CoverageChangeKind::Regressed;
        else if (pair.baseline->branchesCovered != pair.current->branchesCovered || pair.baseline->branchesUncovered != pair.current->branchesUncovered || pair.baseline->branchesUnknown != pair.current->branchesUnknown) kind = CoverageChangeKind::BranchCountsChanged;
        else continue;
        CoverageChange change; change.path = key.first; change.line = key.second; change.kind = kind;
        if (pair.baseline) change.baseline = *pair.baseline;
        if (pair.current) change.current = *pair.current;
        result.changes.push_back(std::move(change));
    }
    comparison = std::move(result); return true;
}
}
