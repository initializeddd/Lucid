#pragma once
#include "workspace/collaboration/SharedProject.hpp"
#include <map>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::workspace::collaboration {
struct Delivery { std::string peer; std::string json; };
// One HostSession is owned by one serialized event loop. Peer IDs are assigned
// by the transport, not supplied by the remote user.
class HostSession {
public:
    HostSession(Project project, std::string token) : m_project(std::move(project)), m_token(std::move(token)) {}
    [[nodiscard]] std::vector<Delivery> receive(std::string_view peer, std::string_view json);
    [[nodiscard]] std::vector<Delivery> disconnect(std::string_view peer);
private:
    struct Peer { std::string file; };
    [[nodiscard]] std::string snapshot(std::string_view path) const;
    [[nodiscard]] std::string presence(std::string_view path) const;
    void broadcast(std::string_view file, const std::string& json, std::vector<Delivery>& deliveries) const;
    SharedProject m_project;
    std::string m_token;
    std::map<std::string, Peer> m_peers;
};
}
