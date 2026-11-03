#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::workspace {
struct GitChange { std::string path; std::string status; bool conflict = false; std::string oldPath; };
struct GitReviewLine { std::string text; int line = 0; };
struct GitReview { std::vector<GitReviewLine> lines; };
struct GitHistoryEntry { std::string hash, author, date, subject; };
[[nodiscard]] bool gitHistory(const std::filesystem::path& root, std::vector<GitHistoryEntry>& entries, std::string& error);
[[nodiscard]] bool gitChanges(const std::filesystem::path& root, std::vector<GitChange>& changes, std::string& error);
[[nodiscard]] bool gitReview(const std::filesystem::path& root, const GitChange& change, GitReview& review, std::string& error);
struct GitCommitPreview { std::vector<std::string> paths; std::string indexTree; std::string head; };
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
