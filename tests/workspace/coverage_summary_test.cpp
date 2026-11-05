#include "modules/CoverageSummary.hpp"
#include <limits>
using namespace lucid::modules;
int main() {
    CoverageReport report; report.incomplete=true; report.skippedFiles=2;
    report.lines={{"b.cpp",0,true,1,1,1},{"a.cpp",3,false,0,2,0},{"a.cpp",4,true,0,0,0}};
    CoverageSummary summary; std::string error;
    if (!summarizeCoverage(report,summary,error) || !summary.incomplete || summary.skippedFiles!=2 || summary.files.size()!=2 || summary.files[0].path!="a.cpp" || summary.total.linesCovered!=2 || summary.total.linesUncovered!=1 || summary.total.branchesCovered!=1 || summary.total.branchesUncovered!=3 || summary.total.branchesUnknown!=1) return 1;
    report.lines.push_back(report.lines[0]); if (summarizeCoverage(report,summary,error) || !summary.files.empty()) return 2;
    report.lines={{"a",0,true,std::numeric_limits<std::size_t>::max(),0,0},{"b",0,true,1,0,0}};
    if (summarizeCoverage(report,summary,error) || !summary.files.empty()) return 3;
    report.lines.clear(); if (!summarizeCoverage(report,summary,error) || !summary.files.empty() || summary.total.linesCovered) return 4;
    CoverageReport oldReport, newReport;
    oldReport.lines = {{"a.cpp",0,true,2,0,1},{"a.cpp",1,false,0,1,0},{"a.cpp",2,true,1,0,0},{"old.cpp",4,true,0,0,0}};
    newReport.lines = {{"a.cpp",0,false,1,1,1},{"a.cpp",1,true,0,1,0},{"a.cpp",2,true,0,1,0},{"new.cpp",3,false,0,0,0}};
    oldReport.incomplete = true; newReport.skippedFiles = 1;
    CoverageComparison comparison;
    if (!compareCoverage(oldReport,newReport,comparison,error) || !comparison.baselineIncomplete || comparison.currentIncomplete || comparison.currentSkippedFiles != 1 || comparison.changes.size() != 5) return 5;
    if (comparison.changes[0].kind != CoverageChangeKind::Regressed || comparison.changes[0].baseline.branchesUnknown != 1 || comparison.changes[0].current.branchesUncovered != 1 ||
        comparison.changes[1].kind != CoverageChangeKind::Improved || comparison.changes[2].kind != CoverageChangeKind::BranchCountsChanged ||
        comparison.changes[3].kind != CoverageChangeKind::CurrentOnly || comparison.changes[4].kind != CoverageChangeKind::BaselineOnly) return 6;
    newReport.lines.push_back(newReport.lines[0]);
    if (compareCoverage(oldReport,newReport,comparison,error) || error.empty() || !comparison.changes.empty()) return 7;
    newReport.lines.pop_back(); oldReport.lines[0].path.clear();
    if (compareCoverage(oldReport,newReport,comparison,error) || error.empty() || !comparison.changes.empty()) return 8;
    oldReport.lines.clear(); newReport.lines.clear();
    if (!compareCoverage(oldReport,newReport,comparison,error) || !comparison.changes.empty() || !comparison.baselineIncomplete || comparison.currentSkippedFiles != 1) return 9;
    return 0;
}
