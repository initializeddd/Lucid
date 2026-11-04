#include "workspace/collaboration/Connection.hpp"
#include "workspace/collaboration/ShareLink.hpp"
#include "workspace/collaboration/SharedDocument.hpp"
#include <chrono>
#if !defined(_WIN32)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>
#endif
namespace lucid::workspace::collaboration {
Connection::~Connection() { close(); }
bool Connection::connect(std::string_view linkText, std::string_view name, std::string& error) {
    close(); ShareLink link;
    SharedDocument validation;
    if (!parseShareLink(linkText, link, error) || !validation.join("self", std::string(name))) { error = "Invalid share link or display name"; return false; }
#if defined(_WIN32)
    error = "Joining is not yet available on Windows"; return false;
#else
    addrinfo hints{}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr; const auto port = std::to_string(link.port);
    if (getaddrinfo(link.host.c_str(), port.c_str(), &hints, &result) != 0) { error = "Could not resolve the private host"; return false; }
    for (auto* item = result; item; item = item->ai_next) {
        const auto* address = reinterpret_cast<const sockaddr_in*>(item->ai_addr);
        const auto value = ntohl(address->sin_addr.s_addr);
        if ((value >> 24) != 127 && (value & 0xffc00000U) != 0x64400000U) continue;
        const int fd = socket(AF_INET, SOCK_STREAM, 0); if (fd < 0) continue;
        (void)fcntl(fd, F_SETFD, FD_CLOEXEC); const int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) { ::close(fd); continue; }
#if defined(SO_NOSIGPIPE)
        int option = 1; (void)setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &option, sizeof(option));
#endif
        int status = ::connect(fd, item->ai_addr, item->ai_addrlen);
        if (status < 0 && errno == EINPROGRESS) {
            pollfd descriptor{fd, POLLOUT, 0};
            if (::poll(&descriptor, 1, 5000) > 0) { int socketError = 0; socklen_t length = sizeof(socketError); status = getsockopt(fd, SOL_SOCKET, SO_ERROR, &socketError, &length) == 0 && socketError == 0 ? 0 : -1; }
        }
        if (status == 0) { m_fd = fd; break; }
        ::close(fd);
    }
    freeaddrinfo(result);
    if (m_fd < 0) { error = "Could not connect to a private overlay host"; return false; }
    const std::string hello = "{\"type\":\"join\",\"version\":3,\"name\":" + debug::jsonString(name) + ",\"token\":" + debug::jsonString(link.token) + "}";
    if (!send(hello, error)) { close(); return false; }
    error.clear(); return true;
#endif
}
bool Connection::send(std::string_view json, std::string& error) {
    if (m_fd < 0) { error = "Not connected"; return false; }
    const auto bytes = debug::frame(json);
    if (bytes.size() > 8U * 1024U * 1024U || m_pending.size() > 8U * 1024U * 1024U - bytes.size()) { error = "Session message exceeds limit"; return false; }
    m_pending += bytes; error.clear(); return true;
}
bool Connection::poll(std::vector<std::string>& messages, std::string& error) {
#if defined(_WIN32)
    (void)messages; error = "Joining is not yet available on Windows"; return false;
#else
    if (m_fd < 0) { error = "Not connected"; return false; }
    const auto initialMessageCount = messages.size();
    while (!m_pending.empty()) {
#if defined(MSG_NOSIGNAL)
        const auto n = ::send(m_fd, m_pending.data(), m_pending.size(), MSG_NOSIGNAL);
#else
        const auto n = ::send(m_fd, m_pending.data(), m_pending.size(), 0);
#endif
        if (n > 0) { m_pending.erase(0, static_cast<std::size_t>(n)); continue; }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        error = "Session connection closed"; close(); return false;
    }
    char buffer[8192];
    for (;;) {
        const auto n = recv(m_fd, buffer, sizeof(buffer), 0);
        if (n > 0) { if (!m_decoder.append(std::string_view(buffer, static_cast<std::size_t>(n)), messages, error)) { close(); return false; } continue; }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { error.clear(); return true; }
        const bool delivered = messages.size() > initialMessageCount;
        error = delivered ? std::string{} : "Session connection closed"; close(); return delivered;
    }
#endif
}
void Connection::close() noexcept {
#if !defined(_WIN32)
    if (m_fd >= 0) ::close(m_fd);
#endif
    m_fd = -1; m_pending.clear(); m_decoder.reset();
}
}
