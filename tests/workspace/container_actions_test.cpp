#include "modules/ContainerActions.hpp"
#include "workspace/Project.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
using namespace lucid::modules;
int main() {
    const auto root = std::filesystem::temp_directory_path() / ("lucid-container-actions-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { std::filesystem::path root; ~Cleanup() { std::error_code ec; std::filesystem::remove_all(root, ec); } } cleanup{root};
    std::filesystem::create_directories(root); std::string error; bool created = false;
    auto project = lucid::workspace::Project::openOrCreate(root, created, error, false);
    if (!error.empty()) return 1;
    const std::string config = R"({"version":1,"actions":[{"name":"Build app","operation":"build","composeFile":"compose.yaml","services":["api","worker"]},{"name":"Start","operation":"up","composeFile":"compose.yaml","services":["api"]}]})";
    { std::ofstream out(root / "actions.json"); out << config; }
    { std::ofstream out(root / "compose.yaml"); out << "services: {}\n"; }
#ifdef _WIN32
    const auto runtime = root / "docker.exe";
#else
    const auto runtime = root / "docker";
#endif
    { std::ofstream out(runtime); out << "placeholder"; }
    std::vector<ContainerProjectAction> actions;
    if (!parseContainerProjectActions(config, actions, error) || actions.size() != 2 || actions[0].services.size() != 2) return 2;
    for (const auto* invalid : {
        R"({"version":1,"actions":[{"name":"bad","operation":"exec","composeFile":"a","services":["x"]}]})",
        R"({"version":1,"actions":[{"name":"bad","operation":"up","composeFile":"a","services":["--privileged"]}]})",
        R"({"version":1,"actions":[{"name":"bad","operation":"up","composeFile":"a","services":[]}]})",
        R"({"version":1,"actions":[{"name":"bad","operation":"up","composeFile":"a","services":["x"],"args":["--force"]}]})",
        R"({"version":1,"actions":[{"name":"bad","name":"x","operation":"up","composeFile":"a","services":["x"]}]})"})
        if (parseContainerProjectActions(invalid, actions, error) || !actions.empty()) return 3;
    ContainerActionReview review;
    if (!prepareContainerProjectAction(project, "actions.json", 0, runtime, review, error) || review.action.operation != "build" || review.arguments.size() != 9 || review.arguments[0] != "compose" || review.arguments[5] != "build" || review.arguments[6] != "--") return 4;
    int calls = 0; const ContainerActionRunner runner = [&](const auto& exe, const auto& args, const auto*) { ++calls; ModuleProcessResult result; if (exe != runtime || args != review.arguments) result.error = "Unexpected arguments"; else { result.exitCode = 0; result.output = "ok"; } return result; };
    if (runContainerProjectAction(project, review, false, runner).error.empty() || calls) return 5;
    auto result = runContainerProjectAction(project, review, true, runner); if (!result.error.empty() || result.exitCode != 0 || calls != 1) return 6;
    auto tampered = review; tampered.arguments.push_back("--privileged");
    if (runContainerProjectAction(project, tampered, true, runner).error.empty() || calls != 1) return 7;
    { std::ofstream out(root / "compose.yaml"); out << "changed\n"; }
    if (runContainerProjectAction(project, review, true, runner).error.empty() || calls != 1) return 8;
    { std::ofstream out(root / "compose.yaml"); out << "services: {}\n"; }
    { std::ofstream out(root / "actions.json"); out << config << " "; }
    if (runContainerProjectAction(project, review, true, runner).error.empty() || calls != 1) return 9;
    std::atomic_bool cancelled{true}; if (!runContainerProjectAction(project, review, true, runner, &cancelled).cancelled || calls != 1) return 10;
    { std::ofstream out(root / "actions.json"); out << R"({"version":1,"actions":[{"name":"outside","operation":"up","composeFile":"../outside.yaml","services":["x"]}]})"; }
    if (prepareContainerProjectAction(project, "actions.json", 0, runtime, review, error)) return 11;
    if (parseContainerProjectActions(std::string(128U * 1024U + 1, 'x'), actions, error)) return 12;
    return 0;
}
