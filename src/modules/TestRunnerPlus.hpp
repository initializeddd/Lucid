#pragma once
#include "editor/json/Json.hpp"
#include <string>
#include <string_view>
#include <optional>
#include <vector>
namespace lucid::modules {
struct DiscoveredTest { std::string name; std::vector<std::string> command; editor::JsonValue properties; };
struct TestDiscovery { std::vector<DiscoveredTest> tests; };
enum class TestResultStatus { Passed, Failed, Skipped };
struct TestResult { std::string name, className, duration, failureMessage, failureText, output, errorOutput; TestResultStatus status = TestResultStatus::Passed; };
struct TestResults { std::vector<TestResult> tests; };
enum class TestMatchStatus { Matched, DiscoveryOnly, ResultOnly, Ambiguous };
struct TestCorrelation { std::string name; TestMatchStatus status = TestMatchStatus::Ambiguous; std::vector<std::size_t> discoveryIndices, resultIndices; };
enum class TestHistoryStatus { Unchanged, Changed, PreviousOnly, CurrentOnly, Ambiguous };
struct TestHistoryEntry { std::string name; TestHistoryStatus status = TestHistoryStatus::Ambiguous; std::vector<std::size_t> previousIndices, currentIndices; };
struct TestHistorySummary {
    std::string name;
    std::size_t observations = 0, passed = 0, failed = 0, skipped = 0, ambiguousRuns = 0, consecutiveFailures = 0;
    std::optional<TestResultStatus> latestStatus;
    std::optional<double> latestDurationSeconds, previousDurationSeconds, durationDeltaSeconds;
    std::optional<std::size_t> latestRunIndex;
    std::size_t passFailTransitions = 0, longestFailureStreak = 0;
    std::optional<std::size_t> mostRecentFailureRunIndex;
};
// Exact names only; duplicate names on either side are ambiguous. Indices
// reference the supplied vectors; callers retain the source runs and details.
[[nodiscard]] std::vector<TestCorrelation> correlateCTestResults(const TestDiscovery& discovery, const TestResults& results);
[[nodiscard]] std::vector<TestHistoryEntry> compareCTestRuns(const TestResults& previous, const TestResults& current);
// Summarizes oldest-first saved runs. An absent test is unobserved (runs can
// contain a single selected test), while a duplicate name makes that run
// ambiguous and breaks any streak; skipped results also break pass/fail
// transitions. Transitions are evidence for triage, not proof of flakiness.
// No failed output is retained.
[[nodiscard]] std::vector<TestHistorySummary> summarizeCTestHistory(const std::vector<TestResults>& runs);
// True only for names that CTestRunner can target with a single exact regex.
[[nodiscard]] bool canRunCTestNameExactly(std::string_view name) noexcept;
enum class FailedRerunStatus { Ready, NotDiscovered, AmbiguousDiscovery, AmbiguousResult, UnsupportedName };
struct FailedRerunCandidate {
    std::string name;
    FailedRerunStatus status = FailedRerunStatus::NotDiscovered;
    std::size_t lastObservedRun = 0; // Zero-based index in the supplied oldest-first runs.
};
// Plans, but never executes, the latest observed failures across partial runs.
// A duplicate in the latest observation is ambiguous, even if its rows disagree.
// Re-discover and explicitly confirm before invoking each run: the plan can become stale.
[[nodiscard]] bool planFailedCTestReruns(const TestDiscovery& discovery, const std::vector<TestResults>& runs,
                                         std::vector<FailedRerunCandidate>& candidates, std::string& error);
// Parse CTest --show-only=json-v1 and --output-junit output respectively.
// Both functions accept bounded text, perform no process or file operations,
// and clear output on error. JUnit DTDs and custom entities are rejected.
[[nodiscard]] bool parseCTestDiscovery(std::string_view input, TestDiscovery& result, std::string& error);
[[nodiscard]] bool parseCTestJUnit(std::string_view input, TestResults& result, std::string& error);
// Strict generic import refuses unknown tags, unsupported test attributes and
// invalid hierarchy so unrecognized failure states cannot appear as passes.
[[nodiscard]] bool parseStrictJUnit(std::string_view input, TestResults& result, std::string& error);

}
