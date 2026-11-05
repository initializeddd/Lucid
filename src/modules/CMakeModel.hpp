#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include "workspace/Project.hpp"
#include <vector>

namespace lucid::modules {
struct CMakeCompileGroup { std::string language; std::vector<std::string> includes, definitions, flags; };
struct CMakeSource { std::string path; int compileGroup = -1; bool generated = false; };
struct CMakeTarget { std::string configuration, name, type; std::vector<CMakeSource> sources; std::vector<CMakeCompileGroup> groups; };
struct CMakeOption { std::string name, value, description; };
struct CMakeModel { std::string sourceDirectory, buildDirectory; std::vector<CMakeTarget> targets; std::vector<CMakeOption> options; bool optionsAvailable = false; };
struct CMakeCacheUpdate { std::string path, original, replacement; };
// Prepare an explicit, conflict-checked BOOL edit without configuring CMake.
[[nodiscard]] bool prepareCMakeBoolCacheUpdate(const workspace::Project& project, std::string_view relativeCachePath,
    std::string_view optionName, std::string_view expectedValue, bool enabled, CMakeCacheUpdate& update, std::string& error);
[[nodiscard]] bool saveCMakeBoolCacheUpdate(workspace::Project& project, const CMakeCacheUpdate& update, std::string& error);
// Reads existing CMake File API replies; does not configure or execute CMake.
[[nodiscard]] bool readCMakeModel(const std::filesystem::path& buildDirectory, CMakeModel& model, std::string& error);
}
