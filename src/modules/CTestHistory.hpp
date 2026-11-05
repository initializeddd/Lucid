#pragma once
#include "modules/TestRunnerPlus.hpp"
#include <filesystem>
#include <string>
#include <vector>
namespace lucid::modules {
struct CTestHistorySnapshot {
    std::filesystem::path settingsRoot, projectRoot, buildDirectory;
    std::vector<TestResults> runs; // Oldest first; only names, status, and duration are retained.
    std::string filename, baseline;
    bool exists = false;
};
// All directories must already exist. Settings directory must be user-controlled.
// Build and project paths are canonicalized, and identities are checked on reload.
[[nodiscard]] bool loadCTestHistory(const std::filesystem::path& settingsDirectory,
    const std::filesystem::path& projectRoot, const std::filesystem::path& buildDirectory,
    CTestHistorySnapshot& history, std::string& error);
// Append only an explicit completed run, keeping at most 10 runs. No test output is saved.
// Uses a conflict-checked baseline; reload if a concurrent save fails.
[[nodiscard]] bool appendCTestHistory(const std::filesystem::path& settingsDirectory,
    CTestHistorySnapshot& history, const TestResults& run, std::string& error);
}
