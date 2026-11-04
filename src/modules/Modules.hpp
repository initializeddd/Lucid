#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
struct Command { std::string title; std::string insert; };
struct Module {
    std::string id, name, version, description, error;
    bool project = false, network = false, processes = false, settings = false;
    bool enabled = false, valid = false;
    std::vector<Command> commands;
};
// Validate a manifest without installing or enabling it, for an install preview.
[[nodiscard]] Module preview(std::string_view id, std::string_view manifest);
// Install a reviewed manifest as a disabled module; never replace an existing one.
[[nodiscard]] bool install(const std::filesystem::path& root, std::string_view id,
                           std::string_view manifest, std::string& error);
[[nodiscard]] bool list(const std::filesystem::path& root, std::vector<Module>& result, std::string& error);
[[nodiscard]] bool setEnabled(const std::filesystem::path& root, std::string_view id, bool enabled, std::string& error);
[[nodiscard]] bool remove(const std::filesystem::path& root, std::string_view id, std::string& error);
}
