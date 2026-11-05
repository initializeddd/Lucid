#pragma once
#include "modules/TestRunnerPlus.hpp"
#include <atomic>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
struct JUnitSummary { std::size_t passed=0, failed=0, skipped=0, ambiguous=0; };
struct JUnitReport { TestResults results; JUnitSummary summary; };
struct JUnitComparisonEntry {
    std::string className, name;
    TestHistoryStatus status=TestHistoryStatus::Ambiguous;
    std::vector<std::size_t> previousIndices, currentIndices;
};
// Imports one bounded JUnit XML report without running tests or fetching files.
// JUnit variants are not fully standardized; unsupported elements, hierarchy,
// and testcase attributes are rejected rather than silently treated as passing.
// Class/name pairs identify rows for comparison; missing classname is allowed,
// and duplicate pairs are counted as ambiguous. Output may contain secrets.
[[nodiscard]] bool importJUnitReport(std::string_view xml,JUnitReport& report,std::string& error,const std::atomic_bool* cancel=nullptr);
// Exact class/name matching only; duplicate pairs on either side are ambiguous.
// A missing test may reflect a partial run, not a removed test. No output text
// is copied into the comparison, and no source locations are inferred.
[[nodiscard]] bool compareJUnitReports(const JUnitReport& previous,const JUnitReport& current,
    std::vector<JUnitComparisonEntry>& comparison,std::string& error,const std::atomic_bool* cancel=nullptr);
}
