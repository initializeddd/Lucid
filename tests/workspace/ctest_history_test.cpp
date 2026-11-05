#include "modules/CTestHistory.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
int main() {
    namespace fs = std::filesystem; using namespace lucid::modules;
    auto base = fs::canonical(fs::temp_directory_path()) / ("lucid-ctest-history-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto settings = base / "settings", project = base / "project", build = base / "build", other = base / "other";
    fs::create_directories(settings); fs::create_directories(project); fs::create_directories(build); fs::create_directories(other);
    fs::permissions(settings, fs::perms::owner_all, fs::perm_options::replace);
    CTestHistorySnapshot h, stale, distinct; std::string error;
    bool ok = loadCTestHistory(settings, project, build, h, error) && !h.exists && h.runs.empty();
    ok = ok && loadCTestHistory(settings, project, build, stale, error);
    TestResults run; TestResult t; t.name = "example"; t.duration = "1.23"; t.status = TestResultStatus::Failed; t.output = "SUPER_SECRET_OUTPUT"; t.failureText = "SUPER_SECRET_FAILURE"; run.tests.push_back(t);
    ok = ok && appendCTestHistory(settings, h, run, error) && h.runs.size() == 1 && h.baseline.find("SUPER_SECRET") == std::string::npos;
    ok = ok && !appendCTestHistory(settings, stale, run, error);
    CTestHistorySnapshot loaded; ok = ok && loadCTestHistory(settings, project, build, loaded, error) && loaded.runs.size() == 1 && loaded.runs[0].tests[0].status == TestResultStatus::Failed && loaded.runs[0].tests[0].output.empty();
    for (int i = 0; i < 12; ++i) ok = ok && appendCTestHistory(settings, loaded, run, error);
    ok = ok && loaded.runs.size() == 10;
    const auto summaries = summarizeCTestHistory(loaded.runs);
    ok = ok && summaries.size() == 1 && summaries[0].name == "example" && summaries[0].failed == 10 &&
        summaries[0].consecutiveFailures == 10 && summaries[0].latestDurationSeconds && summaries[0].durationDeltaSeconds &&
        *summaries[0].durationDeltaSeconds == 0.0;
    ok = ok && loadCTestHistory(settings, project, other, distinct, error) && !distinct.exists && distinct.filename != h.filename;
    run.tests[0].duration = "secret data"; ok = ok && !appendCTestHistory(settings, loaded, run, error);
    { std::ofstream f(settings / loaded.filename, std::ios::trunc); f << "{\"project\":\"x\",\"project\":\"y\"}"; }
    CTestHistorySnapshot invalid; ok = ok && !loadCTestHistory(settings, project, build, invalid, error);
#ifndef _WIN32
    fs::remove(settings / loaded.filename); fs::create_symlink(settings / "elsewhere", settings / loaded.filename);
    ok = ok && !loadCTestHistory(settings, project, build, invalid, error);
#endif
    fs::remove_all(base); return ok ? 0 : 1;
}
