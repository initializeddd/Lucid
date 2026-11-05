#pragma once
#include "workspace/Project.hpp"
#include <atomic>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
enum class MarkdownLinkStatus { Found, Missing, NotChecked };
struct MarkdownLink { std::string destination, detail; MarkdownLinkStatus status = MarkdownLinkStatus::NotChecked; std::optional<std::size_t> sourceOffset; };
struct MarkdownLinkResults { std::vector<MarkdownLink> links; bool incomplete = false; };
// Checks only project-local file targets; external URLs and fragments are not fetched.
[[nodiscard]] MarkdownLinkResults checkMarkdownLinks(const workspace::Project& project, std::string_view documentPath,
                                                      std::string_view markdown, const std::atomic_bool* cancel = nullptr,
                                                      std::size_t limit = 1000);
}
