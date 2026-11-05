#include "modules/TestRunnerPlus.hpp"
#include "text/Utf8.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <charconv>
#include <optional>

namespace lucid::modules {
namespace {
constexpr std::size_t maxBytes = 8U * 1024U * 1024U, maxTests = 10000;
using editor::JsonType;
const editor::JsonValue* field(const editor::JsonValue& obj, const char* key, JsonType type) {
    const auto* v = obj.find(key); return v && v->type == type ? v : nullptr;
}
bool xmlCharacter(unsigned c) { return c==9 || c==10 || c==13 || (c>=0x20 && c<=0xd7ff) || (c>=0xe000 && c<=0xfffd) || (c>=0x10000 && c<=0x10ffff); }
bool validXmlUtf8(std::string_view input) {
    for(std::size_t i=0;i<input.size();) {const auto start=i;const auto c=text::nextUtf8(input,i);if((c==U'\ufffd' && i==start+1 && static_cast<unsigned char>(input[start])>=0x80U) || !xmlCharacter(c)) return false;}
    return true;
}
void appendUtf8(std::string& s, unsigned c) {
    if (c < 128) s.push_back(static_cast<char>(c));
    else if (c < 2048) { s.push_back(static_cast<char>(0xc0 | (c >> 6))); s.push_back(static_cast<char>(0x80 | (c & 63))); }
    else if (c < 65536) { s.push_back(static_cast<char>(0xe0 | (c >> 12))); s.push_back(static_cast<char>(0x80 | ((c >> 6) & 63))); s.push_back(static_cast<char>(0x80 | (c & 63))); }
    else { s.push_back(static_cast<char>(0xf0 | (c >> 18))); s.push_back(static_cast<char>(0x80 | ((c >> 12) & 63))); s.push_back(static_cast<char>(0x80 | ((c >> 6) & 63))); s.push_back(static_cast<char>(0x80 | (c & 63))); }
}
bool decode(std::string_view in, std::string& out) {
    for (std::size_t i = 0; i < in.size(); ++i) {
        if (in[i] != '&') { out.push_back(in[i]); continue; }
        auto end = in.find(';', i + 1); if (end == in.npos || end - i > 12) return false;
        auto e = in.substr(i + 1, end - i - 1); unsigned c = 0;
        if (e == "amp") c = '&'; else if (e == "lt") c = '<'; else if (e == "gt") c = '>'; else if (e == "quot") c = '"'; else if (e == "apos") c = '\'';
        else if (e.size() > 1 && e[0] == '#') {
            std::size_t j = 1; unsigned base = 10; if (j < e.size() && (e[j] == 'x' || e[j] == 'X')) { base = 16; ++j; }
            if (j == e.size()) return false;
            for (; j < e.size(); ++j) { unsigned digit; char ch = e[j]; if (ch >= '0' && ch <= '9') digit = ch - '0'; else if (ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10; else if (ch >= 'A' && ch <= 'F') digit = ch - 'A' + 10; else return false; if (digit >= base || c > (0x10ffff - digit) / base) return false; c = c * base + digit; }
            if (!xmlCharacter(c)) return false;
        } else return false;
        appendUtf8(out, c); i = end;
    }
    return true;
}
bool nameChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == ':' || c == '.'; }
struct Tag { std::string name; std::map<std::string,std::string> attrs; bool closing = false, self = false; };
bool parseTag(std::string_view t, Tag& tag) {
    std::size_t i = 0; auto ws = [&] { while (i < t.size() && std::isspace(static_cast<unsigned char>(t[i]))) ++i; };
    ws(); if (i < t.size() && t[i] == '/') { tag.closing = true; ++i; }
    auto start = i; while (i < t.size() && nameChar(t[i])) ++i; if (i == start) return false; tag.name = std::string(t.substr(start, i - start));
    while (true) {
        ws(); if (i == t.size()) return true;
        if (t[i] == '/' && i + 1 == t.size() && !tag.closing) { tag.self = true; return true; }
        if (tag.closing) return false;
        start = i; while (i < t.size() && nameChar(t[i])) ++i; if (i == start) return false; std::string key(t.substr(start, i - start));
        ws(); if (i == t.size() || t[i++] != '=') return false; ws(); if (i == t.size() || (t[i] != '"' && t[i] != '\'')) return false;
        char quote = t[i++]; start = i; while (i < t.size() && t[i] != quote) ++i; if (i == t.size()) return false;
        std::string value; if (t.substr(start,i-start).find('<')!=std::string_view::npos || !decode(t.substr(start, i - start), value) || !tag.attrs.emplace(std::move(key), std::move(value)).second) return false; ++i;
    }
}
}
bool parseCTestDiscovery(std::string_view input, TestDiscovery& result, std::string& error) {
    result = {}; error.clear(); if (input.size() > maxBytes) { error = "CTest discovery exceeds size limit"; return false; }
    editor::JsonError e; auto root = editor::parseJson(input, e);
    if (!root || root->type != JsonType::Object) { error = "Invalid CTest discovery JSON"; return false; }
    const auto* kind = field(*root,"kind",JsonType::String); const auto* version = field(*root,"version",JsonType::Object);
    const auto* major = version ? field(*version,"major",JsonType::Number) : nullptr; const auto* tests = field(*root,"tests",JsonType::Array);
    if (!kind || kind->stringValue != "ctestInfo" || !major || major->numberValue != 1 || !tests || tests->children.size() > maxTests) { error = "Unsupported CTest discovery format"; return false; }
    TestDiscovery output; std::set<std::string> names;
    for (const auto& t : tests->children) {
        const auto* name = field(t,"name",JsonType::String); const auto* cmd = t.find("command"); const auto* props = t.find("properties");
        if (t.type != JsonType::Object || !name || name->stringValue.empty() || !names.insert(name->stringValue).second || (cmd && cmd->type != JsonType::Array) || (props && props->type != JsonType::Array)) { error = "Invalid CTest test entry"; return false; }
        DiscoveredTest d; d.name = name->stringValue;
        if (cmd) for (const auto& arg : cmd->children) { if (arg.type != JsonType::String) { error = "Invalid CTest command"; return false; } d.command.push_back(arg.stringValue); }
        if (props) d.properties = *props;
        output.tests.push_back(std::move(d));
    }
    result = std::move(output); return true;
}
static bool parseJUnitImpl(std::string_view input, TestResults& result, std::string& error, bool strict) {
    result = {}; error.clear(); if (input.size() > maxBytes) { error = "JUnit report exceeds size limit"; return false; }
    if(!validXmlUtf8(input)) {error="Invalid JUnit XML characters or UTF-8";return false;}
    struct SuiteCounts { std::optional<std::size_t> tests,failures,errors,skipped; std::size_t seen=0,failed=0,errored=0,skip=0; std::set<std::size_t> identities; };
    std::vector<SuiteCounts> suites; std::map<std::pair<std::string,std::string>,std::size_t> identities; std::string failureKind;
    const auto countAttr=[](const Tag& tag,const char* name,std::optional<std::size_t>& output) {const auto it=tag.attrs.find(name);if(it==tag.attrs.end())return true;std::size_t value=0;const auto parsed=std::from_chars(it->second.data(),it->second.data()+it->second.size(),value);if(parsed.ec!=std::errc{} || parsed.ptr!=it->second.data()+it->second.size() || value>maxTests)return false;output=value;return true;};
    const auto countsAgree=[](const SuiteCounts& suite){return (!suite.tests || *suite.tests==suite.seen || *suite.tests==suite.identities.size()) && (!suite.failures || *suite.failures==suite.failed) && (!suite.errors || *suite.errors==suite.errored) && (!suite.skipped || *suite.skipped==suite.skip);};
    TestResults output; std::vector<std::string> stack; bool rootSeen = false, rootClosed = false; std::size_t pos = 0; TestResult* test = nullptr;
    auto fail = [&]() { error = "Invalid or unsupported JUnit XML"; return false; };
    auto addText = [&](std::string_view raw, bool encoded) {
        std::string v; if (encoded) { if (!decode(raw, v)) return false; } else v = std::string(raw);
        if (test && !stack.empty()) { auto& tag = stack.back(); if (tag == "failure" || tag == "error") test->failureText += v; else if (tag == "system-out") test->output += v; else if (tag == "system-err") test->errorOutput += v; }
        else if (!stack.empty() && (stack.back()=="system-out" || stack.back()=="system-err")) { /* Suite-level output is not attributed to a test. */ }
        else for (char c : v) if (!std::isspace(static_cast<unsigned char>(c))) return false;
        return true;
    };
    while (pos < input.size()) {
        auto lt = input.find('<', pos); if (lt == input.npos) lt = input.size(); if (!addText(input.substr(pos,lt-pos), true)) return fail(); if (lt == input.size()) break;
        if (input.substr(lt,4) == "<!--") { auto end = input.find("-->",lt+4); if (end == input.npos) return fail(); pos = end+3; continue; }
        if (input.substr(lt,9) == "<![CDATA[") { auto end = input.find("]]>",lt+9); if (end == input.npos || !addText(input.substr(lt+9,end-lt-9), false)) return fail(); pos = end+3; continue; }
        if (input.substr(lt,2) == "<?") { auto end = input.find("?>",lt+2); if (end == input.npos || rootSeen) return fail(); pos = end+2; continue; }
        if (input.substr(lt,2) == "<!") return fail();
        std::size_t end = lt+1; char quote = 0;
        for (; end < input.size(); ++end) { char c = input[end]; if (quote) { if (c == quote) quote = 0; } else if (c == '\'' || c == '"') quote = c; else if (c == '>') break; }
        if (end == input.size()) return fail();
        Tag tag; if (!parseTag(input.substr(lt+1,end-lt-1),tag)) return fail(); pos = end+1;
        if(strict && !tag.closing) {
            const auto parent=stack.empty()?std::string_view{}:std::string_view(stack.back());
            if(tag.name=="testsuites") {if(!stack.empty()) return fail();}
            else if(tag.name=="testsuite") {if(!stack.empty() && parent!="testsuites" && parent!="testsuite") return fail();if(tag.attrs.count("status") || tag.attrs.count("result")) return fail(); SuiteCounts counts;if(!countAttr(tag,"tests",counts.tests) || !countAttr(tag,"failures",counts.failures) || !countAttr(tag,"errors",counts.errors) || !countAttr(tag,"skipped",counts.skipped)) return fail();suites.push_back(counts);}
            else if(tag.name=="testcase") {
                if(parent!="testsuite") return fail();
                for(const auto& [key,_]:tag.attrs) if(key!="name" && key!="classname" && key!="time" && key!="file" && key!="line" && key!="url" && key!="assertions") return fail();
            }
            else if(tag.name=="failure" || tag.name=="error" || tag.name=="skipped") {if(parent!="testcase") return fail();}
            else if(tag.name=="system-out" || tag.name=="system-err") {if(parent!="testcase" && parent!="testsuite") return fail();}
            else if(tag.name=="properties") {if(parent!="testcase" && parent!="testsuite") return fail();}
            else if(tag.name=="property") {if(parent!="properties") return fail();}
            else return fail();
        }
        if (tag.closing) { if (stack.empty() || stack.back() != tag.name) return fail(); if (tag.name == "testcase") {test = nullptr;failureKind.clear();} if(strict && tag.name=="testsuite") {if(suites.empty() || !countsAgree(suites.back()))return fail();suites.pop_back();} stack.pop_back(); if (stack.empty()) rootClosed = true; continue; }
        if (stack.empty()) { if (rootSeen || (tag.name != "testsuite" && tag.name != "testsuites")) return fail(); rootSeen = true; }
        else if (rootClosed) return fail();
        if (tag.name == "testcase") {
            if (test || stack.empty() || stack.back() != "testsuite" || output.tests.size() >= maxTests) return fail();
            auto name = tag.attrs.find("name"); if (name == tag.attrs.end() || name->second.empty()) return fail();
            output.tests.push_back({}); test = &output.tests.back(); test->name = name->second; failureKind.clear();
            if (auto it=tag.attrs.find("classname"); it!=tag.attrs.end()) test->className=it->second;
            if (auto it=tag.attrs.find("time"); it!=tag.attrs.end()) test->duration=it->second;
            // pytest can emit separate call and teardown rows for one identity
            // while its suite's tests count remains one. Preserve both rows as
            // ambiguous, and accept either emitted-row or distinct-name totals.
            if(strict) {auto [identity,_]=identities.try_emplace({test->className,test->name},identities.size());for(auto& suite:suites){++suite.seen;suite.identities.insert(identity->second);}}
        } else if (tag.name == "failure" || tag.name == "error" || tag.name == "skipped") {
            if (!test || stack.empty() || stack.back() != "testcase") return fail();
            if(test->status != TestResultStatus::Passed && (!strict || tag.name=="skipped" || failureKind!=tag.name)) return fail();
            const bool first=test->status==TestResultStatus::Passed;
            if(!first && !test->failureText.empty()) test->failureText+='\n';
            test->status = tag.name == "skipped" ? TestResultStatus::Skipped : TestResultStatus::Failed;
            if(strict && first) {failureKind=tag.name; for(auto& suite:suites) {if(tag.name=="skipped")++suite.skip;else if(tag.name=="failure")++suite.failed;else ++suite.errored;}}
            if (auto it=tag.attrs.find("message"); it!=tag.attrs.end()) {if(!test->failureMessage.empty())test->failureMessage+='\n';test->failureMessage+=it->second;}
        }
        if (!tag.self) { if (stack.size() >= 64) return fail(); stack.push_back(tag.name); }
        else { if (tag.name == "testcase") {test = nullptr;failureKind.clear();} if(strict && tag.name=="testsuite") {if(suites.empty() || !countsAgree(suites.back()))return fail();suites.pop_back();} if (stack.empty()) rootClosed = true; }
    }
    if (!rootSeen || !rootClosed || !stack.empty()) return fail();
    result = std::move(output); return true;
}
bool parseCTestJUnit(std::string_view input, TestResults& result, std::string& error) {return parseJUnitImpl(input,result,error,false);}
bool parseStrictJUnit(std::string_view input, TestResults& result, std::string& error) {return parseJUnitImpl(input,result,error,true);}
std::vector<TestCorrelation> correlateCTestResults(const TestDiscovery& discovery, const TestResults& results) {
    std::map<std::string, TestCorrelation> entries;
    for (std::size_t i = 0; i < discovery.tests.size(); ++i) { auto& e = entries[discovery.tests[i].name]; e.name = discovery.tests[i].name; e.discoveryIndices.push_back(i); }
    for (std::size_t i = 0; i < results.tests.size(); ++i) { auto& e = entries[results.tests[i].name]; e.name = results.tests[i].name; e.resultIndices.push_back(i); }
    std::vector<TestCorrelation> output; output.reserve(entries.size());
    for (auto& [name, e] : entries) {
        if (name.empty() || e.discoveryIndices.size() > 1 || e.resultIndices.size() > 1) e.status = TestMatchStatus::Ambiguous;
        else if (e.discoveryIndices.empty()) e.status = TestMatchStatus::ResultOnly;
        else if (e.resultIndices.empty()) e.status = TestMatchStatus::DiscoveryOnly;
        else e.status = TestMatchStatus::Matched;
        output.push_back(std::move(e));
    }
    return output;
}
std::vector<TestHistoryEntry> compareCTestRuns(const TestResults& previous, const TestResults& current) {
    std::map<std::string, TestHistoryEntry> entries;
    for (std::size_t i = 0; i < previous.tests.size(); ++i) { auto& e = entries[previous.tests[i].name]; e.name = previous.tests[i].name; e.previousIndices.push_back(i); }
    for (std::size_t i = 0; i < current.tests.size(); ++i) { auto& e = entries[current.tests[i].name]; e.name = current.tests[i].name; e.currentIndices.push_back(i); }
    std::vector<TestHistoryEntry> output; output.reserve(entries.size());
    for (auto& [name, e] : entries) {
        if (name.empty() || e.previousIndices.size() > 1 || e.currentIndices.size() > 1) e.status = TestHistoryStatus::Ambiguous;
        else if (e.previousIndices.empty()) e.status = TestHistoryStatus::CurrentOnly;
        else if (e.currentIndices.empty()) e.status = TestHistoryStatus::PreviousOnly;
        else e.status = previous.tests[e.previousIndices[0]].status == current.tests[e.currentIndices[0]].status ? TestHistoryStatus::Unchanged : TestHistoryStatus::Changed;
        output.push_back(std::move(e));
    }
    return output;
}

namespace {
std::optional<double> durationSeconds(std::string_view value) {
    if (value.empty() || value.size() > 32) return std::nullopt;
    double total = 0, factor = 1; bool dot = false, digit = false;
    for (const char c : value) {
        if (c == '.' && !dot) { dot = true; continue; }
        if (c < '0' || c > '9') return std::nullopt;
        digit = true;
        if (!dot) total = total * 10 + (c - '0');
        else { factor *= 0.1; total += (c - '0') * factor; }
        if (!std::isfinite(total)) return std::nullopt;
    }
    return digit ? std::optional<double>(total) : std::nullopt;
}
}

std::vector<TestHistorySummary> summarizeCTestHistory(const std::vector<TestResults>& runs) {
    std::map<std::string, TestHistorySummary> summaries;
    for (std::size_t runIndex = 0; runIndex < runs.size(); ++runIndex) {
        std::map<std::string, std::vector<const TestResult*>> observed;
        for (const auto& test : runs[runIndex].tests) if (!test.name.empty()) observed[test.name].push_back(&test);
        for (const auto& [name, matches] : observed) {
            auto& summary = summaries[name]; summary.name = name;
            if (matches.size() != 1) { ++summary.ambiguousRuns; summary.consecutiveFailures = 0; summary.latestStatus.reset(); summary.latestRunIndex = runIndex; summary.latestDurationSeconds.reset(); summary.previousDurationSeconds.reset(); summary.durationDeltaSeconds.reset(); continue; }
            const auto& test = *matches.front(); ++summary.observations;
            if (summary.latestStatus && *summary.latestStatus != TestResultStatus::Skipped && test.status != TestResultStatus::Skipped && *summary.latestStatus != test.status) ++summary.passFailTransitions;
            switch (test.status) {
                case TestResultStatus::Passed: ++summary.passed; summary.consecutiveFailures = 0; break;
                case TestResultStatus::Failed: ++summary.failed; ++summary.consecutiveFailures; summary.longestFailureStreak = std::max(summary.longestFailureStreak, summary.consecutiveFailures); summary.mostRecentFailureRunIndex = runIndex; break;
                case TestResultStatus::Skipped: ++summary.skipped; summary.consecutiveFailures = 0; break;
            }
            summary.latestStatus = test.status; summary.latestRunIndex = runIndex;
            summary.previousDurationSeconds = summary.latestDurationSeconds;
            summary.latestDurationSeconds = durationSeconds(test.duration);
            summary.durationDeltaSeconds.reset();
            if (summary.latestDurationSeconds && summary.previousDurationSeconds)
                summary.durationDeltaSeconds = *summary.latestDurationSeconds - *summary.previousDurationSeconds;
        }
    }
    std::vector<TestHistorySummary> output; output.reserve(summaries.size());
    for (auto& [name, summary] : summaries) output.push_back(std::move(summary));
    return output;
}

bool canRunCTestNameExactly(std::string_view name) noexcept {
    if (name.empty() || name.size() > 1024) return false;
    for (unsigned char c : name) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '/' || c == ':' || c == ' ' || c == '.')) return false;
    return true;
}

bool planFailedCTestReruns(const TestDiscovery& discovery, const std::vector<TestResults>& runs,
                           std::vector<FailedRerunCandidate>& candidates, std::string& error) {
    candidates.clear(); error.clear();
    if (runs.size() > 10 || discovery.tests.size() > maxTests) { error = "Test rerun plan exceeds limits"; return false; }
    std::size_t nameBytes = 0;
    const auto countName = [&](std::string_view name) {
        if (name.size() > maxBytes - nameBytes) return false;
        nameBytes += name.size(); return true;
    };
    std::map<std::string, std::size_t> discovered;
    for (const auto& test : discovery.tests) {
        if (!countName(test.name)) { error = "Test rerun plan exceeds limits"; return false; }
        ++discovered[test.name];
    }
    struct Observation { std::size_t run = 0, count = 0; TestResultStatus status = TestResultStatus::Passed; };
    std::map<std::string, Observation> latest;
    for (std::size_t run = 0; run < runs.size(); ++run) {
        if (runs[run].tests.size() > maxTests) { error = "Test rerun plan exceeds limits"; return false; }
        std::map<std::string, Observation> observed;
        for (const auto& test : runs[run].tests) {
            if (!countName(test.name)) { error = "Test rerun plan exceeds limits"; return false; }
            auto& entry = observed[test.name]; entry.run = run; ++entry.count; entry.status = test.status;
        }
        for (auto& [name, value] : observed) latest[std::move(name)] = value;
    }
    std::vector<FailedRerunCandidate> result;
    for (const auto& [name, observation] : latest) {
        if (observation.count == 1 && observation.status != TestResultStatus::Failed) continue;
        const auto found = discovered.find(name); FailedRerunStatus status;
        if (observation.count != 1) status = FailedRerunStatus::AmbiguousResult;
        else if (!canRunCTestNameExactly(name)) status = FailedRerunStatus::UnsupportedName;
        else if (found == discovered.end()) status = FailedRerunStatus::NotDiscovered;
        else if (found->second != 1) status = FailedRerunStatus::AmbiguousDiscovery;
        else status = FailedRerunStatus::Ready;
        result.push_back({name, status, observation.run});
    }
    candidates = std::move(result); return true;
}

}
