#include "modules/ApiRequests.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <set>

namespace lucid::modules {
namespace {
bool validHeaderName(std::string_view s) {
    if (s.empty() || s.size() > 256) return false;
    for (unsigned char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || std::string_view("!#$%&'*+-.^_`|~").find(c) != std::string_view::npos)) return false;
    return true;
}
bool noControl(std::string_view s) {
    return std::none_of(s.begin(), s.end(), [](unsigned char c) { return c < 32 || c == 127; });
}
struct State { ApiResponse result; const std::atomic_bool* cancel; bool limit = false; };
// These fixed messages intentionally omit libcurl's error buffer, which can
// contain URLs, credentials or server-controlled text.
const char* transportError(CURLcode code) {
    switch (code) {
    case CURLE_COULDNT_RESOLVE_HOST: return "Could not resolve the host";
    case CURLE_COULDNT_CONNECT: return "Could not connect to the host";
    case CURLE_OPERATION_TIMEDOUT: return "The HTTP request timed out";
    case CURLE_PEER_FAILED_VERIFICATION: return "TLS certificate verification failed";
    case CURLE_SSL_CONNECT_ERROR: return "Could not establish a TLS connection";
    case CURLE_SEND_ERROR: return "Could not send the HTTP request";
    case CURLE_RECV_ERROR: case CURLE_GOT_NOTHING: case CURLE_PARTIAL_FILE:
        return "The HTTP response was interrupted or incomplete";
    case CURLE_UNSUPPORTED_PROTOCOL: case CURLE_URL_MALFORMAT: return "Invalid request URL";
    default: return "HTTP request failed";
    }
}
}
ApiResponse sendApiRequest(const ApiRequest& request, const std::atomic_bool* cancel) {
    if (cancel && cancel->load()) return {0, {}, {}, "Request cancelled"};
    if (request.method.empty() || request.method.size() > 16 ||
        !std::all_of(request.method.begin(), request.method.end(), [](char c) { return c >= 'A' && c <= 'Z'; }) ||
        request.url.size() > 8192 || !noControl(request.url) || request.url.find(' ') != std::string::npos ||
        request.body.size() > 2U * 1024U * 1024U || request.headers.size() > 100)
        return {0, {}, {}, "Invalid request"};
    if ((!request.url.starts_with("https://") && !request.url.starts_with("http://")) ||
        (request.method == "HEAD" && !request.body.empty())) return {0, {}, {}, "Invalid request"};
    const auto authorityStart = request.url.find("://") + 3;
    const auto authorityEnd = request.url.find_first_of("/?#", authorityStart);
    if (authorityStart == authorityEnd || request.url.substr(authorityStart, authorityEnd - authorityStart).find('@') != std::string::npos)
        return {0, {}, {}, "Invalid request URL"};
    static std::once_flag once; std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    using Url = std::unique_ptr<CURLU, decltype(&curl_url_cleanup)>;
    Url url(curl_url(), curl_url_cleanup);
    if (!url || curl_url_set(url.get(), CURLUPART_URL, request.url.c_str(), 0) != CURLUE_OK)
        return {0, {}, {}, "Invalid request URL"};
    auto part = [&](CURLUPart p) { char* raw = nullptr; std::string value; if (curl_url_get(url.get(), p, &raw, 0) == CURLUE_OK) { value = raw; curl_free(raw); } return value; };
    const auto scheme = part(CURLUPART_SCHEME);
    if ((scheme != "http" && scheme != "https") || part(CURLUPART_HOST).empty() || !part(CURLUPART_USER).empty() || !part(CURLUPART_PASSWORD).empty())
        return {0, {}, {}, "Invalid request URL"};
    using Handle = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
    Handle curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl) return {0, {}, {}, "Could not initialize HTTP"};
    using Headers = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;
    Headers headers(nullptr, curl_slist_free_all);
    std::size_t total = 0;
    std::set<std::string> names;
    for (const auto& [name, value] : request.headers) {
        total += name.size() + value.size();
        if (!validHeaderName(name) || !noControl(value) || total > 65536) return {0, {}, {}, "Invalid request headers"};
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : static_cast<char>(c); });
        // libcurl controls message framing; accepting conflicting framing fields
        // could cause request smuggling on a persistent server connection.
        if (!names.insert(lower).second || lower == "content-length" || lower == "transfer-encoding" || lower == "proxy-authorization")
            return {0, {}, {}, "Invalid request headers"};
        // libcurl's "Name:" removes a header; "Name;" sends it empty.
        const auto line = value.empty() ? name + ";" : name + ": " + value;
        auto* next = curl_slist_append(headers.get(), line.c_str());
        if (!next) return {0, {}, {}, "Could not allocate HTTP headers"};
        (void)headers.release(); headers.reset(next);
    }
    State state{{}, cancel, false};
    curl_easy_setopt(curl.get(), CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl.get(), CURLOPT_PROXY, "");
    curl_easy_setopt(curl.get(), CURLOPT_NETRC, CURL_NETRC_IGNORED);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT_MS, 5000L);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT_MS, 30000L);
    curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, request.method.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
    if (!request.body.empty() || request.method == "POST" || request.method == "PUT" || request.method == "PATCH") { curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, request.body.data()); curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(request.body.size())); }
    if (request.method == "HEAD") curl_easy_setopt(curl.get(), CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, +[](void* data, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int { const auto* s = static_cast<State*>(data); return s->cancel && s->cancel->load() ? 1 : 0; });
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &state);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, +[](char* data, size_t size, size_t count, void* context) -> size_t { auto& s = *static_cast<State*>(context); if (size && count > (4U * 1024U * 1024U - s.result.body.size()) / size) { s.limit = true; return 0; } const auto n = size * count; s.result.body.append(data, n); return n; });
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &state);
    curl_easy_setopt(curl.get(), CURLOPT_HEADERFUNCTION, +[](char* data, size_t size, size_t count, void* context) -> size_t { auto& s = *static_cast<State*>(context); if (size && count > (65536U - s.result.headers.size()) / size) { s.limit = true; return 0; } const auto n = size * count; s.result.headers.append(data, n); return n; });
    curl_easy_setopt(curl.get(), CURLOPT_HEADERDATA, &state);
    const auto start = std::chrono::steady_clock::now();
    const auto code = curl_easy_perform(curl.get());
    state.result.elapsedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &state.result.status);
    if (code != CURLE_OK) state.result.error = cancel && cancel->load() ? "Request cancelled" : state.limit ? "Response exceeds size limit" : transportError(code);
    return state.result;
}
}
