#include "modules/BenchmarkComparison.hpp"
#include <cmath>
#include <string>
using namespace lucid::modules;
int main() {
    const std::string base = R"({"context":{"machine":"m","build":"release","environment":"e","revision":"a"},"measurements":[{"name":"latency","unit":"ms","direction":"lower","value":100},{"name":"throughput","unit":"ops/s","direction":"higher","value":100},{"name":"zero","unit":"ms","direction":"lower","value":0}]})";
    const std::string next = R"({"context":{"machine":"m","build":"release","environment":"e","revision":"b"},"measurements":[{"name":"latency","unit":"ms","direction":"lower","value":120},{"name":"throughput","unit":"ops/s","direction":"higher","value":120},{"name":"zero","unit":"ms","direction":"lower","value":1},{"name":"new","unit":"ms","direction":"lower","value":1}]})";
    BenchmarkRun a, b; std::string error;
    if (!parseBenchmarkRun(base, a, error) || !parseBenchmarkRun(next, b, error)) return 1;
    auto comparison = compareBenchmarkRuns(a, b);
    if (comparison.entries.size() != 4 || comparison.entries[0].assessment != BenchmarkAssessment::PossibleRegression ||
        comparison.entries[1].assessment != BenchmarkAssessment::CandidateOnly || comparison.entries[2].assessment != BenchmarkAssessment::Improvement ||
        comparison.entries[3].assessment != BenchmarkAssessment::Incomparable || comparison.entries[3].hasPercentChange) return 2;
    b.context.machine = "other"; comparison = compareBenchmarkRuns(a, b);
    if (comparison.entries[0].assessment != BenchmarkAssessment::Incomparable) return 3;
    b.context.machine.clear(); comparison = compareBenchmarkRuns(a, b);
    if (comparison.entries[0].assessment != BenchmarkAssessment::Incomparable) return 4;
    if (parseBenchmarkRun(R"({"measurements":[{"name":"x","unit":"s","direction":"lower","value":1},{"name":"x","unit":"s","direction":"lower","value":2}]})", b, error) || !b.measurements.empty()) return 5;
    if (parseBenchmarkRun(R"({"measurements":[{"name":"x","unit":"s","direction":"lower","value":-1}]})", b, error)) return 6;
    b = a; b.measurements[0].unit = "ns"; b.measurements[0].value = 150000000; comparison = compareBenchmarkRuns(a, b);
    if (comparison.entries[0].assessment != BenchmarkAssessment::PossibleRegression || comparison.entries[0].baselineUnit != "ms" || comparison.entries[0].candidateUnit != "ns" || std::abs(comparison.entries[0].percentChange-50)>1e-9) return 7;
    const std::string google = R"({"context":{"host_name":"host","build":"release","environment":"linux","date":"today","num_cpus":8,"caches":[{"type":"Data"}]},"benchmarks":[
      {"name":"BM/a","run_name":"BM/a","run_type":"iteration","repetition_index":0,"real_time":10,"cpu_time":8,"time_unit":"ns"},
      {"name":"BM/a","run_name":"BM/a","run_type":"iteration","repetition_index":1,"real_time":14,"cpu_time":10,"time_unit":"ns"},
      {"name":"BM/a_mean","run_name":"BM/a","run_type":"aggregate","aggregate_name":"mean","aggregate_unit":"time","real_time":12,"cpu_time":9,"time_unit":"ns"},
      {"name":"BM/a_stddev","run_name":"BM/a","run_type":"aggregate","aggregate_name":"stddev","real_time":2,"cpu_time":1,"time_unit":"ns"},
      {"name":"failed","error_occurred":true}]})";
    if (!parseBenchmarkRun(google, a, error) || a.context.machine != "host" || a.measurements.size() != 2 ||
        a.measurements[0].value != 9 || a.measurements[1].value != 12 || a.measurements[1].samples != 2 ||
        a.measurements[1].statistic != "mean" || !a.importedContext.find("caches")) return 8;
    b = a; b.measurements[1].value = 15; comparison = compareBenchmarkRuns(a, b);
    if (comparison.entries[1].assessment != BenchmarkAssessment::PossibleRegression || comparison.entries[1].baselineSamples != 2 || comparison.entries[1].candidateSamples != 2 || comparison.entries[1].baselineStatistic != "mean") return 9;
    b.measurements[1].statistic = "single"; comparison = compareBenchmarkRuns(a, b);
    if (comparison.entries[1].assessment != BenchmarkAssessment::Incomparable) return 10;
    const std::string aggregateOnly = R"({"context":{"host_name":"host","build":"release","environment":"linux"},"benchmarks":[{"name":"BM/a_mean","run_name":"BM/a","run_type":"aggregate","aggregate_name":"mean","real_time":12,"time_unit":"ns"}]})";
    if (!parseBenchmarkRun(aggregateOnly, b, error) || b.measurements.size() != 1 || b.measurements[0].samples != 0 || b.measurements[0].statistic != "mean") return 11;
    if (parseBenchmarkRun(R"({"benchmarks":[{"name":"x","time_unit":"ns","real_time":-1}]})", b, error)) return 12;
    if (!parseBenchmarkRun(R"({"context":{"host_name":"host"},"benchmarks":[{"name":"x","time_unit":"ns","real_time":1}]})", b, error)) return 13;
    comparison = compareBenchmarkRuns(b, b);
    if (comparison.entries[0].assessment != BenchmarkAssessment::Incomparable) return 14;
    const std::string samples=R"({"context":{"machine":"m","build":"release","environment":"e"},"measurements":[{"name":"latency","unit":"ms","direction":"lower","samples":[8,10,12]}]})";
    if(!parseBenchmarkRun(samples,a,error) || a.measurements.size()!=1 || a.measurements[0].value!=10 || a.measurements[0].samples!=3 || !a.measurements[0].hasSampleSpread || a.measurements[0].sampleMin!=8 || a.measurements[0].sampleMax!=12 || std::abs(a.measurements[0].sampleStddev-2)>1e-12) return 15;
    b=a; b.measurements[0].value=11; auto spread=compareBenchmarkRuns(a,b); if(!spread.entries[0].baselineHasSpread || !spread.entries[0].candidateHasSpread || spread.entries[0].baselineStddev!=2 || spread.entries[0].assessment!=BenchmarkAssessment::PossibleRegression) return 16;
    for(const auto* malformed:{R"({"measurements":[{"name":"x","unit":"s","direction":"lower","samples":[]}]})",R"({"measurements":[{"name":"x","unit":"s","direction":"lower","samples":[1,-1]}]})",R"({"measurements":[{"name":"x","unit":"s","direction":"lower","samples":[1],"value":1}]})",R"({"measurements":[{"name":"x","unit":"s","direction":"lower","samples":[1],"samples":[2]}]})"}) if(parseBenchmarkRun(malformed,b,error) || !b.measurements.empty()) return 17;
    if(!parseBenchmarkRun(R"({"measurements":[{"name":"x","unit":"s","direction":"lower","samples":[1]}]})",b,error) || b.measurements[0].hasSampleSpread || b.measurements[0].samples!=1 || b.measurements[0].statistic!="single") return 18;
    if(!parseBenchmarkRun(R"({"measurements":[{"name":"x","unit":"s","direction":"lower","samples":[0,1e200]}]})",b,error) || b.measurements[0].hasSampleSpread || !std::isfinite(b.measurements[0].value)) return 19;
    if(!parseBenchmarkRun(google,b,error) || !b.measurements[1].hasSampleSpread || b.measurements[1].sampleMin!=10 || b.measurements[1].sampleMax!=14 || std::abs(b.measurements[1].sampleStddev-std::sqrt(8.0))>1e-12) return 20;
    if(!parseBenchmarkRun(aggregateOnly,b,error) || b.measurements[0].hasSampleSpread) return 21;
    if(!parseBenchmarkRun(base,a,error)) return 22;
    b=a; b.measurements[0].unit="s"; b.measurements[0].value=0.1; comparison=compareBenchmarkRuns(a,b);
    if(comparison.entries[0].assessment!=BenchmarkAssessment::WithinThreshold || std::abs(comparison.entries[0].percentChange)>1e-9 || comparison.entries[0].baseline!=100 || comparison.entries[0].candidate!=0.1) return 23;
    b.measurements[0].unit="minutes"; comparison=compareBenchmarkRuns(a,b);
    if(comparison.entries[0].assessment!=BenchmarkAssessment::Incomparable || comparison.entries[0].hasPercentChange) return 24;
    b.measurements[0].unit="us"; b.measurements[0].value=120000; b.measurements[0].lowerIsBetter=false; comparison=compareBenchmarkRuns(a,b);
    if(comparison.entries[0].assessment!=BenchmarkAssessment::Incomparable) return 25;
    b=a; b.measurements[0].unit="us"; b.measurements[0].value=120000; b.context.machine="other"; comparison=compareBenchmarkRuns(a,b);
    if(comparison.entries[0].assessment!=BenchmarkAssessment::Incomparable || comparison.entries[0].hasPercentChange) return 26;
    return 0;
}
