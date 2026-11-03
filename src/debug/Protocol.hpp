#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::debug {
// DAP messages are UTF-8 JSON preceded by an ASCII Content-Length header.
[[nodiscard]] std::string frame(std::string_view json);
[[nodiscard]] std::string jsonString(std::string_view text);
class FrameDecoder {
public:
    [[nodiscard]] bool append(std::string_view bytes, std::vector<std::string>& messages, std::string& error);
    void reset() { m_buffer.clear(); }
    [[nodiscard]] bool empty() const noexcept { return m_buffer.empty(); }
private:
    std::string m_buffer;
};
}
