#include "workspace/collaboration/HostServer.hpp"
#include "workspace/collaboration/HostSession.hpp"
#include "workspace/collaboration/ShareLink.hpp"
#include "debug/Protocol.hpp"
#include <atomic>
#include <chrono>
#include <map>
#include <thread>
#if !defined(_WIN32)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <cerrno>
#endif
namespace lucid::workspace::collaboration {
struct HostServer::Impl {
    std::atomic_bool stopping{false}; std::thread thread; std::uint16_t port = 0;
#if !defined(_WIN32)
    int listener = -1;
#endif
};
HostServer::HostServer() : m_impl(std::make_unique<Impl>()) {}
HostServer::~HostServer() { stop(); }
std::uint16_t HostServer::port() const noexcept { return m_impl->port; }
bool HostServer::start(Project project, const std::string& bindAddress, std::uint16_t portValue, const std::string& token, std::string& error) {
    if (m_impl->thread.joinable()) { error = "A collaboration host is already running"; return false; }
#if defined(_WIN32)
    (void)project; (void)bindAddress; (void)portValue; (void)token;
    error = "Hosting is not yet available on Windows"; return false;
#else
    if (!sameToken(token, token)) { error = "Invalid session token"; return false; }
    in_addr address{};
    if (inet_pton(AF_INET, bindAddress.c_str(), &address) != 1) { error = "Choose a numeric private overlay address"; return false; }
    const auto value = ntohl(address.s_addr);
    if ((value >> 24) != 127 && (value & 0xffc00000U) != 0x64400000U) { error = "Bind to loopback or a 100.64.0.0/10 private overlay address"; return false; }
    const int listener = socket(AF_INET, SOCK_STREAM, 0); if (listener < 0) { error = "Could not create session listener"; return false; }
    int enabled = 1; (void)setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
    (void)fcntl(listener, F_SETFD, FD_CLOEXEC);
    const int flags = fcntl(listener, F_GETFL, 0);
    if (flags < 0 || fcntl(listener, F_SETFL, flags | O_NONBLOCK) < 0) { ::close(listener); error = "Could not configure session listener"; return false; }
    sockaddr_in endpoint{}; endpoint.sin_family = AF_INET; endpoint.sin_addr = address; endpoint.sin_port = htons(portValue);
    if (bind(listener, reinterpret_cast<sockaddr*>(&endpoint), sizeof(endpoint)) != 0 || listen(listener, 16) != 0) { ::close(listener); error = "Could not bind session listener"; return false; }
    socklen_t size = sizeof(endpoint); if (getsockname(listener, reinterpret_cast<sockaddr*>(&endpoint), &size) != 0) { ::close(listener); error = "Could not inspect session listener"; return false; }
    m_impl->listener = listener; m_impl->port = ntohs(endpoint.sin_port); m_impl->stopping = false;
    m_impl->thread = std::thread([state = m_impl.get(), project = std::move(project), token]() mutable {
        struct Peer { int fd; std::string id, output; debug::FrameDecoder decoder; bool authenticated = false; std::chrono::steady_clock::time_point deadline; };
        HostSession session(std::move(project), token); std::map<int, Peer> peers; unsigned long next = 1;
        auto deliver = [&peers](const std::vector<Delivery>& messages) {
            for (const auto& message : messages) for (auto& [fd, peer] : peers) {
                (void)fd; if (peer.id != message.peer || message.json.empty()) continue;
                const auto bytes = debug::frame(message.json);
                if (bytes.size() > 8U * 1024U * 1024U || peer.output.size() > 8U * 1024U * 1024U - bytes.size()) { peer.output.clear(); peer.deadline = std::chrono::steady_clock::time_point::min(); }
                else peer.output += bytes;
            }
        };
        auto drop = [&session, &peers, &deliver](int fd) {
            auto found = peers.find(fd); if (found == peers.end()) return;
            const auto id = found->second.id; ::close(fd); peers.erase(found); deliver(session.disconnect(id));
        };
        while (!state->stopping) {
            std::vector<pollfd> descriptors{{state->listener, POLLIN, 0}};
            for (const auto& [fd, peer] : peers) descriptors.push_back({fd, static_cast<short>(POLLIN | (peer.output.empty() ? 0 : POLLOUT)), 0});
            const int ready = ::poll(descriptors.data(), static_cast<nfds_t>(descriptors.size()), 100);
            if (ready < 0 && errno != EINTR) break;
            if (!descriptors.empty() && (descriptors[0].revents & POLLIN)) {
                for (;;) {
                    const int fd = accept(state->listener, nullptr, nullptr);
                    if (fd < 0) break;
                    if (peers.size() >= 32) { ::close(fd); continue; }
                    (void)fcntl(fd, F_SETFD, FD_CLOEXEC); const int current = fcntl(fd, F_GETFL, 0);
                    if (current < 0 || fcntl(fd, F_SETFL, current | O_NONBLOCK) < 0) { ::close(fd); continue; }
#if defined(SO_NOSIGPIPE)
                    int option = 1; (void)setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &option, sizeof(option));
#endif
                    peers.emplace(fd, Peer{fd, "peer-" + std::to_string(next++), {}, {}, false, std::chrono::steady_clock::now() + std::chrono::seconds(10)});
                }
            }
            for (std::size_t i = 1; i < descriptors.size(); ++i) {
                const int fd = descriptors[i].fd; auto found = peers.find(fd); if (found == peers.end()) continue;
                if (descriptors[i].revents & (POLLERR | POLLHUP | POLLNVAL)) { drop(fd); continue; }
                if (descriptors[i].revents & POLLIN) {
                    char buffer[8192]; const auto n = recv(fd, buffer, sizeof(buffer), 0);
                    if (n <= 0) { if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) drop(fd); continue; }
                    std::vector<std::string> messages; std::string parseError;
                    if (!found->second.decoder.append(std::string_view(buffer, static_cast<std::size_t>(n)), messages, parseError)) { drop(fd); continue; }
                    bool rejected = false;
                    for (const auto& message : messages) {
                        auto deliveries = session.receive(found->second.id, message);
                        if (!found->second.authenticated) {
                            if (deliveries.empty() || deliveries.front().json.find("\"type\":\"joined\"") == std::string::npos) { rejected = true; break; }
                            found->second.authenticated = true;
                        }
                        deliver(deliveries);
                    }
                    if (rejected) { drop(fd); continue; }
                }
                found = peers.find(fd); if (found == peers.end()) continue;
                if ((descriptors[i].revents & POLLOUT) && !found->second.output.empty()) {
#if defined(MSG_NOSIGNAL)
                    const auto n = send(fd, found->second.output.data(), found->second.output.size(), MSG_NOSIGNAL);
#else
                    const auto n = send(fd, found->second.output.data(), found->second.output.size(), 0);
#endif
                    if (n > 0) found->second.output.erase(0, static_cast<std::size_t>(n));
                    else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) { drop(fd); continue; }
                }
            }
            const auto now = std::chrono::steady_clock::now();
            for (auto it = peers.begin(); it != peers.end();) {
                const int fd = it->first; const bool expired = (!it->second.authenticated && now > it->second.deadline) || it->second.deadline == std::chrono::steady_clock::time_point::min(); ++it; if (expired) drop(fd);
            }
        }
        for (auto& [fd, peer] : peers) { (void)peer; ::close(fd); }
    });
    error.clear(); return true;
#endif
}
void HostServer::stop() {
    m_impl->stopping = true;
    if (m_impl->thread.joinable()) m_impl->thread.join();
#if !defined(_WIN32)
    if (m_impl->listener >= 0) { ::close(m_impl->listener); m_impl->listener = -1; }
#endif
    m_impl->port = 0;
}
}
