#pragma once
#include "editor/json/Json.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace lucid::modules {
// Supports Google Benchmark JSON (real_time and cpu_time) and this schema: {"context":{"machine":"...","build":"...","environment":"...",
// "revision":"..."},"measurements":[{"name":"...","unit":"...",
// "direction":"lower"|"higher","value":number}]}. Context fields are optional;
// absent context prevents a regression classification. Revision is informational.
// Google context is retained in importedContext; host_name supplies machine, while
// build and environment must be provided as custom context. Repetitions become
// arithmetic means; aggregate-only mean rows have an unknown sample count (0).
struct BenchmarkContext { std::string machine, build, environment, revision; };
struct BenchmarkMeasurement {
    std::string name, unit; bool lowerIsBetter = true; double value = 0; std::string statistic; std::size_t samples = 1;
    // Descriptive spread for raw repeated measurements only. Aggregate means
    // and single values have no spread. It is not a significance test.
    bool hasSampleSpread = false; double sampleMin = 0, sampleMax = 0, sampleStddev = 0;
};
struct BenchmarkRun { BenchmarkContext context; std::vector<BenchmarkMeasurement> measurements; editor::JsonValue importedContext; };
enum class BenchmarkAssessment { PossibleRegression, WithinThreshold, Improvement, Incomparable, BaselineOnly, CandidateOnly };
struct BenchmarkComparisonEntry {
    std::string name, unit, reason;
    std::string baselineUnit, candidateUnit, baselineStatistic, candidateStatistic;
    std::size_t baselineSamples = 0, candidateSamples = 0;
    bool baselineLowerIsBetter = true, candidateLowerIsBetter = true;
    BenchmarkAssessment assessment = BenchmarkAssessment::Incomparable;
    double baseline = 0, candidate = 0, percentChange = 0;
    bool hasPercentChange = false;
    bool baselineHasSpread = false, candidateHasSpread = false;
    double baselineMin = 0, baselineMax = 0, baselineStddev = 0;
    double candidateMin = 0, candidateMax = 0, candidateStddev = 0;
};
struct BenchmarkComparison { std::vector<BenchmarkComparisonEntry> entries; };
// On failure, clears run and sets error. Input is bounded to 4 MiB / 10,000 entries.
[[nodiscard]] bool parseBenchmarkRun(std::string_view json, BenchmarkRun& run, std::string& error);
// Time measurements using ns, us, ms or s are compared in a common unit;
// the entry retains each original value, unit and sample spread. Other unit
// mismatches remain incomparable. Threshold is a nonnegative percentage. A possible regression is an observed
// change, not statistical proof; repeated measurements are not inferred.
[[nodiscard]] BenchmarkComparison compareBenchmarkRuns(const BenchmarkRun& baseline, const BenchmarkRun& candidate,
                                                        double thresholdPercent = 5.0);
}
