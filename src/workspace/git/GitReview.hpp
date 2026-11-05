#pragma once
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::workspace {
struct GitChange { std::string path; std::string status; bool conflict = false; std::string oldPath; };
struct GitReviewLine { std::string text; int line = 0; };
struct GitReview { std::vector<GitReviewLine> lines; };
struct GitHistoryEntry { std::string hash, author, date, subject; };
struct GitFileHistoryEntry { std::string hash, author, date, subject, path, previousPath; };
struct GitRangeHistoryEntry { std::string hash, author, date, subject; GitReview review; };
struct GitLineAttribution { std::size_t line = 0; std::string hash, author, summary, text; };
struct GitBranch { std::string name, commit; bool current = false; };
struct GitBranches { std::string head; std::vector<GitBranch> branches; };
[[nodiscard]] bool gitBranches(const std::filesystem::path& root, GitBranches& result, std::string& error);
// Refuses a dirty worktree or changes to HEAD or the selected branch since review.
[[nodiscard]] bool gitSwitchBranch(const std::filesystem::path& root, const GitBranches& expected,
                                   const GitBranch& target, std::string& output, std::string& error);
[[nodiscard]] bool gitHistory(const std::filesystem::path& root, std::vector<GitHistoryEntry>& entries, std::string& error);
// File history follows renames while the historical path remains inside the project.
[[nodiscard]] bool gitFileHistory(const std::filesystem::path& root, std::string_view path, std::vector<GitFileHistoryEntry>& entries, std::string& error, bool* stoppedAtBoundary = nullptr);
// Trace a selected one-based inclusive range in the exact committed source.
// Restricted to a repository-root project, so rename tracking cannot expose
// paths outside a nested project. Historical lines are never current locations.
[[nodiscard]] bool gitRangeHistory(const std::filesystem::path& root, std::string_view path,
                                   std::string_view expectedSource, std::size_t first, std::size_t last,
                                   std::vector<GitRangeHistoryEntry>& entries, std::string& error);
// Attribution is anchored to a captured HEAD and requires the exact committed
// bytes. Returns at most 1000 current lines; metadata is untrusted Git data.
[[nodiscard]] bool gitLineAttribution(const std::filesystem::path& root, std::string_view path,
    std::string_view expectedSource, std::vector<GitLineAttribution>& entries, bool& truncated, std::string& error);
[[nodiscard]] bool gitHistoricalFileDiff(const std::filesystem::path& root, std::string_view path, std::string_view commit, GitReview& review, std::string& error, std::string_view previousPath = {});
// Reads a historical project-relative blob without consulting or changing the working tree.
// Rejects binary and incomplete output; contents are untrusted historical text.
[[nodiscard]] bool gitHistoricalFileText(const std::filesystem::path& root, std::string_view path, std::string_view commit, std::string& text, std::string& error);
[[nodiscard]] bool gitChanges(const std::filesystem::path& root, std::vector<GitChange>& changes, std::string& error);
[[nodiscard]] bool gitReview(const std::filesystem::path& root, const GitChange& change, GitReview& review, std::string& error);
struct GitCommitPreview { std::vector<std::string> paths; std::string indexTree; std::string head; };
// Read the blob recorded in the reviewed tree, never the mutable working file.
// `present` is false for a staged deletion. Non-regular entries are rejected.
[[nodiscard]] bool gitReadReviewedBlob(const std::filesystem::path& root, const GitCommitPreview& preview,
                                       std::string_view path, std::string& contents, bool& present, std::string& error);
struct GitUpstream { std::string branch, remote, mergeRef, head; };
enum class GitRemoteAction { Fetch, Pull, Push };
[[nodiscard]] bool gitUpstream(const std::filesystem::path& root, GitUpstream& upstream, std::string& error);
[[nodiscard]] bool gitRemote(const std::filesystem::path& root, const GitUpstream& expected, GitRemoteAction action, std::string& output, std::string& error);
[[nodiscard]] bool gitStage(const std::filesystem::path& root, const GitChange& change, std::string& error);
[[nodiscard]] bool gitUnstage(const std::filesystem::path& root, const GitChange& change, std::string& error);
[[nodiscard]] bool gitCommitPreview(const std::filesystem::path& root, GitCommitPreview& preview, std::string& error);
[[nodiscard]] bool gitCommit(const std::filesystem::path& root, const GitCommitPreview& preview, std::string_view message, std::string& output, std::string& error);
[[nodiscard]] bool gitBlame(const std::filesystem::path& root, std::string_view path, int line, std::string& author, std::string& error);
}
