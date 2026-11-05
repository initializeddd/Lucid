#pragma once
#include "workspace/Project.hpp"
#include <cstddef>
#include <atomic>
#include <string>
#include <vector>
namespace lucid::workspace {
struct ProjectSearchMatch { std::size_t file = 0, offset = 0, line = 0, column = 0; std::string preview; };
struct ProjectSearchFile { std::string path, original; };
struct ProjectSearchSkippedFile { std::string path, reason; };
struct ProjectSearchResults { std::vector<ProjectSearchFile> files; std::vector<ProjectSearchMatch> matches; std::vector<ProjectSearchSkippedFile> skippedFiles; std::size_t skipped = 0; bool truncated = false; bool indexIncomplete = false; bool caseSensitive = true; std::size_t readErrors = 0; std::string firstReadErrorPath; std::string firstSkippedPath; std::string firstSkippedReason; };
[[nodiscard]] bool validProjectPathFilter(std::string_view filter);
[[nodiscard]] std::string replacementPreview(const ProjectSearchFile& file, const ProjectSearchMatch& match,
                                              std::string_view query, std::string_view replacement, bool caseSensitive = true);
[[nodiscard]] ProjectSearchResults searchProject(const Project& project, std::string_view query, std::size_t limit = 1000,
                                                 const std::atomic_bool* cancel = nullptr, bool wholeWord = false, bool caseSensitive = true,
                                                 std::string_view pathFilter = {});
[[nodiscard]] bool replaceProject(Project& project, const ProjectSearchResults& results, std::string_view query,
                                  std::string_view replacement, std::size_t& changed, std::string& error,
                                  std::vector<std::string>* changedPaths = nullptr,
                                  const std::filesystem::path& historyRoot = {});
}
