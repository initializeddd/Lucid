#pragma once
#include "workspace/Project.hpp"
#include <cstdint>
#include <memory>
#include <string>
namespace lucid::workspace::collaboration {
// Transport is deliberately restricted to loopback or a 100.64.0.0/10
// private-overlay address. It provides no encryption: use a trusted VPN.
class HostServer {
public:
    HostServer();
    ~HostServer();
    HostServer(const HostServer&) = delete;
    HostServer& operator=(const HostServer&) = delete;
    [[nodiscard]] bool start(Project project, const std::string& bindAddress, std::uint16_t port,
                             const std::string& token, std::string& error);
    [[nodiscard]] std::uint16_t port() const noexcept;
    void stop();
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
