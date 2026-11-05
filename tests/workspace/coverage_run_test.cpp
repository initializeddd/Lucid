#include "modules/CoverageRun.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
int main() {
    using namespace lucid::modules; namespace fs=std::filesystem;
    auto root=fs::temp_directory_path()/("lucid-coverage-run-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);
#if defined(_WIN32)
    const fs::path executable=L"C:\\Windows\\System32\\cmd.exe";
#else
    const auto executable=root/"fake-coverage";std::ofstream(executable)<<"#!/bin/sh\nprintf invoked > generated.info\nexit 0\n";fs::permissions(executable,fs::perms::owner_all);
#endif
    const auto config=std::string("{\"executable\":\"")+executable.generic_string()+"\",\"arguments\":[\"--example\"],\"report\":\"coverage.info\",\"format\":\"lcov\"}";
    std::ofstream(root/"coverage.json")<<config;
    bool created=false;std::string error;auto project=lucid::workspace::Project::openOrCreate(root,created,error,false,false);
    auto plan=prepareCoverageRun(project,"coverage.json");if(!plan.error.empty() || plan.arguments.size()!=1 || plan.reportPath!="coverage.info")return 1;
    if(runCoverageGenerator(project,plan,false).error.empty())return 2;
    auto altered=plan;altered.arguments.push_back("bad");if(runCoverageGenerator(project,altered,true).error.empty())return 3;
    std::ofstream(root/"coverage.json",std::ios::trunc)<<"{}";if(runCoverageGenerator(project,plan,true).error.empty())return 4;
    std::ofstream(root/"coverage.json",std::ios::trunc)<<config;
    std::atomic_bool cancel{true};if(!runCoverageGenerator(project,plan,true,&cancel).cancelled)return 5;
#if !defined(_WIN32)
    auto result=runCoverageGenerator(project,plan,true);if(!result.error.empty() || result.exitCode || !fs::exists(root/"generated.info"))return 6;
#endif
    const auto benchmarkConfig=std::string("{\"executable\":\"")+executable.generic_string()+"\",\"arguments\":[],\"report\":\"bench.json\",\"format\":\"benchmark\"}";
    std::ofstream(root/"benchmark.json")<<benchmarkConfig;
    auto benchmark=prepareCoverageRun(project,"benchmark.json",ReportRunKind::Benchmark);
    if(!benchmark.error.empty() || benchmark.reportPath!="bench.json" || benchmark.kind!=ReportRunKind::Benchmark) return 7;
    if(prepareCoverageRun(project,"benchmark.json").error.empty() || prepareCoverageRun(project,"coverage.json",ReportRunKind::Benchmark).error.empty()) return 8;
    if(runCoverageGenerator(project,benchmark,false).error.empty()) return 9;
    fs::remove_all(root);return 0;
}
