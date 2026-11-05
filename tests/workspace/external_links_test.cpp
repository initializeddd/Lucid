#include "modules/ExternalLinks.hpp"
int main() {
    using namespace lucid::modules; int calls=0;
    LinkTransport t=[&](std::string_view u,bool head)->LinkResponse { ++calls; if (u=="https://example.com/start") return {302,"/end",{}}; if (u=="https://example.com/end") return head ? LinkResponse{405,{},{}} : LinkResponse{200,{},{}}; return {404,{},{}}; };
    auto r=checkExternalLink("https://example.com/start",t); if (r.state!=LinkState::Ok || calls!=3) return 1;
    calls=0; r=checkExternalLink("https://user:pass@example.com/",t); if (r.state!=LinkState::Unverified || calls) return 2;
    r=checkExternalLink("https://example.com/?token=x",t); if (r.state!=LinkState::Unverified || calls) return 3;
    r=checkExternalLink("https://example.com/missing",t); if (r.state!=LinkState::Broken || r.status!=404) return 4;
    LinkTransport bad=[](std::string_view,bool){return LinkResponse{302,"http://example.com/",{}};}; r=checkExternalLink("https://example.com/",bad); if (r.state!=LinkState::Unverified) return 5;
    LinkTransport transient=[](std::string_view,bool){return LinkResponse{503,{},{}};}; r=checkExternalLink("https://example.com/",transient); if (r.state!=LinkState::Unverified) return 6;
    calls=0; r=checkApprovedExternalLink("https://example.com/start",nullptr,t);
    if (r.state!=LinkState::Unverified || calls!=1 || r.detail!="Redirect not followed") return 7;
    calls=0; r=checkApprovedExternalLink("https://user:pass@example.com/",nullptr,t);
    if (r.state!=LinkState::Unverified || calls) return 8;
    r=checkApprovedExternalLink("https://example.com/?token=x",nullptr,t);
    if (r.state!=LinkState::Unverified || calls) return 9;
    r=checkApprovedExternalLink("https://example.com/path#fragment",nullptr,t);
    if (r.state!=LinkState::Unverified || calls) return 10;
    std::atomic_bool cancelled=true; r=checkApprovedExternalLink("https://example.com/",&cancelled,t);
    if (r.state!=LinkState::Unverified || calls) return 11;
    LinkTransport onlyHead=[&](std::string_view,bool head) { ++calls; return LinkResponse{head?405:200,{},{}}; };
    r=checkApprovedExternalLink("https://example.com/",nullptr,onlyHead);
    if (r.state!=LinkState::Unverified || calls!=1) return 12;
    return 0;
}
