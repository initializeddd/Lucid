#pragma once
#include "modules/PullRequestData.hpp"
#include <atomic>
#include <functional>
#include <string>
#include <string_view>
namespace lucid::modules {
struct GitHubResponse { long status = 0; std::string body, error; };
using GitHubTransport = std::function<GitHubResponse(std::string_view url, std::string_view token, const std::atomic_bool* cancel)>;
// Fetches PR metadata, changed files, review comments and head check runs.
// This contacts GitHub and sends the optional token; the caller must explicitly
// obtain user consent. Pagination and input are bounded. Failure leaves output
// unchanged. It does not modify the pull request, cache tokens or log responses.
[[nodiscard]] bool fetchGitHubPullRequest(std::string_view url, std::string_view token,
    const GitHubTransport& transport, PullRequestData& output, std::string& error,
    const std::atomic_bool* cancel = nullptr);
// One-shot HTTPS GET to api.github.com only. No redirects, proxies or ambient
// credentials. Errors never include the token or response body.
[[nodiscard]] GitHubResponse requestGitHubPullRequest(std::string_view url, std::string_view token,
    const std::atomic_bool* cancel = nullptr);
using GitHubPostTransport = std::function<GitHubResponse(std::string_view url, std::string_view token, std::string_view json, const std::atomic_bool* cancel)>;
// Posts exactly one inline comment. Call only after explicit user confirmation.
// A transport error or cancellation can occur after GitHub accepted the comment;
// the caller must refresh before retrying. No automatic retry is attempted.
[[nodiscard]] bool postGitHubReviewComment(std::string_view pullUrl, std::string_view token,
    std::string_view commitSha, std::string_view path, std::size_t line, std::string_view body,
    const GitHubPostTransport& transport, std::string& error, const std::atomic_bool* cancel = nullptr);
[[nodiscard]] GitHubResponse requestGitHubReviewComment(std::string_view url, std::string_view token,
    std::string_view json, const std::atomic_bool* cancel = nullptr);
enum class GitHubReviewEvent { Approve, RequestChanges, Comment };
// Caller must confirm the concrete review before invoking. A fresh authenticated
// PR read checks the head and open state; the explicit commit is included in the
// one-shot POST. A head change can still race between that read and the POST.
// An uncertain POST result requires refresh before any retry.
[[nodiscard]] bool submitGitHubReview(std::string_view pullUrl, std::string_view token,
    std::string_view expectedHead, GitHubReviewEvent event, std::string_view body, bool confirmed,
    const GitHubTransport& get, const GitHubPostTransport& post, std::string& error,
    const std::atomic_bool* cancel = nullptr);
[[nodiscard]] GitHubResponse requestGitHubReview(std::string_view url, std::string_view token,
    std::string_view json, const std::atomic_bool* cancel = nullptr);

}
