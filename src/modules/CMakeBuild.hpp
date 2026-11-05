#pragma once
#include "modules/CMakeModel.hpp"
#include "modules/ModuleProcess.hpp"
#include <atomic>
#include <string>
#include <vector>
namespace lucid::modules {
struct CMakeBuildPlan {
    std::filesystem::path projectRoot, buildDirectory, executable;
    std::string buildRelative, target, configuration, targetType, sourceSnapshot, error;
    std::vector<std::string> arguments;
};
// Preparation reads File API metadata only. Build directory must already exist
// inside project, without symbolic-link components. The selected target must be
// uniquely present in the latest File API model for that project/build pair.
[[nodiscard]] CMakeBuildPlan prepareCMakeBuild(const workspace::Project& project, std::string_view buildRelative,
    std::string_view target, std::string_view configuration, const std::filesystem::path& cmakeExecutable);
// Explicit confirmation runs CMake/build tools and project code with user
// privileges, can modify files, and is not a sandbox. Cancellation is best-effort.
[[nodiscard]] ModuleProcessResult runCMakeBuild(const workspace::Project& project, const CMakeBuildPlan& plan,
    bool confirmed, const std::atomic_bool* cancel=nullptr);
}
