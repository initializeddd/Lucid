#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
[[nodiscard]] bool supportsCapability(std::string_view capability) noexcept;
struct Command { std::string title; std::string insert; bool snippet = false; std::string action; };
struct SnippetRange { std::size_t start = 0, end = 0; };
struct ExpandedSnippet { std::string text; std::size_t cursorLine = 0, cursorColumn = 0, endLine = 0, endColumn = 0, finalOffset = 0, finalFieldIndex = 0; bool hasCursor = false, hasPlaceholder = false; std::vector<SnippetRange> fields; };
// Expand literal placeholders without executing code or accessing files.
[[nodiscard]] bool expandSnippet(std::string_view text, std::string_view selection, ExpandedSnippet& result, std::string& error, bool placeholders = false, bool multiple = false);
struct Module {
    std::string id, name, version, description, error;
    bool project = false, network = false, processes = false, settings = false, devices = false;
    bool enabled = false, valid = false;
    std::vector<std::string> requirements;
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
