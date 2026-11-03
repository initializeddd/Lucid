#pragma once
#include "debug/Protocol.hpp"
#include <string>
#include <string_view>
#include <vector>
namespace lucid::workspace::collaboration {
class Connection {
public:
    Connection() = default;
    ~Connection();
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    // DNS and connection setup can block for several seconds; call off the UI thread.
    [[nodiscard]] bool connect(std::string_view link, std::string_view name, std::string& error);
    [[nodiscard]] bool send(std::string_view json, std::string& error);
    [[nodiscard]] bool poll(std::vector<std::string>& messages, std::string& error);
    void close() noexcept;
private:
    int m_fd = -1;
    std::string m_pending;
    debug::FrameDecoder m_decoder;
};
}
