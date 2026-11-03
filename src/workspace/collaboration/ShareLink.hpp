#pragma once
#include <cstdint>
#include <string>
#include <string_view>
namespace lucid::workspace::collaboration {
struct ShareLink { std::string host; std::uint16_t port = 0; std::string token; };
// The token is a secret bearer credential; do not log share links.
[[nodiscard]] bool newShareToken(std::string& token, std::string& error);
[[nodiscard]] bool parseShareLink(std::string_view text, ShareLink& link, std::string& error);
[[nodiscard]] std::string formatShareLink(const ShareLink& link);
[[nodiscard]] bool sameToken(std::string_view expected, std::string_view received) noexcept;
}
