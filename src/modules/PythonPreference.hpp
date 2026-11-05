#pragma once
#include <filesystem>
#include <string>
namespace lucid::modules {
// Snapshot is an opaque optimistic-write baseline; reload after each save.
struct PythonPreference {
    std::filesystem::path projectRoot, settingsRoot, interpreter;
    std::string baseline, filename;
    bool exists = false;
};
// Settings directory and project must already exist. No interpreter is executed.
// Empty interpreter denotes no selection; clearing persists a guarded tombstone.
[[nodiscard]] bool loadPythonPreference(const std::filesystem::path& settingsDirectory, const std::filesystem::path& projectRoot, PythonPreference& preference, std::string& error);
[[nodiscard]] bool savePythonPreference(const std::filesystem::path& settingsDirectory, PythonPreference& preference, const std::filesystem::path& interpreter, std::string& error);
}
