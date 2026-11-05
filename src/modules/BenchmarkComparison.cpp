#include "modules/BenchmarkComparison.hpp"
#include "editor/json/Json.hpp"
#include <cmath>
#include <algorithm>
#include <map>
#include <set>
#include <limits>

namespace lucid::modules {
namespace {
using editor::JsonType;
const editor::JsonValue* field(const editor::JsonValue& obj, const char* key, JsonType type) {
    const auto* value = obj.find(key); return value && value->type == type ? value : nullptr;
}
struct RunningSamples {
    std::size_t count=0; double mean=0, m2=0, minimum=0, maximum=0; bool spreadValid=true;
    void add(double value) {
        if(!count) minimum=maximum=value; else {minimum=std::min(minimum,value);maximum=std::max(maximum,value);}
        ++count; const double delta=value-mean; mean+=delta/static_cast<double>(count);
        if(spreadValid) {m2+=delta*(value-mean); if(!std::isfinite(m2) || m2<0) spreadValid=false;}
    }
    void apply(BenchmarkMeasurement& measurement) const {
        measurement.value=mean; measurement.samples=count; measurement.statistic=count>1?"mean":"single";
        if(count>1 && spreadValid) { const double sd=std::sqrt(m2/static_cast<double>(count-1)); if(std::isfinite(sd)) {measurement.hasSampleSpread=true;measurement.sampleMin=minimum;measurement.sampleMax=maximum;measurement.sampleStddev=sd;} }
    }
};
bool contextString(const editor::JsonValue& object, const char* key, std::string& result) {
    const auto* value = object.find(key);
    if (!value) return true;
    if (value->type != JsonType::String || value->stringValue.size() > 4096) return false;
    result = value->stringValue; return true;
}
// Time units are converted only when both sides use one of these exact,
// unambiguous spellings. Other units (including rates) require exact matches.
long double timeScale(std::string_view unit) {
    if (unit == "ns") return 1.0e-9L;
    if (unit == "us") return 1.0e-6L;
    if (unit == "ms") return 1.0e-3L;
    if (unit == "s") return 1.0L;
    return 0.0L;
}
}
bool parseBenchmarkRun(std::string_view json, BenchmarkRun& run, std::string& error) {
    run = {}; error.clear();
    if (json.size() > 4U * 1024U * 1024U) { error = "Benchmark report exceeds size limit"; return false; }
    editor::JsonError parseError; auto root = editor::parseJson(json, parseError);
    if (!root || root->type != JsonType::Object) { error = "Invalid benchmark report JSON"; return false; }
    BenchmarkRun result;
    const bool google = root->find("benchmarks") != nullptr;
    if (const auto* context = root->find("context")) {
        result.importedContext = *context;
        if (context->type != JsonType::Object || !contextString(*context, "machine", result.context.machine) ||
            !contextString(*context, "build", result.context.build) || !contextString(*context, "environment", result.context.environment) ||
            !contextString(*context, "revision", result.context.revision)) { error = "Invalid benchmark context"; return false; }
    }
    if (google) {
        const auto* rows = field(*root, "benchmarks", JsonType::Array);
        if (!rows || rows->children.size() > 10000) { error = "Invalid Google Benchmark results"; return false; }
        if (result.context.machine.empty()) {
            if (const auto* host = field(result.importedContext, "host_name", JsonType::String)) result.context.machine = host->stringValue;
        }
        // Repetition rows take precedence over a supplied mean aggregate. Keep
        // aggregate-only reports usable without mixing either with single runs.
        struct Group { BenchmarkMeasurement measurement; RunningSamples values; bool aggregate = false; };
        std::map<std::string, Group> groups;
        for (const auto& row : rows->children) {
            if (row.type != JsonType::Object) { error = "Invalid Google Benchmark row"; return false; }
            if (const auto* failed = row.find("error_occurred")) {
                if (failed->type != JsonType::Bool) { error = "Invalid benchmark error flag"; return false; }
                if (failed->boolValue) continue;
            }
            const auto* type = field(row, "run_type", JsonType::String);
            if (row.find("run_type") && !type) { error = "Invalid benchmark run type"; return false; }
            const bool aggregate = type && type->stringValue == "aggregate";
            if (type && type->stringValue != "iteration" && !aggregate) continue;
            if (aggregate) {
                const auto* kind = field(row, "aggregate_name", JsonType::String);
                if (!kind || kind->stringValue != "mean") continue;
                if (const auto* unit = field(row, "aggregate_unit", JsonType::String); unit && unit->stringValue != "time") continue;
            }
            const auto* name = field(row, "run_name", JsonType::String);
            if (!name) name = field(row, "name", JsonType::String);
            const auto* unit = field(row, "time_unit", JsonType::String);
            if (!name || name->stringValue.empty() || name->stringValue.size() > 4000 || !unit || unit->stringValue.empty() || unit->stringValue.size() > 256) {
                error = "Invalid Google Benchmark name or time unit"; return false;
            }
            for (const char* metric : {"real_time", "cpu_time"}) {
                const auto* value = row.find(metric); if (!value) continue;
                if (value->type != JsonType::Number || !std::isfinite(value->numberValue) || value->numberValue < 0) { error = "Invalid Google Benchmark time"; return false; }
                const auto key = name->stringValue + "/" + metric;
                auto [it, inserted] = groups.try_emplace(key);
                auto& group = it->second;
                if (inserted) { group.measurement = {key, unit->stringValue, true, 0, {}, 1}; group.aggregate = aggregate; }
                else if (group.measurement.unit != unit->stringValue) { error = "Inconsistent Google Benchmark units"; return false; }
                if (!inserted && group.aggregate && !aggregate) { group.values = {}; group.aggregate = false; }
                if (!inserted && !group.aggregate && aggregate) continue;
                if (aggregate && group.values.count) { error = "Duplicate Google Benchmark mean"; return false; }
                group.values.add(value->numberValue);
            }
        }
        for (auto& [key, group] : groups) {
            group.values.apply(group.measurement);
            if(group.aggregate) {group.measurement.samples=0;group.measurement.statistic="mean";group.measurement.hasSampleSpread=false;}
            result.measurements.push_back(std::move(group.measurement));
        }
        run = std::move(result); return true;
    }
    const auto* measurements = field(*root, "measurements", JsonType::Array);
    if (!measurements || measurements->children.size() > 10000) { error = "Invalid benchmark measurements"; return false; }
    std::set<std::string> names; std::size_t totalSamples=0;
    for (const auto& item : measurements->children) {
        if (item.type != JsonType::Object) { error = "Invalid benchmark measurement"; return false; }
        const auto* name = field(item, "name", JsonType::String); const auto* unit = field(item, "unit", JsonType::String);
        const auto* direction = field(item, "direction", JsonType::String); const auto* value = field(item, "value", JsonType::Number); const auto* samples=item.find("samples");
        std::size_t valueFields=0, sampleFields=0; for(const auto& f:item.children) {valueFields+=f.key=="value";sampleFields+=f.key=="samples";}
        if (!name || name->stringValue.empty() || name->stringValue.size() > 4096 || !unit || unit->stringValue.empty() || unit->stringValue.size() > 256 ||
            !direction || (direction->stringValue != "lower" && direction->stringValue != "higher") || valueFields>1 || sampleFields>1 || (valueFields && sampleFields) || (!valueFields && !sampleFields) || !names.insert(name->stringValue).second) { error = "Invalid or duplicate benchmark measurement"; return false; }
        BenchmarkMeasurement measurement{name->stringValue,unit->stringValue,direction->stringValue=="lower",0,{},1};
        if(valueFields) { if(!value || !std::isfinite(value->numberValue) || value->numberValue<0) {error="Invalid benchmark value";return false;} measurement.value=value->numberValue; }
        else {
            if(!samples || samples->type!=JsonType::Array || samples->children.empty() || samples->children.size()>10000-totalSamples) {error="Invalid or excessive benchmark samples";return false;}
            totalSamples+=samples->children.size(); RunningSamples values;
            for(const auto& sample:samples->children) {if(sample.type!=JsonType::Number || !std::isfinite(sample.numberValue) || sample.numberValue<0) {error="Invalid benchmark sample";return false;} values.add(sample.numberValue);}
            values.apply(measurement);
        }
        result.measurements.push_back(std::move(measurement));
    }
    run = std::move(result); return true;
}
BenchmarkComparison compareBenchmarkRuns(const BenchmarkRun& baseline, const BenchmarkRun& candidate, double thresholdPercent) {
    BenchmarkComparison result;
    if (!std::isfinite(thresholdPercent) || thresholdPercent < 0) thresholdPercent = 5;
    std::map<std::string, const BenchmarkMeasurement*> old, current;
    std::set<std::string> duplicate;
    for (const auto& m : baseline.measurements) if (!old.emplace(m.name, &m).second) duplicate.insert(m.name);
    for (const auto& m : candidate.measurements) if (!current.emplace(m.name, &m).second) duplicate.insert(m.name);
    std::set<std::string> names; for (const auto& [name, _] : old) names.insert(name); for (const auto& [name, _] : current) names.insert(name);
    const auto& a = baseline.context; const auto& b = candidate.context;
    const bool contextKnown = !a.machine.empty() && !a.build.empty() && !a.environment.empty() && !b.machine.empty() && !b.build.empty() && !b.environment.empty();
    const bool contextMatches = a.machine == b.machine && a.build == b.build && a.environment == b.environment;
    for (const auto& name : names) {
        BenchmarkComparisonEntry entry; entry.name = name;
        auto x = old.find(name), y = current.find(name);
        if (x != old.end()) { entry.unit = x->second->unit; entry.baseline = x->second->value; entry.baselineUnit = x->second->unit; entry.baselineStatistic = x->second->statistic; entry.baselineSamples = x->second->samples; entry.baselineLowerIsBetter = x->second->lowerIsBetter; }
        if (y != current.end()) { if (x == old.end()) entry.unit = y->second->unit; entry.candidate = y->second->value; entry.candidateUnit = y->second->unit; entry.candidateStatistic = y->second->statistic; entry.candidateSamples = y->second->samples; entry.candidateLowerIsBetter = y->second->lowerIsBetter; }
        if(x!=old.end() && x->second->hasSampleSpread) {entry.baselineHasSpread=true;entry.baselineMin=x->second->sampleMin;entry.baselineMax=x->second->sampleMax;entry.baselineStddev=x->second->sampleStddev;}
        if(y!=current.end() && y->second->hasSampleSpread) {entry.candidateHasSpread=true;entry.candidateMin=y->second->sampleMin;entry.candidateMax=y->second->sampleMax;entry.candidateStddev=y->second->sampleStddev;}
        if (duplicate.count(name)) entry.reason = "Duplicate measurement name";
        else if (x == old.end()) entry.assessment = BenchmarkAssessment::CandidateOnly;
        else if (y == current.end()) entry.assessment = BenchmarkAssessment::BaselineOnly;
        else if ((x->second->unit != y->second->unit && (!timeScale(x->second->unit) || !timeScale(y->second->unit))) || x->second->lowerIsBetter != y->second->lowerIsBetter) entry.reason = "Unit or direction differs";
        else if (x->second->statistic != y->second->statistic) entry.reason = "Measurement statistics differ";
        else if (!std::isfinite(entry.baseline) || !std::isfinite(entry.candidate) || entry.baseline < 0 || entry.candidate < 0) entry.reason = "Invalid measurement value";
        else if (!contextKnown) entry.reason = "Machine, build, or environment context is missing";
        else if (!contextMatches) entry.reason = "Machine, build, or environment context differs";
        else if (entry.baseline == 0) entry.reason = "Percentage change is undefined for a zero baseline";
        else {
            const auto oldScale = x->second->unit == y->second->unit ? 1.0L : timeScale(x->second->unit);
            const auto newScale = x->second->unit == y->second->unit ? 1.0L : timeScale(y->second->unit);
            const long double percent = ((static_cast<long double>(entry.candidate) / static_cast<long double>(entry.baseline)) * (newScale / oldScale) - 1.0L) * 100.0L;
            if (!std::isfinite(percent) || std::abs(percent) > static_cast<long double>(std::numeric_limits<double>::max())) entry.reason = "Percentage change is out of range";
            else {
                entry.percentChange = static_cast<double>(percent);
                entry.hasPercentChange = true;
                const auto worse = x->second->lowerIsBetter ? entry.percentChange : -entry.percentChange;
                entry.assessment = worse > thresholdPercent ? BenchmarkAssessment::PossibleRegression :
                    worse < -thresholdPercent ? BenchmarkAssessment::Improvement : BenchmarkAssessment::WithinThreshold;
            }
        }
        result.entries.push_back(std::move(entry));
    }
    return result;
}
}
