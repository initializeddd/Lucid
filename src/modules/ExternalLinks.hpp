#pragma once
#include <functional>
#include <atomic>
#include <string>
#include <string_view>
namespace lucid::modules {
enum class LinkState { Ok, Broken, Unverified };
struct LinkResponse { long status = 0; std::string location, error; };
using LinkTransport = std::function<LinkResponse(std::string_view url, bool head)>;
struct LinkCheck { LinkState state = LinkState::Unverified; long status = 0; std::string detail; };
// Validates URLs and redirects before each request; query strings and userinfo
// are rejected because they may carry credentials. The transport must not follow redirects.
[[nodiscard]] LinkCheck checkExternalLink(std::string_view url, const LinkTransport& transport, unsigned maxRedirects = 5);
// Direct libcurl transport, no proxies, cookies, credentials, redirect following,
// or response body retention. Connect timeout 3s, request timeout 5s.
[[nodiscard]] LinkResponse requestExternalLink(std::string_view url, bool head);
// For an explicitly approved single link. Queries, fragments and userinfo are
// rejected; only HEAD is sent and redirects are reported without being followed.
// The default transport forbids proxies and private or reserved network addresses.
// The optional transport is an injectable test seam; it must enforce those same
// network restrictions if used outside tests.
[[nodiscard]] LinkCheck checkApprovedExternalLink(std::string_view url, const std::atomic_bool* cancel = nullptr,
                                                  const LinkTransport& transport = {});
}
