#include "modules/CTestRunner.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace lucid::modules;
int main(int argc, char** argv) {
#ifndef _WIN32
    if (argc > 3 && std::string(argv[1]) == "--test-dir") {
        for (int i = 3; i < argc; ++i) {
            if (std::string(argv[i]) == "--show-only=json-v1") {
                std::cout << R"({"kind":"ctestInfo","version":{"major":1},"tests":[{"name":"evil"}]})";
                return 0;
            }
            if (std::string(argv[i]) == "--output-junit" && i + 1 < argc) {
                std::error_code ec;
                std::filesystem::create_symlink(std::filesystem::path(argv[2]) / "fake.xml", argv[i + 1], ec);
                return ec ? 1 : 0;
            }
        }
        return 1;
    }
#endif
    if (argc == 2 && std::string(argv[1]) == "--pass") return 0;
    if (argc == 2 && std::string(argv[1]) == "--fail") return 1;
    if (argc != 2 || !ctestSingleRunSupported()) return 1;
    if (!findCTestOnPath(std::filesystem::path(argv[1]).parent_path().string()).size() || !findCTestOnPath("relative").empty()) return 7;
    namespace fs = std::filesystem; const auto root = fs::temp_directory_path() / ("lucid-ctest-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    const auto self = fs::absolute(argv[0]).generic_string();
    { std::ofstream file(root / "CTestTestfile.cmake"); file << "add_test(\"ok.test\" \"" << self << "\" \"--pass\")\nadd_test(\"fail.test\" \"" << self << "\" \"--fail\")\nadd_test(\"okXtest\" \"" << self << "\" \"--fail\")\nadd_test(\"setup\" \"" << self << "\" \"--fail\")\nset_tests_properties(setup PROPERTIES FIXTURES_SETUP example)\nset_tests_properties(ok.test PROPERTIES FIXTURES_REQUIRED example)\n"; }
    auto discovery = discoverCTest(argv[1], root);
    if (!discovery.ok || discovery.discovery.tests.size() != 4) return 2;
    auto passed = runCTestSingle(argv[1], root, "ok.test");
    if (!passed.ok || passed.results.tests.size() != 1 || passed.results.tests[0].status != TestResultStatus::Passed) return 3;
    auto failed = runCTestSingle(argv[1], root, "fail.test");
    if (!failed.ok || failed.process.exitCode == 0 || failed.results.tests[0].status != TestResultStatus::Failed) return 4;
    if (runCTestSingle(argv[1], root, "missing").ok || runCTestSingle(argv[1], root, "ok.*").ok || runCTestSingle("ctest", root, "ok.test").ok) return 5;
    if (runCTestSingle(argv[1], root, "ok.test", std::chrono::milliseconds(0)).ok) return 6;
    std::atomic_bool cancel{true};
    auto cancelled = runCTestSingle(argv[1], root, "ok.test", std::chrono::seconds(5), &cancel);
    if (cancelled.ok || !cancelled.process.cancelled) return 8;
#ifndef _WIN32
    { std::ofstream file(root / "fake.xml"); file << "<testsuite><testcase name=\"evil\"/></testsuite>"; }
    const auto linked = runCTestSingle(fs::absolute(argv[0]), root, "evil");
    if (linked.ok || linked.error.empty()) return 9;
#endif
    fs::remove_all(root); return 0;
}
