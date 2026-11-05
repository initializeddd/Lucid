#include "modules/Modules.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "Expected module directory\n"; return 1; }
    const auto root = std::filesystem::path(argv[1]);
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec) || ec) { std::cerr << "Module directory unavailable\n"; return 1; }
    std::vector<lucid::modules::Module> modules; std::string error;
    if (!lucid::modules::list(root, modules, error)) { std::cerr << error << '\n'; return 1; }
    // A count alone lets an unrelated extra module mask a missing requested one.
    constexpr std::array expected{
        "lucid-accessibility-test-runner", "lucid-api-request-studio", "lucid-benchmark-comparison",
        "lucid-binary-inspector", "lucid-change-archaeology", "lucid-cmake-navigator",
        "lucid-container-tools", "lucid-cpp-starter", "lucid-crash-dump-reader",
        "lucid-dependency-risk-review", "lucid-design-token-explorer", "lucid-diagram-preview",
        "lucid-embedded-cpp", "lucid-hex-editor", "lucid-local-error-explainer",
        "lucid-localization-workspace", "lucid-log-correlation", "lucid-markdown-studio",
        "lucid-microcontroller-dashboard", "lucid-pull-request-companion", "lucid-python-environments",
        "lucid-release-note-builder", "lucid-rust-ownership-lens", "lucid-secret-scanner",
        "lucid-shader-workbench", "lucid-snippet-toolkit", "lucid-sql-workbench",
        "lucid-test-coverage-lens", "lucid-test-runner-plus", "lucid-visual-regression-review",
        "lucid-web-component-preview"
    };
    for (const auto* id : expected) {
        if (std::none_of(modules.begin(), modules.end(), [id](const auto& module) { return module.id == id; })) {
            std::cerr << "Missing requested module: " << id << '\n'; return 1;
        }
    }
    for (const auto& module : modules) {
#if !defined(__APPLE__)
        if (module.id == "lucid-web-component-preview" && module.valid && module.error == "Requires unsupported capability: web-component-preview-macos-v1") continue;
#endif
        if (!module.valid || !module.error.empty()) {
            std::cerr << module.id << ": " << (module.error.empty() ? "Invalid manifest" : module.error) << '\n';
            return 1;
        }
    }
    return 0;
}
