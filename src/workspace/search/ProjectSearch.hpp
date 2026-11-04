#pragma once
#include "workspace/Project.hpp"
#include <cstddef>
#include <atomic>
#include <string>
#include <vector>
namespace lucid::workspace {
struct ProjectSearchMatch { std::size_t file = 0, offset = 0, line = 0, column = 0; std::string preview; };
struct ProjectSearchFile { std::string path, original; };
struct ProjectSearchResults { std::vector<ProjectSearchFile> files; std::vector<ProjectSearchMatch> matches; std::size_t skipped = 0; bool truncated = false; bool indexIncomplete = false; };
[[nodiscard]] std::string replacementPreview(const ProjectSearchFile& file, const ProjectSearchMatch& match,
                                              std::string_view query, std::string_view replacement);
[[nodiscard]] ProjectSearchResults searchProject(const Project& project, std::string_view query, std::size_t limit = 1000,
                                                 const std::atomic_bool* cancel = nullptr, bool wholeWord = false);
[[nodiscard]] bool replaceProject(Project& project, const ProjectSearchResults& results, std::string_view query,
                                  std::string_view replacement, std::size_t& changed, std::string& error,
                                  std::vector<std::string>* changedPaths = nullptr,
                                  const std::filesystem::path& historyRoot = {});
}
