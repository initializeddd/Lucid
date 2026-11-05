#pragma once
#include "modules/ModuleProcess.hpp"
#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::workspace { class Project; }
namespace lucid::modules {
struct ContainerProjectAction {
    std::string name, operation, composeFile;
    std::vector<std::string> services;
};
// Declarative version-1 JSON: {"version":1,"actions":[{"name":"...",
// "operation":"build|up|stop|restart","composeFile":"compose.yaml",
// "services":["web"]}]}. No arbitrary flags, commands, file access or process execution.
[[nodiscard]] bool parseContainerProjectActions(std::string_view json, std::vector<ContainerProjectAction>& actions, std::string& error);
struct ContainerActionReview {
    std::string projectRoot, configPath, configSnapshot, composeSnapshot;
    std::filesystem::path executable;
    std::size_t actionIndex = 0;
    ContainerProjectAction action;
    std::vector<std::string> arguments;
};
// Reads the bounded project-local config and Compose file; does not run anything.
// The executable is selected by the user and must be an absolute docker/podman path.
[[nodiscard]] bool prepareContainerProjectAction(const workspace::Project& project, std::string_view configPath,
    std::size_t index, const std::filesystem::path& executable, ContainerActionReview& review,
    std::string& error, const std::atomic_bool* cancel = nullptr);
using ContainerActionRunner = std::function<ModuleProcessResult(const std::filesystem::path&, const std::vector<std::string>&, const std::atomic_bool*)>;
// Requires an explicit user confirmation. Revalidates both reviewed files and
// the reconstructed plan before calling runner; no shell is involved. There is
// still a race between checking files and the runtime reading them. Compose can
// run project-defined code, load other files, and contact a remote runtime.
[[nodiscard]] ModuleProcessResult runContainerProjectAction(const workspace::Project& project,
    const ContainerActionReview& review, bool confirmed, const ContainerActionRunner& runner,
    const std::atomic_bool* cancel = nullptr);
// Production runner: no shell, 120-second limit, 1 MiB output limit, closed stdin.
[[nodiscard]] ModuleProcessResult runBoundedContainerProjectAction(const workspace::Project& project,
    const ContainerActionReview& review, bool confirmed, const std::atomic_bool* cancel = nullptr);
}
