#pragma once
#include "modules/TestRunnerPlus.hpp"
#include "modules/ModuleProcess.hpp"
#include <chrono>
#include <filesystem>
#include <string>
namespace lucid::modules {
struct CTestRunResult { ModuleProcessResult process; TestResults results; std::string error; bool ok = false; };
struct CTestDiscoveryResult { ModuleProcessResult process; TestDiscovery discovery; std::string error; bool ok = false; };
// List existing absolute PATH candidates without running or selecting one.
[[nodiscard]] std::vector<std::filesystem::path> findCTestOnPath(std::string_view pathVariable);
// True when a private temporary JUnit report directory is supported.
[[nodiscard]] bool ctestSingleRunSupported() noexcept;
// Explicit user-selected executable/build; run only upon explicit user request.
// Tests execute arbitrary project code. Run these synchronous helpers off the UI thread.
[[nodiscard]] CTestDiscoveryResult discoverCTest(const std::filesystem::path& executable, const std::filesystem::path& buildDirectory, const std::atomic_bool* cancel = nullptr);
[[nodiscard]] CTestRunResult runCTestSingle(const std::filesystem::path& executable, const std::filesystem::path& buildDirectory, const std::string& name, std::chrono::milliseconds timeout = std::chrono::minutes(5), const std::atomic_bool* cancel = nullptr);
}
