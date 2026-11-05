#include "modules/TestRunnerPlus.hpp"
#include <string>
using namespace lucid::modules;
int main() {
    TestDiscovery discovery; TestResults results; std::string error;
    const auto json = R"({"kind":"ctestInfo","version":{"major":1,"minor":0},"tests":[{"name":"alpha","command":["/bin/test","--a"],"properties":[{"name":"LABELS","value":["unit"]}]},{"name":"missing","properties":[]}]})";
    if (!parseCTestDiscovery(json, discovery, error) || discovery.tests.size() != 2 || discovery.tests[0].command.size() != 2 || discovery.tests[0].properties.children.empty()) return 1;
    if (parseCTestDiscovery(R"({"kind":"ctestInfo","version":{"major":2},"tests":[]})", discovery, error) || !discovery.tests.empty()) return 2;
    const auto xml = R"(<?xml version="1.0" encoding="UTF-8"?><testsuite name="suite"><testcase name="ok" classname="suite" time="0.01"/><testcase name="bad"><failure message="bad &amp; worse">line &lt;1&gt;</failure><system-out><![CDATA[actual output <x>]]></system-out><system-err>oops&#10;more</system-err></testcase><testcase name="skip"><skipped message="disabled"/></testcase></testsuite>)";
    if (!parseCTestJUnit(xml, results, error) || results.tests.size() != 3 || results.tests[0].status != TestResultStatus::Passed || results.tests[1].status != TestResultStatus::Failed || results.tests[1].failureMessage != "bad & worse" || results.tests[1].failureText != "line <1>" || results.tests[1].output != "actual output <x>" || results.tests[1].errorOutput != "oops\nmore" || results.tests[2].status != TestResultStatus::Skipped) return 3;
    if (parseCTestJUnit("<!DOCTYPE x [<!ENTITY x SYSTEM 'file:///etc/passwd'>]><testsuite/>", results, error) || !results.tests.empty()) return 4;
    if (parseCTestJUnit("<testsuite><testcase name='a'></testsuite>", results, error)) return 5;
    if (parseCTestJUnit("<testsuite><testcase name='&unknown;'/></testsuite>", results, error)) return 6;
    if (parseCTestJUnit(std::string(8U*1024U*1024U+1,'x'), results, error)) return 7;
    if (!parseCTestDiscovery(json, discovery, error) || !parseCTestJUnit(xml, results, error)) return 8;
    auto matches = correlateCTestResults(discovery, results);
    if (matches.size() != 5 || matches[0].name != "alpha" || matches[0].status != TestMatchStatus::DiscoveryOnly || matches[1].status != TestMatchStatus::ResultOnly) return 9;
    results.tests[0].name = "alpha"; matches = correlateCTestResults(discovery, results);
    if (matches[0].status != TestMatchStatus::Matched || matches[0].discoveryIndices[0] != 0 || matches[0].resultIndices[0] != 0) return 10;
    results.tests.push_back(results.tests[0]); matches = correlateCTestResults(discovery, results);
    if (matches[0].status != TestMatchStatus::Ambiguous || matches[0].resultIndices.size() != 2) return 11;
    TestResults previous = results, current = results;
    current.tests.pop_back(); current.tests[0].status = TestResultStatus::Failed;
    auto history = compareCTestRuns(previous, current);
    if (history[0].status != TestHistoryStatus::Ambiguous) return 12;
    previous.tests.pop_back(); previous.tests.erase(previous.tests.begin() + 2); current.tests.push_back({}); current.tests.back().name = "new";
    history = compareCTestRuns(previous, current);
    if (history.size() != 4 || history[0].name != "alpha" || history[0].status != TestHistoryStatus::Changed || history[1].status != TestHistoryStatus::Unchanged || history[2].status != TestHistoryStatus::CurrentOnly || history[3].status != TestHistoryStatus::CurrentOnly) return 13;
    history = compareCTestRuns(current, previous);
    if (history[2].status != TestHistoryStatus::PreviousOnly) return 14;
    TestResult fail; fail.name = "flaky"; fail.status = TestResultStatus::Failed; fail.duration = "0.25";
    TestResult pass = fail; pass.status = TestResultStatus::Passed; pass.duration = "0.50";
    TestResult other; other.name = "other"; other.duration = "1";
    auto summaries = summarizeCTestHistory({{{pass}}, {{other}}, {{fail}}, {{fail}}});
    if (summaries.size() != 2 || summaries[0].name != "flaky" || summaries[0].observations != 3 ||
        summaries[0].passed != 1 || summaries[0].failed != 2 || summaries[0].consecutiveFailures != 2 ||
        !summaries[0].latestRunIndex || *summaries[0].latestRunIndex != 3 || !summaries[0].durationDeltaSeconds || *summaries[0].durationDeltaSeconds != 0.0) return 15;
    summaries = summarizeCTestHistory({{{fail}}, {{fail, fail}}, {{fail}}});
    if (summaries.size() != 1 || summaries[0].ambiguousRuns != 1 || summaries[0].consecutiveFailures != 1 || summaries[0].durationDeltaSeconds) return 16;
    fail.duration = "not a duration"; summaries = summarizeCTestHistory({{{pass}}, {{fail}}});
    if (summaries[0].latestDurationSeconds || summaries[0].durationDeltaSeconds || !summaries[0].previousDurationSeconds) return 17;
    TestResult skipped = pass; skipped.status = TestResultStatus::Skipped;
    summaries = summarizeCTestHistory({{{pass}}, {{other}}, {{fail}}, {{fail}}, {{pass}}, {{skipped}}, {{fail}}, {{fail, fail}}, {{pass}}});
    if (summaries.size() != 2 || summaries[0].name != "flaky" || summaries[0].passFailTransitions != 2 || summaries[0].longestFailureStreak != 2 || !summaries[0].mostRecentFailureRunIndex || *summaries[0].mostRecentFailureRunIndex != 6 || summaries[0].ambiguousRuns != 1) return 18;
    summaries = summarizeCTestHistory({{{pass}}, {{skipped}}, {{fail}}});
    if (summaries[0].passFailTransitions != 0 || summaries[0].longestFailureStreak != 1) return 19;
    summaries = summarizeCTestHistory({{{pass}}, {{fail, fail}}, {{fail}}});
    if (summaries[0].passFailTransitions != 0 || summaries[0].longestFailureStreak != 1 || summaries[0].ambiguousRuns != 1) return 20;
    TestDiscovery discovered; discovered.tests.resize(4);
    discovered.tests[0].name = "flaky"; discovered.tests[1].name = "duplicate"; discovered.tests[2].name = "duplicate"; discovered.tests[3].name = "unsafe[1]";
    TestResult duplicate = fail; duplicate.name = "duplicate"; TestResult missing = fail; missing.name = "missing";
    TestResult unsafe = fail; unsafe.name = "unsafe[1]"; TestResult ambiguous = fail; ambiguous.name = "ambiguous";
    std::vector<FailedRerunCandidate> plan;
    if (!planFailedCTestReruns(discovered, {{{pass, duplicate, missing, unsafe, ambiguous}}, {{other}}, {{fail, ambiguous, ambiguous}}}, plan, error) || plan.size() != 5) return 21;
    if (plan[0].name != "ambiguous" || plan[0].status != FailedRerunStatus::AmbiguousResult || plan[0].lastObservedRun != 2 ||
        plan[1].status != FailedRerunStatus::AmbiguousDiscovery || plan[2].name != "flaky" || plan[2].status != FailedRerunStatus::Ready || plan[2].lastObservedRun != 2 ||
        plan[3].status != FailedRerunStatus::NotDiscovered || plan[4].status != FailedRerunStatus::Ready) return 22;
    if (!planFailedCTestReruns(discovered, {{{fail}}, {{other}}, {{pass}}}, plan, error) || !plan.empty()) return 23;
    if (planFailedCTestReruns(discovered, std::vector<TestResults>(11), plan, error) || error.empty() || !plan.empty()) return 24;
    if (!canRunCTestNameExactly("suite.test 1") || !canRunCTestNameExactly("suite[1]") || !canRunCTestNameExactly("case(a+b)?") || canRunCTestNameExactly("suite\\1") || canRunCTestNameExactly(std::string(1025, 'a')) || canRunCTestNameExactly("")) return 25;
    return 0;
}
