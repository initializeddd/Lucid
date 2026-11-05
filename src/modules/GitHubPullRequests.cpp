#include "modules/GitHubPullRequests.hpp"
#include "modules/ApiRequests.hpp"
#include "editor/json/Json.hpp"
#include "editor/document/Document.hpp"
#include "debug/Protocol.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace lucid::modules {
namespace {
constexpr std::size_t maxResponse = 4U * 1024U * 1024U, maxTotal = 8U * 1024U * 1024U;
constexpr unsigned maxPages = 20;
constexpr std::string_view emptyChecks = R"({"total_count":0,"check_runs":[]})";
bool safeToken(std::string_view token) {
    return token.size() <= 4096 && std::all_of(token.begin(), token.end(), [](unsigned char c) { return c >= 33 && c <= 126; });
}
bool parse(std::string_view input, editor::JsonValue& output) {
    editor::JsonError e; auto value = editor::parseJson(input, e);
    if (!value) return false;
    output = std::move(*value); return true;
}
bool validSha(std::string_view sha) {
    return sha.size() == 40 && std::all_of(sha.begin(), sha.end(), [](unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    });
}
}
GitHubResponse requestGitHubPullRequest(std::string_view url, std::string_view token, const std::atomic_bool* cancel) {
    if (!url.starts_with("https://api.github.com/") || url.size() > 2048 || !safeToken(token)) return {0, {}, "Invalid GitHub request"};
    ApiRequest request{"GitHub pull request", "GET", std::string(url), "", {{"Accept", "application/vnd.github+json"}, {"User-Agent", "Lucid-Editor"}, {"X-GitHub-Api-Version", "2022-11-28"}}};
    if (!token.empty()) request.headers.emplace("Authorization", "Bearer " + std::string(token));
    auto result = sendApiRequest(request, cancel);
    return {result.status, std::move(result.body), std::move(result.error)};
}
GitHubResponse requestGitHubReviewComment(std::string_view url, std::string_view token, std::string_view json, const std::atomic_bool* cancel) {
    if (!url.starts_with("https://api.github.com/repos/") || !url.ends_with("/comments") || url.size() > 2048 || token.empty() || !safeToken(token) || json.size() > 65536) return {0, {}, "Invalid GitHub request"};
    ApiRequest request{"GitHub review comment", "POST", std::string(url), std::string(json), {{"Accept", "application/vnd.github+json"}, {"Content-Type", "application/json"}, {"User-Agent", "Lucid-Editor"}, {"X-GitHub-Api-Version", "2022-11-28"}, {"Authorization", "Bearer " + std::string(token)}}};
    auto result = sendApiRequest(request, cancel);
    return {result.status, std::move(result.body), std::move(result.error)};
}
bool postGitHubReviewComment(std::string_view pullUrl, std::string_view token, std::string_view sha,
    std::string_view path, std::size_t line, std::string_view body, const GitHubPostTransport& transport,
    std::string& error, const std::atomic_bool* cancel) {
    error.clear(); PullRequestEndpoints endpoints; std::string textError;
    if (!transport || token.empty() || !safeToken(token) || !pullRequestEndpoints(pullUrl, endpoints, error) || !validSha(sha) ||
        path.empty() || path.size() > 4096 || path.front() == '/' || path.find('\\') != path.npos ||
        path.find('\0') != path.npos || !editor::Document::canEditLosslessly(path, textError) ||
        line == 0 || line > 1000000000 || body.empty() || body.size() > 16000 || !editor::Document::canEditLosslessly(body, textError)) {
        if (error.empty()) error = "Invalid review comment";
        return false;
    }
    if (cancel && cancel->load()) { error = "Comment cancelled before sending"; return false; }
    const auto json = std::string("{\"body\":") + debug::jsonString(body) + ",\"commit_id\":" + debug::jsonString(sha) + ",\"path\":" + debug::jsonString(path) + ",\"line\":" + std::to_string(line) + ",\"side\":\"RIGHT\"}";
    const auto response = transport(endpoints.pull + "/comments", token, json, cancel);
    if (!response.error.empty() || response.status != 201) { error = "Comment could not be confirmed (HTTP " + std::to_string(response.status) + "); refresh before retrying"; return false; }
    return true;
}

GitHubResponse requestGitHubReview(std::string_view url, std::string_view token, std::string_view json, const std::atomic_bool* cancel) {
    if (!url.starts_with("https://api.github.com/repos/") || !url.ends_with("/reviews") || url.size() > 2048 || token.empty() || !safeToken(token) || json.size() > 65536) return {0, {}, "Invalid GitHub request"};
    ApiRequest request{"GitHub review", "POST", std::string(url), std::string(json), {{"Accept", "application/vnd.github+json"}, {"Content-Type", "application/json"}, {"User-Agent", "Lucid-Editor"}, {"X-GitHub-Api-Version", "2022-11-28"}, {"Authorization", "Bearer " + std::string(token)}}};
    auto result = sendApiRequest(request, cancel);
    return {result.status, std::move(result.body), std::move(result.error)};
}
bool submitGitHubReview(std::string_view pullUrl, std::string_view token, std::string_view expectedHead,
                        GitHubReviewEvent event, std::string_view body, bool confirmed,
                        const GitHubTransport& get, const GitHubPostTransport& post, std::string& error,
                        const std::atomic_bool* cancel) {
    error.clear(); PullRequestEndpoints endpoints; std::string textError;
    const char* action = nullptr; const char* desiredState = nullptr;
    switch (event) {
        case GitHubReviewEvent::Approve: action="APPROVE"; desiredState="APPROVED"; break;
        case GitHubReviewEvent::RequestChanges: action="REQUEST_CHANGES"; desiredState="CHANGES_REQUESTED"; break;
        case GitHubReviewEvent::Comment: action="COMMENT"; desiredState="COMMENTED"; break;
        default: error="Invalid review action"; return false;
    }
    if (!confirmed || !get || !post || token.empty() || !safeToken(token) || !validSha(expectedHead) ||
        !pullRequestEndpoints(pullUrl,endpoints,error) || body.size()>16000 || !editor::Document::canEditLosslessly(body,textError) ||
        (event != GitHubReviewEvent::Approve && body.find_first_not_of(" \t\r\n") == std::string_view::npos)) {
        if (error.empty()) error="Invalid or unconfirmed review"; return false;
    }
    if (cancel && cancel->load()) { error="Review cancelled before sending"; return false; }
    GitHubResponse current;
    try { current=get(endpoints.pull,token,cancel); }
    catch (...) { error="Could not verify pull request head"; return false; }
    if (cancel && cancel->load()) { error="Review cancelled before sending"; return false; }
    if (!current.error.empty() || current.status != 200 || current.body.size()>maxResponse) { error="Could not verify pull request head"; return false; }
    editor::JsonValue root;
    if (!parse(current.body,root) || root.type!=editor::JsonType::Object) { error="Invalid pull request head response"; return false; }
    auto uniqueField=[](const editor::JsonValue& value,const char* key) -> const editor::JsonValue* {
        const editor::JsonValue* found=nullptr; for (const auto& child:value.children) if (child.key==key) { if (found) return nullptr; found=&child; } return found;
    };
    const auto* head=uniqueField(root,"head"), *state=uniqueField(root,"state");
    const auto* sha=head && head->type==editor::JsonType::Object ? uniqueField(*head,"sha") : nullptr;
    auto sameSha=[](std::string_view a,std::string_view b) { return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](unsigned char x,unsigned char y) { if (x>='A' && x<='F') x+= 'a'-'A'; if (y>='A' && y<='F') y+= 'a'-'A'; return x==y; }); };
    if (!sha || sha->type!=editor::JsonType::String || !validSha(sha->stringValue) || !sameSha(sha->stringValue,expectedHead) ||
        !state || state->type!=editor::JsonType::String || state->stringValue!="open") { error="Pull request changed or is no longer open; refresh before reviewing"; return false; }
    if (cancel && cancel->load()) { error="Review cancelled before sending"; return false; }
    const auto json=std::string("{\"commit_id\":")+debug::jsonString(expectedHead)+",\"event\":"+debug::jsonString(action)+",\"body\":"+debug::jsonString(body)+"}";
    GitHubResponse response;
    try { response=post(endpoints.pull+"/reviews",token,json,cancel); }
    catch (...) { error="Review could not be confirmed; refresh before retrying"; return false; }
    if (!response.error.empty() || response.status!=200 || response.body.size()>maxResponse) { error="Review could not be confirmed; refresh before retrying"; return false; }
    editor::JsonValue review;
    if (!parse(response.body,review) || review.type!=editor::JsonType::Object) { error="Review could not be confirmed; refresh before retrying"; return false; }
    const auto* returnedState=uniqueField(review,"state"), *returnedSha=uniqueField(review,"commit_id");
    if (!returnedState || returnedState->type!=editor::JsonType::String || returnedState->stringValue!=desiredState || !returnedSha ||
        returnedSha->type!=editor::JsonType::String || !validSha(returnedSha->stringValue) || !sameSha(returnedSha->stringValue,expectedHead)) {
        error="Review could not be confirmed; refresh before retrying"; return false;
    }
    return true;
}
bool fetchGitHubPullRequest(std::string_view url, std::string_view token, const GitHubTransport& transport,
                            PullRequestData& output, std::string& error, const std::atomic_bool* cancel) {
    error.clear(); PullRequestEndpoints endpoints;
    if (!transport || !safeToken(token) || !pullRequestEndpoints(url, endpoints, error)) { if (error.empty()) error = "Invalid GitHub request"; return false; }
    const auto cancelled = [&] { return cancel && cancel->load(); };
    auto get = [&](const std::string& target, std::string& body) {
        if (cancelled()) { error = "GitHub request cancelled"; return false; }
        auto response = transport(target, token, cancel);
        if (cancelled()) { error = "GitHub request cancelled"; return false; }
        if (!response.error.empty() || response.status != 200) { error = "GitHub request failed (HTTP " + std::to_string(response.status) + ")"; return false; }
        if (response.body.size() > maxResponse) { error = "GitHub response exceeds size limit"; return false; }
        body = std::move(response.body); return true;
    };
    std::string pr;
    if (!get(endpoints.pull, pr)) return false;
    editor::JsonValue root;
    if (!parse(pr, root) || root.type != editor::JsonType::Object) { error = "Invalid pull request response"; return false; }
    PullRequestData result;
    if (!parsePullRequestData(pr, "[]", "[]", emptyChecks, true, result, error)) return false;
    const auto* head = root.find("head"); const auto* sha = head && head->type == editor::JsonType::Object ? head->find("sha") : nullptr;
    if (sha && sha->type == editor::JsonType::String && validSha(sha->stringValue)) result.headSha = sha->stringValue;
    std::string checks;
    const auto pos = endpoints.pull.rfind("/pulls/");
    if (sha && sha->type == editor::JsonType::String && validSha(sha->stringValue) && pos != std::string::npos)
        checks = endpoints.pull.substr(0, pos) + "/commits/" + sha->stringValue + "/check-runs?per_page=100";
    else { result.partial = true; result.warnings.push_back("Head SHA unavailable; check runs were not fetched"); }
    std::set<std::string> warnings(result.warnings.begin(), result.warnings.end());
    auto addWarning = [&](const std::string& warning) { if (warnings.insert(warning).second && result.warnings.size() < 1000) result.warnings.push_back(warning); result.partial = true; };
    auto pages = [&](const std::string& endpoint, int kind) {
        std::size_t totalBytes = 0, totalItems = 0; double advertised = 0; bool countKnown = false;
        for (unsigned page = 1; page <= maxPages; ++page) {
            std::string body;
            if (!get(endpoint + "&page=" + std::to_string(page), body)) return false;
            if (body.size() > maxTotal - totalBytes) { error = "GitHub pages exceed size limit"; return false; }
            totalBytes += body.size(); editor::JsonValue json;
            if (!parse(body, json)) { error = "Invalid GitHub page"; return false; }
            const editor::JsonValue* items = &json;
            if (kind == 2) {
                items = json.type == editor::JsonType::Object ? json.find("check_runs") : nullptr;
                const auto* count = json.find("total_count");
                if (count && count->type == editor::JsonType::Number && count->numberValue >= 0 && std::isfinite(count->numberValue) && std::floor(count->numberValue) == count->numberValue) { countKnown = true; advertised = std::max(advertised, count->numberValue); }
                else addWarning("Check run count unavailable");
            }
            if (!items || items->type != editor::JsonType::Array || items->children.size() > 100) { error = "Invalid GitHub page list"; return false; }
            const auto count = items->children.size(); totalItems += count; PullRequestData parsed;
            if (!parsePullRequestData(pr, kind == 0 ? body : "[]", kind == 1 ? body : "[]", kind == 2 ? body : emptyChecks, true, parsed, error)) return false;
            for (const auto& warning : parsed.warnings) if (warning != "Check runs are incomplete") addWarning(warning);
            if (kind == 0) result.files.insert(result.files.end(), parsed.files.begin(), parsed.files.end());
            else if (kind == 1) result.comments.insert(result.comments.end(), parsed.comments.begin(), parsed.comments.end());
            else result.checks.insert(result.checks.end(), parsed.checks.begin(), parsed.checks.end());
            if (count < 100) { if (kind == 2 && countKnown && advertised > static_cast<double>(totalItems)) addWarning("Check runs are incomplete"); return true; }
        }
        addWarning("GitHub page limit reached; results may be incomplete"); return true;
    };
    if (!pages(endpoints.files, 0) || !pages(endpoints.comments, 1) || (!checks.empty() && !pages(checks, 2))) return false;
    // File pages are keyed by PR number while check runs are keyed by the head
    // SHA captured above. A force-push during pagination can otherwise combine
    // different revisions and present them as a complete review.
    if (!checks.empty()) {
        std::string latest;
        if (!get(endpoints.pull, latest)) return false;
        editor::JsonValue finalRoot;
        if (!parse(latest, finalRoot) || finalRoot.type != editor::JsonType::Object) { error = "Invalid pull request response"; return false; }
        const auto* finalHead = finalRoot.find("head");
        const auto* finalSha = finalHead && finalHead->type == editor::JsonType::Object ? finalHead->find("sha") : nullptr;
        if (!finalSha || finalSha->type != editor::JsonType::String || !validSha(finalSha->stringValue))
            addWarning("Could not verify the final head revision; review data may be inconsistent");
        else if (finalSha->stringValue != sha->stringValue)
            addWarning("Pull request head changed during loading; review data may span revisions");
        const auto* initialBase = root.find("base");
        const auto* finalBase = finalRoot.find("base");
        const auto* initialBaseSha = initialBase && initialBase->type == editor::JsonType::Object ? initialBase->find("sha") : nullptr;
        const auto* finalBaseSha = finalBase && finalBase->type == editor::JsonType::Object ? finalBase->find("sha") : nullptr;
        if (initialBaseSha && finalBaseSha && initialBaseSha->type == editor::JsonType::String && finalBaseSha->type == editor::JsonType::String &&
            validSha(initialBaseSha->stringValue) && validSha(finalBaseSha->stringValue) && initialBaseSha->stringValue != finalBaseSha->stringValue)
            addWarning("Pull request base changed during loading; review data may span revisions");
    }
    output = std::move(result); return true;
}
}
