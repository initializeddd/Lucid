#include "modules/ExternalLinks.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <cctype>
#include <memory>
#include <mutex>
#include <string>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif
namespace lucid::modules {
namespace {
using Url = std::unique_ptr<CURLU, decltype(&curl_url_cleanup)>;
void init() { static std::once_flag once; std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); }); }
std::string part(CURLU* u, CURLUPart p) { char* raw = nullptr; if (curl_url_get(u,p,&raw,0) != CURLUE_OK) return {}; std::string s(raw); curl_free(raw); return s; }
bool safe(CURLU* u) {
    auto scheme=part(u,CURLUPART_SCHEME), host=part(u,CURLUPART_HOST);
    auto whole = part(u,CURLUPART_URL); auto authorityStart = whole.find("://");
    if (authorityStart == std::string::npos || whole.find('?', authorityStart + 3) != std::string::npos) return false;
    auto authorityEnd = whole.find_first_of("/#", authorityStart + 3);
    if (whole.substr(authorityStart + 3, authorityEnd - (authorityStart + 3)).find('@') != std::string::npos) return false;
    if ((scheme!="http" && scheme!="https") || host.empty() || !part(u,CURLUPART_USER).empty() || !part(u,CURLUPART_PASSWORD).empty() || !part(u,CURLUPART_QUERY).empty()) return false;
    return true;
}
bool normalize(std::string_view input, std::string& out) {
    if (input.empty() || input.size()>8192 || std::any_of(input.begin(),input.end(),[](unsigned char c){return c<=32 || c==127;})) return false;
    Url u(curl_url(),curl_url_cleanup); if (!u || curl_url_set(u.get(),CURLUPART_URL,std::string(input).c_str(),0)!=CURLUE_OK || !safe(u.get())) return false;
    curl_url_set(u.get(),CURLUPART_FRAGMENT,nullptr,0); out=part(u.get(),CURLUPART_URL); return !out.empty();
}
bool resolve(const std::string& base, const std::string& location, std::string& out) {
    if (location.empty() || location.size()>8192 || std::any_of(location.begin(),location.end(),[](unsigned char c){return c<=32 || c==127;})) return false;
    Url u(curl_url(),curl_url_cleanup); if (!u || curl_url_set(u.get(),CURLUPART_URL,base.c_str(),0)!=CURLUE_OK || curl_url_set(u.get(),CURLUPART_URL,location.c_str(),0)!=CURLUE_OK || !safe(u.get())) return false;
    if (base.starts_with("https://") && part(u.get(),CURLUPART_SCHEME)!="https") return false;
    curl_url_set(u.get(),CURLUPART_FRAGMENT,nullptr,0); out=part(u.get(),CURLUPART_URL); return !out.empty();
}
bool publicAddress(const curl_sockaddr& address) {
    if (address.family == AF_INET && address.addrlen >= sizeof(sockaddr_in)) {
        const auto& in = reinterpret_cast<const sockaddr_in&>(address.addr);
        const auto* b = reinterpret_cast<const unsigned char*>(&in.sin_addr);
        return b[0] != 0 && b[0] != 10 && b[0] != 127 && b[0] < 224 &&
            !(b[0] == 100 && (b[1] & 0xc0) == 64) && !(b[0] == 169 && b[1] == 254) &&
            !(b[0] == 172 && b[1] >= 16 && b[1] <= 31) &&
            !(b[0] == 192 && (b[1] == 168 || (b[1] == 0 && (b[2] == 0 || b[2] == 2)) || (b[1] == 88 && b[2] == 99))) &&
            !(b[0] == 198 && (b[1] == 18 || b[1] == 19 || (b[1] == 51 && b[2] == 100))) &&
            !(b[0] == 203 && b[1] == 0 && b[2] == 113);
    }
    if (address.family == AF_INET6 && address.addrlen >= sizeof(sockaddr_in6)) {
        const auto& in = reinterpret_cast<const sockaddr_in6&>(address.addr);
        const auto* b = reinterpret_cast<const unsigned char*>(&in.sin6_addr);
        // Global unicast only; exclude reserved/documentation and transition space.
        return (b[0] & 0xe0) == 0x20 && !(b[0] == 0x20 && b[1] == 0x02) &&
            !(b[0] == 0x20 && b[1] == 0x01 && (b[2] < 0x02 || (b[2] == 0x0d && b[3] == 0xb8))) &&
            !(b[0] == 0x3f && b[1] == 0xff && (b[2] & 0xf0) == 0);
    }
    return false;
}
LinkResponse requestApproved(const std::string& url, const std::atomic_bool* cancel) {
    using Handle = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
    Handle c(curl_easy_init(), curl_easy_cleanup);
    if (!c) return {0, {}, "Cannot initialize HTTP"};
    curl_easy_setopt(c.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(c.get(), CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(c.get(), CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(c.get(), CURLOPT_PROXY, "");
    curl_easy_setopt(c.get(), CURLOPT_NETRC, CURL_NETRC_IGNORED);
    curl_easy_setopt(c.get(), CURLOPT_NOBODY, 1L);
    curl_easy_setopt(c.get(), CURLOPT_CONNECTTIMEOUT_MS, 3000L);
    curl_easy_setopt(c.get(), CURLOPT_TIMEOUT_MS, 5000L);
    curl_easy_setopt(c.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c.get(), CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(c.get(), CURLOPT_XFERINFOFUNCTION, +[](void* data, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
        const auto* flag = static_cast<const std::atomic_bool*>(data); return flag && flag->load() ? 1 : 0;
    });
    curl_easy_setopt(c.get(), CURLOPT_XFERINFODATA, cancel);
    curl_easy_setopt(c.get(), CURLOPT_OPENSOCKETFUNCTION, +[](void*, curlsocktype purpose, curl_sockaddr* address) -> curl_socket_t {
        if (purpose != CURLSOCKTYPE_IPCXN || !address || !publicAddress(*address)) return CURL_SOCKET_BAD;
        return ::socket(address->family, address->socktype, address->protocol);
    });
    // Discard headers; libcurl's default header output must not print URLs.
    std::size_t receivedHeaders = 0;
    curl_easy_setopt(c.get(), CURLOPT_HEADERFUNCTION, +[](char*, size_t size, size_t count, void* data) -> size_t {
        auto& received = *static_cast<std::size_t*>(data);
        if (size && count > (65536 - received) / size) return 0;
        const auto bytes = size * count; received += bytes; return bytes;
    });
    curl_easy_setopt(c.get(), CURLOPT_HEADERDATA, &receivedHeaders);
    const auto code = curl_easy_perform(c.get()); long status = 0;
    curl_easy_getinfo(c.get(), CURLINFO_RESPONSE_CODE, &status);
    if (code != CURLE_OK) return {status, {}, "Request failed"};
    return {status, {}, {}};
}
}
LinkCheck checkApprovedExternalLink(std::string_view url, const std::atomic_bool* cancel, const LinkTransport& transport) {
    init(); std::string normalized;
    if ((cancel && cancel->load()) || url.find('#') != std::string_view::npos || !normalize(url, normalized))
        return {LinkState::Unverified, 0, "Cancelled or unsupported URL"};
    const auto response = transport ? transport(normalized, true) : requestApproved(normalized, cancel);
    if ((cancel && cancel->load()) || !response.error.empty()) return {LinkState::Unverified, response.status, "Request failed or cancelled"};
    if (response.status >= 200 && response.status < 300) return {LinkState::Ok, response.status, "HTTP target responded"};
    if (response.status == 404 || response.status == 410) return {LinkState::Broken, response.status, "HTTP target not found"};
    if (response.status >= 300 && response.status < 400) return {LinkState::Unverified, response.status, "Redirect not followed"};
    return {LinkState::Unverified, response.status, "HTTP status inconclusive"};
}
LinkCheck checkExternalLink(std::string_view url, const LinkTransport& transport, unsigned maxRedirects) {
    init(); std::string current; if (!transport || !normalize(url,current)) return {LinkState::Unverified,0,"Invalid or credential-bearing URL"};
    maxRedirects=std::min(maxRedirects, 10u);
    for (unsigned redirects=0;;) {
        auto response=transport(current,true);
        if (!response.error.empty()) return {LinkState::Unverified,response.status,"Request failed"};
        if (response.status==405 || response.status==501) { response=transport(current,false); if (!response.error.empty()) return {LinkState::Unverified,response.status,"Request failed"}; }
        if (response.status>=200 && response.status<300) return {LinkState::Ok,response.status,{}};
        if (response.status>=300 && response.status<400 && response.status!=304) {
            std::string next; if (redirects>=maxRedirects || !resolve(current,response.location,next)) return {LinkState::Unverified,response.status,"Unsafe or excessive redirect"};
            current=std::move(next); ++redirects; continue;
        }
        if (response.status==404 || response.status==410) return {LinkState::Broken,response.status,"Not found"};
        return {LinkState::Unverified,response.status,"HTTP status inconclusive"};
    }
}
LinkResponse requestExternalLink(std::string_view url, bool head) {
    init(); std::string normalized; if (!normalize(url,normalized)) return {0,{},"Invalid URL"};
    using Handle=std::unique_ptr<CURL,decltype(&curl_easy_cleanup)>; Handle c(curl_easy_init(),curl_easy_cleanup); if (!c) return {0,{},"Cannot initialize HTTP"};
    struct State { std::string location; std::size_t headerBytes=0; bool stopped=false; } state;
    curl_easy_setopt(c.get(),CURLOPT_URL,normalized.c_str()); curl_easy_setopt(c.get(),CURLOPT_PROTOCOLS_STR,"http,https"); curl_easy_setopt(c.get(),CURLOPT_FOLLOWLOCATION,0L); curl_easy_setopt(c.get(),CURLOPT_PROXY,""); curl_easy_setopt(c.get(),CURLOPT_NETRC,CURL_NETRC_IGNORED); curl_easy_setopt(c.get(),CURLOPT_CONNECTTIMEOUT_MS,3000L); curl_easy_setopt(c.get(),CURLOPT_TIMEOUT_MS,5000L); curl_easy_setopt(c.get(),CURLOPT_NOSIGNAL,1L); curl_easy_setopt(c.get(),CURLOPT_NOBODY,head ? 1L : 0L);
    curl_easy_setopt(c.get(),CURLOPT_HEADERFUNCTION,+[](char* p,size_t a,size_t b,void* v)->size_t { auto& s=*static_cast<State*>(v); if (a && b>65536/a) return 0; auto n=a*b; if (n>65536-s.headerBytes) return 0; s.headerBytes+=n; std::string_view line(p,n); constexpr std::string_view key="location:"; if (line.size()>=key.size() && std::equal(key.begin(),key.end(),line.begin(),[](char x,char y){return x==static_cast<char>(std::tolower(static_cast<unsigned char>(y)));})) { line.remove_prefix(key.size()); while (!line.empty() && (line.front()==' ' || line.front()=='\t')) line.remove_prefix(1); while (!line.empty() && (line.back()=='\r' || line.back()=='\n' || line.back()==' ' || line.back()=='\t')) line.remove_suffix(1); s.location=std::string(line); } return n; });
    curl_easy_setopt(c.get(),CURLOPT_HEADERDATA,&state); curl_easy_setopt(c.get(),CURLOPT_WRITEFUNCTION,+[](char*,size_t,size_t,void* v)->size_t { static_cast<State*>(v)->stopped=true; return 0; }); curl_easy_setopt(c.get(),CURLOPT_WRITEDATA,&state);
    auto code=curl_easy_perform(c.get()); long status=0; curl_easy_getinfo(c.get(),CURLINFO_RESPONSE_CODE,&status);
    if (code!=CURLE_OK && !(code==CURLE_WRITE_ERROR && state.stopped && status>0)) return {status,{},"HTTP transport failed"};
    return {status,std::move(state.location),{}};
}
}
