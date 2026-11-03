#include "workspace/collaboration/ShareLink.hpp"
#include <array>
#include <charconv>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#elif defined(__APPLE__)
#include <stdlib.h>
#else
#include <cerrno>
#include <sys/random.h>
#endif
namespace lucid::workspace::collaboration {
namespace {
bool validHost(std::string_view host) {
    if (host.empty() || host.size() > 253 || host.front() == '.' || host.back() == '.') return false;
    for (char c : host) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-')) return false;
    return true;
}
bool validToken(std::string_view token) {
    if (token.size() != 64) return false;
    for (char c : token) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}
}
bool newShareToken(std::string& token, std::string& error) {
    std::array<unsigned char, 32> random{};
#if defined(_WIN32)
    if (BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) { error = "Could not create a secure session token"; return false; }
#elif defined(__APPLE__)
    arc4random_buf(random.data(), random.size());
#else
    std::size_t written = 0;
    while (written < random.size()) {
        const auto n = getrandom(random.data() + written, random.size() - written, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { error = "Could not create a secure session token"; return false; }
        written += static_cast<std::size_t>(n);
    }
#endif
    static constexpr char hex[] = "0123456789abcdef";
    token.clear(); token.reserve(64);
    for (unsigned char c : random) { token.push_back(hex[c >> 4]); token.push_back(hex[c & 15]); }
    error.clear(); return true;
}
bool parseShareLink(std::string_view text, ShareLink& link, std::string& error) {
    constexpr std::string_view prefix = "lucid://";
    if (!text.starts_with(prefix) || text.size() > 512) { error = "Invalid share link"; return false; }
    text.remove_prefix(prefix.size()); const auto colon = text.find(':'); const auto slash = text.find('/');
    if (colon == std::string_view::npos || slash == std::string_view::npos || colon >= slash || text.find('/', slash + 1) != std::string_view::npos) { error = "Invalid share link"; return false; }
    const auto host = text.substr(0, colon), portText = text.substr(colon + 1, slash - colon - 1), token = text.substr(slash + 1);
    unsigned int port = 0; const auto parsed = std::from_chars(portText.data(), portText.data() + portText.size(), port);
    if (!validHost(host) || portText.empty() || parsed.ec != std::errc{} || parsed.ptr != portText.data() + portText.size() || port < 1 || port > 65535 || !validToken(token)) { error = "Invalid share link"; return false; }
    link = {std::string(host), static_cast<std::uint16_t>(port), std::string(token)}; error.clear(); return true;
}
std::string formatShareLink(const ShareLink& link) {
    if (!validHost(link.host) || !link.port || !validToken(link.token)) return {};
    return "lucid://" + link.host + ":" + std::to_string(link.port) + "/" + link.token;
}
bool sameToken(std::string_view expected, std::string_view received) noexcept {
    if (!validToken(expected) || !validToken(received)) return false;
    unsigned int difference = 0;
    for (std::size_t i = 0; i < expected.size(); ++i) difference |= static_cast<unsigned char>(expected[i]) ^ static_cast<unsigned char>(received[i]);
    return difference == 0;
}
}
