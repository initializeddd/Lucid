#pragma once
#include "modules/ModuleProcess.hpp"
#include "modules/ToolchainProfiles.hpp"
#include "workspace/Project.hpp"
#include <atomic>
#include <string>
#include <vector>
namespace lucid::modules {
struct TargetConfigurePlan {
    std::string profileName, declaredTarget, buildRelative, sourceSnapshot, error;
    ToolchainProfile profile;
    std::filesystem::path projectRoot, executable, buildResolved;
    std::vector<std::string> arguments;
};
// Prepare a review-only plan. Build directory must already exist within the
// project and contain no symlink components. Compiler paths are declarations.
[[nodiscard]] TargetConfigurePlan prepareTargetConfigure(const workspace::Project& project, const ToolchainProfile& profile,
    std::string_view buildRelative, const std::filesystem::path& cmakeExecutable);
// Explicit confirmation is mandatory. Runs user-selected CMake with user project
// scripts and privileges, potentially changing build files or executing code.
// Refuses changed CMakeLists.txt or changed build path. Not a sandbox.
[[nodiscard]] ModuleProcessResult runTargetConfigure(const workspace::Project& project, const TargetConfigurePlan& plan,
    bool confirmed, const std::atomic_bool* cancel=nullptr);
}
