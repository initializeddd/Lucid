#include "modules/Modules.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
using namespace lucid::modules;
int main() {
    const auto root = std::filesystem::temp_directory_path() / ("lucid-modules-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root / "example");
    auto manifest = [&](bool network) {
        std::ofstream f(root / "example/module.json", std::ios::trunc);
        f << "{\"name\":\"Example\",\"version\":\"1\",\"permissions\":{\"project\":false,\"network\":" << (network ? "true" : "false") << ",\"processes\":false,\"settings\":false},\"commands\":[{\"title\":\"Hello\",\"insert\":\"hello\\n\"}]}";
    };
    std::string error; std::vector<Module> modules;
    manifest(false);
    if (!list(root, modules, error) || modules.size() != 1 || !modules[0].valid || modules[0].enabled || modules[0].commands[0].insert != "hello\n") return 1;
    if (!setEnabled(root, "example", true, error) || !list(root, modules, error) || !modules[0].enabled) return 2;
    manifest(true);
    if (!list(root, modules, error) || modules[0].enabled || !modules[0].network || setEnabled(root, "example", true, error)) return 3;
    if (!setEnabled(root, "example", false, error)) return 4;
    if (setEnabled(root, "../escape", true, error) || remove(root, "../escape", error)) return 5;
    { std::ofstream f(root / "example/module.json", std::ios::trunc); f << "{}"; }
    if (!list(root, modules, error) || modules[0].valid || setEnabled(root, "example", true, error)) return 6;
    if (!remove(root, "example", error) || std::filesystem::exists(root / "example")) return 7;
    std::filesystem::remove_all(root); return 0;
}
