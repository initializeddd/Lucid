#include "modules/GitHubPullRequests.hpp"
#include <atomic>
#include <string>
using namespace lucid::modules;
int main() {
    const std::string pr = R"({"number":42,"title":"Example","state":"open","body":null,"html_url":"https://github.com/a/b/pull/42","user":{"login":"alice"},"base":{"ref":"main"},"head":{"ref":"branch","sha":"0123456789012345678901234567890123456789"}})";
    const std::string comment = R"({"user":{"login":"bob"},"body":"review","path":"main.cpp","created_at":"2026-01-01"})";
    const std::string check = R"({"name":"build","status":"completed","conclusion":"success","html_url":null})";
    std::string files = "[";
    for (int i = 0; i < 100; ++i) { if (i) files += ','; files += "{\"filename\":\"f" + std::to_string(i) + "\",\"status\":\"modified\"}"; }
    files += ']';
    int calls = 0;
    GitHubTransport mock = [&](std::string_view url, std::string_view token, const std::atomic_bool*) -> GitHubResponse {
        ++calls; if (!url.starts_with("https://api.github.com/") || token != "secret") return {0, {}, "unexpected input"};
        if (url.ends_with("/pulls/42")) return {200, pr, {}};
        if (url.ends_with("/files?per_page=100&page=1")) return {200, files, {}};
        if (url.ends_with("/files?per_page=100&page=2")) return {200, R"([{"filename":"last","status":"added"}])", {}};
        if (url.ends_with("/comments?per_page=100&page=1")) return {200, "[" + comment + "]", {}};
        if (url.ends_with("/check-runs?per_page=100&page=1")) return {200, "{\"total_count\":1,\"check_runs\":[" + check + "]}", {}};
        return {0, {}, "unexpected endpoint"};
    };
    PullRequestData data; std::string error;
    if (!fetchGitHubPullRequest("https://github.com/a/b/pull/42", "secret", mock, data, error) || calls != 6 || data.partial || data.files.size() != 101 || data.comments.size() != 1 || data.checks.size() != 1 || data.checks[0].conclusion != "success") return 1;
    GitHubTransport fail = [](std::string_view, std::string_view, const std::atomic_bool*) { return GitHubResponse{403, "sensitive", "secret"}; };
    if (fetchGitHubPullRequest("https://github.com/a/b/pull/42", "secret", fail, data, error) || data.files.size() != 101 || error.find("secret") != std::string::npos || error.find("sensitive") != std::string::npos) return 2;
    std::atomic_bool cancelled{true}; calls = 0;
    if (fetchGitHubPullRequest("https://github.com/a/b/pull/42", "secret", mock, data, error, &cancelled) || calls != 0 || data.files.size() != 101) return 3;
    if (fetchGitHubPullRequest("https://evil.test/a/b/pull/42", "secret", mock, data, error) || calls != 0) return 4;
    if (fetchGitHubPullRequest("https://github.com/a/b/pull/42", "bad\nheader", mock, data, error) || calls != 0) return 5;
    if (requestGitHubPullRequest("https://api.github.com.evil.test/x", "secret").error.empty()) return 6;
    calls = 0;
    GitHubTransport limited = [&](std::string_view url, std::string_view, const std::atomic_bool*) -> GitHubResponse {
        ++calls;
        if (url.ends_with("/pulls/42")) return {200, pr, {}};
        if (url.find("/files?") != std::string_view::npos) return {200, files, {}};
        if (url.find("/comments?") != std::string_view::npos) return {200, "[]", {}};
        return {200, R"({"total_count":1,"check_runs":[]})", {}};
    };
    if (!fetchGitHubPullRequest("https://github.com/a/b/pull/42", "", limited, data, error) || !data.partial || data.files.size() != 2000 || calls != 24 || data.warnings.size() < 2) return 7;
    cancelled.store(false); calls = 0;
    GitHubTransport midCancel = [&](std::string_view, std::string_view, const std::atomic_bool*) -> GitHubResponse { ++calls; cancelled.store(true); return {200, pr, {}}; };
    if (fetchGitHubPullRequest("https://github.com/a/b/pull/42", "", midCancel, data, error, &cancelled) || calls != 1 || data.files.size() != 2000) return 8;
    calls = 0;
    GitHubTransport changed = [&](std::string_view url, std::string_view, const std::atomic_bool*) -> GitHubResponse {
        if (url.ends_with("/pulls/42")) {
            ++calls;
            auto value = pr;
            if (calls == 2) value.replace(value.find("0123456789012345678901234567890123456789"), 40, 40, 'a');
            return {200, value, {}};
        }
        if (url.find("/check-runs?") != std::string_view::npos) return {200, R"({"total_count":0,"check_runs":[]})", {}};
        return {200, "[]", {}};
    };
    if (!fetchGitHubPullRequest("https://github.com/a/b/pull/42", "", changed, data, error) || !data.partial || data.warnings.empty() || data.warnings.back().find("head changed") == std::string::npos) return 9;
    calls = 0;
    GitHubTransport changedBase = [&](std::string_view url, std::string_view, const std::atomic_bool*) -> GitHubResponse {
        if (url.ends_with("/pulls/42")) {
            ++calls;
            auto value = pr;
            value.replace(value.find("\"base\":{\"ref\":\"main\"}"), 21,
                std::string("\"base\":{\"ref\":\"main\",\"sha\":\"") + std::string(40, calls == 1 ? 'a' : 'b') + "\"}");
            return {200, value, {}};
        }
        if (url.find("/check-runs?") != std::string_view::npos) return {200, R"({"total_count":0,"check_runs":[]})", {}};
        return {200, "[]", {}};
    };
    if (!fetchGitHubPullRequest("https://github.com/a/b/pull/42", "", changedBase, data, error) || !data.partial || data.warnings.empty() || data.warnings.back().find("base changed") == std::string::npos) return 10;
    unsigned posts = 0; std::string sent;
    GitHubPostTransport post = [&](std::string_view url, std::string_view token, std::string_view json, const std::atomic_bool*) -> GitHubResponse {
        ++posts; sent = std::string(json); if (url != "https://api.github.com/repos/a/b/pulls/42/comments" || token != "secret") return {0, {}, "wrong destination"}; return {201, "{}", {}};
    };
    const std::string sha(40, 'a');
    if (!postGitHubReviewComment("https://github.com/a/b/pull/42", "secret", sha, "src/file.cpp", 4, "Check \"this\"", post, error) || posts != 1 || sent.find("\\\"this\\\"") == std::string::npos || sent.find("\"line\":4") == std::string::npos) return 11;
    if (postGitHubReviewComment("https://github.com.evil/a/b/pull/42", "secret", sha, "x", 4, "x", post, error) || posts != 1) return 12;
    if (postGitHubReviewComment("https://github.com/a/b/pull/42", "", sha, "x", 4, "x", post, error) || posts != 1) return 13;
    if (postGitHubReviewComment("https://github.com/a/b/pull/42", "secret", "bad", "x", 0, "x", post, error) || posts != 1) return 14;
    cancelled.store(true);
    if (postGitHubReviewComment("https://github.com/a/b/pull/42", "secret", sha, "x", 4, "x", post, error, &cancelled) || posts != 1) return 15;
    cancelled.store(false);
    GitHubPostTransport uncertain = [](std::string_view, std::string_view, std::string_view, const std::atomic_bool*) -> GitHubResponse { return {0, {}, "timeout"}; };
    if (postGitHubReviewComment("https://github.com/a/b/pull/42", "secret", sha, "x", 4, "x", uncertain, error) || error.find("refresh before retrying") == std::string::npos) return 16;
    return 0;
}
