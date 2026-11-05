#pragma once
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>
namespace lucid::modules {
struct WebPreviewState { std::string name, url; };
struct WebPreviewConfig {
    std::filesystem::path entry; // Project-relative HTML file.
    std::size_t width = 0, height = 0;
    std::vector<WebPreviewState> states;
};
// Validates existing project-contained HTML files and relative local state URLs.
// State URLs may use a relative HTML path, query and fragment, or just query/fragment
// for the entry. No filesystem mutation, server, browser, or network access.
[[nodiscard]] bool validateWebPreviewConfig(const std::filesystem::path& projectRoot,
    const WebPreviewConfig& config, std::string& error);
}
