#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
struct PullRequestFile {
    std::string path, status, previousPath, patch;
    std::size_t additions = 0, deletions = 0;
    bool hasPatch = false, hasCounts = false;
};
struct PullRequestComment {
    std::string author, body, path, createdAt, side;
    std::size_t line = 0;
    bool hasLine = false;
};
struct PullRequestCheck { std::string name, status, conclusion, url; };
struct PullRequestConversation { std::string author, body, createdAt; };
struct PullRequestData {
    std::string number, title, state, body, url, author, baseRef, headRef, headSha;
    std::vector<PullRequestFile> files;
    std::vector<PullRequestComment> comments;
    std::vector<PullRequestConversation> conversation;
    std::vector<PullRequestCheck> checks;
    std::vector<std::string> warnings;
    bool partial = false;
};
struct PullRequestEndpoints {
    std::string pull, files, comments, conversation;
};
// Accepts a canonical public GitHub pull-request URL and returns fixed-host API
// endpoints. No request or authentication is performed. Pagination and checks
// are separate operations; the latter needs the head commit SHA from the API.
[[nodiscard]] bool pullRequestEndpoints(std::string_view url, PullRequestEndpoints& output, std::string& error);
// Inputs are JSON bodies from GitHub's pull request, list-files, list-review-
// comments, and list-check-runs REST endpoints. Empty optional inputs mark
// data missing. Array endpoints may be paginated: caller must indicate whether
// all pages were supplied; no network calls are made.
[[nodiscard]] bool parsePullRequestData(std::string_view pull, std::string_view files, std::string_view comments,
    std::string_view checks, bool allPagesSupplied, PullRequestData& output, std::string& error);
// A bounded page of issue comments belonging to a pull request. Parsing never
// performs I/O, and unsupported entries mark the supplied data incomplete.
[[nodiscard]] bool parsePullRequestConversation(std::string_view json, PullRequestData& output, std::string& error);
}
