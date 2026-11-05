#include "modules/PullRequestData.hpp"
#include <string>
using namespace lucid::modules;
int main() {
    PullRequestEndpoints endpoints; std::string urlError;
    if (!pullRequestEndpoints("https://github.com/Owner/repo.name/pull/42/", endpoints, urlError) || endpoints.pull != "https://api.github.com/repos/Owner/repo.name/pulls/42" || endpoints.files != endpoints.pull + "/files?per_page=100") return 6;
    for (const auto* url : {"https://github.com.evil.test/a/b/pull/1", "https://github.com/a/../pull/1", "https://github.com/a/b/pull/1?token=x", "https://github.com/a/b/pull/0", "https://user@github.com/a/b/pull/1", "https://github.com/a/b/pull/18446744073709551616"})
        if (pullRequestEndpoints(url, endpoints, urlError) || !endpoints.pull.empty()) return 7;
    const auto pr = R"({"number":42,"title":"Example","state":"open","body":null,"html_url":"https://example.test/pull/42","user":{"login":"alice"},"base":{"ref":"main"},"head":{"ref":"feature"}})";
    const auto files = R"([{"filename":"src/main.cpp","status":"modified"}])";
    const auto comments = R"([{"user":{"login":"bob"},"body":"review","path":"src/main.cpp","created_at":"2026-01-01"}])";
    const auto checks = R"({"total_count":1,"check_runs":[{"name":"build","status":"completed","conclusion":"failure","html_url":null}]})";
    PullRequestData result; std::string error;
    if (!parsePullRequestData(pr,files,comments,checks,true,result,error) || result.partial || result.number != "42" || result.files.size()!=1 || result.comments[0].author != "bob" || result.checks[0].conclusion != "failure") return 1;
    if (!parsePullRequestData(pr,"","",R"({"total_count":2,"check_runs":[]})",false,result,error) || !result.partial || result.warnings.size() < 4) return 2;
    if (parsePullRequestData(pr,"{}",comments,checks,true,result,error) || !result.files.empty()) return 3;
    if (!parsePullRequestData(R"({})","[]","[]",R"({"total_count":0,"check_runs":[]})",true,result,error) || !result.partial || result.warnings.empty()) return 4;
    if (parsePullRequestData(std::string(8U*1024U*1024U+1,'x'),"","","",false,result,error)) return 5;
    if (!parsePullRequestData(pr,R"([{"filename":"new.cpp","status":"renamed","previous_filename":"old.cpp","additions":3,"deletions":2,"patch":"@@ -1 +1 @@\n-old\n+new"}])",R"([{"user":{"login":"bob"},"body":"review","path":"new.cpp","created_at":"2026-01-01","line":17,"side":"RIGHT"}])",checks,true,result,error) ||
        !result.files[0].hasPatch || !result.files[0].hasCounts || result.files[0].previousPath != "old.cpp" || result.files[0].additions != 3 || result.files[0].deletions != 2 ||
        !result.comments[0].hasLine || result.comments[0].line != 17 || result.comments[0].side != "RIGHT") return 8;
    return 0;
}
