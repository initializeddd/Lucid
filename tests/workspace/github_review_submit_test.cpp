#include "modules/GitHubPullRequests.hpp"
#include <atomic>
#include <stdexcept>
#include <string>
using namespace lucid::modules;
int main() {
    const std::string sha(40,'a'), stale(40,'b'); const std::string url="https://github.com/a/b/pull/42", token="sensitive-token"; std::string error, payload; int gets=0, posts=0;
    GitHubTransport get=[&](std::string_view u,std::string_view t,const std::atomic_bool*) -> GitHubResponse {
        ++gets; if (u!="https://api.github.com/repos/a/b/pulls/42" || t!=token) return {0,{},"bad"};
        return {200,"{\"state\":\"open\",\"head\":{\"sha\":\""+sha+"\"}}",{}};
    };
    GitHubPostTransport post=[&](std::string_view u,std::string_view t,std::string_view p,const std::atomic_bool*) -> GitHubResponse {
        ++posts; payload=std::string(p); if (u!="https://api.github.com/repos/a/b/pulls/42/reviews" || t!=token) return {0,{},"bad"};
        return {200,"{\"state\":\"APPROVED\",\"commit_id\":\""+sha+"\"}",{}};
    };
    if (submitGitHubReview(url,token,sha,GitHubReviewEvent::Approve,"",false,get,post,error) || gets || posts) return 1;
    if (!submitGitHubReview(url,token,sha,GitHubReviewEvent::Approve,"",true,get,post,error) || gets!=1 || posts!=1 || payload.find("\"event\":\"APPROVE\"")==std::string::npos || payload.find(sha)==std::string::npos || payload.find(token)!=std::string::npos) return 2;
    if (submitGitHubReview(url,token,stale,GitHubReviewEvent::Approve,"",true,get,post,error) || posts!=1 || error.find("refresh")==std::string::npos) return 3;
    if (submitGitHubReview(url,token,sha,GitHubReviewEvent::Comment," ",true,get,post,error) || posts!=1) return 4;
    GitHubTransport duplicate=[&](std::string_view,std::string_view,const std::atomic_bool*) -> GitHubResponse {return {200,"{\"state\":\"open\",\"head\":{\"sha\":\""+sha+"\",\"sha\":\""+stale+"\"}}",{}};};
    if (submitGitHubReview(url,token,sha,GitHubReviewEvent::Approve,"",true,duplicate,post,error) || posts!=1) return 5;
    GitHubPostTransport uncertain=[&](std::string_view,std::string_view,std::string_view,const std::atomic_bool*) -> GitHubResponse {++posts; throw std::runtime_error(token);};
    if (submitGitHubReview(url,token,sha,GitHubReviewEvent::Approve,"",true,get,uncertain,error) || posts!=2 || error.find(token)!=std::string::npos || error.find("refresh") == std::string::npos) return 6;
    std::atomic_bool cancelled{true}; if (submitGitHubReview(url,token,sha,GitHubReviewEvent::Approve,"",true,get,post,error,&cancelled) || posts!=2) return 7;
    GitHubTransport closed=[&](std::string_view,std::string_view,const std::atomic_bool*) -> GitHubResponse {return {200,"{\"state\":\"closed\",\"head\":{\"sha\":\""+sha+"\"}}",{}};};
    if (submitGitHubReview(url,token,sha,GitHubReviewEvent::Approve,"",true,closed,post,error) || posts!=2) return 9;
    GitHubPostTransport wrongState=[&](std::string_view,std::string_view,std::string_view,const std::atomic_bool*) -> GitHubResponse {++posts; return {200,"{\"state\":\"PENDING\",\"commit_id\":\""+sha+"\"}",{}};};
    if (submitGitHubReview(url,token,sha,GitHubReviewEvent::Approve,"",true,get,wrongState,error) || posts!=3 || error.find("refresh")==std::string::npos) return 10;
    for (const auto event : {GitHubReviewEvent::RequestChanges,GitHubReviewEvent::Comment}) {
        GitHubPostTransport valid=[&](std::string_view,std::string_view,std::string_view p,const std::atomic_bool*) -> GitHubResponse {++posts; const char* expected=event==GitHubReviewEvent::Comment?"COMMENT":"REQUEST_CHANGES"; const char* state=event==GitHubReviewEvent::Comment?"COMMENTED":"CHANGES_REQUESTED"; if(p.find(expected)==p.npos)return {0,{},"bad"};return {200,"{\"state\":\""+std::string(state)+"\",\"commit_id\":\""+sha+"\"}",{}};};
        if (!submitGitHubReview(url,token,sha,event,"Please check",true,get,valid,error)) return 8;
    }
    return 0;
}
