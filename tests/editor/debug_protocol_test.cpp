#include "debug/Protocol.hpp"
#include <string>
#include <vector>
int main() {
    using namespace lucid::debug;
    FrameDecoder d; std::vector<std::string> messages; std::string error;
    const auto bytes = frame("{\"text\":\"🙂\"}") + frame("{}");
    for (char c : bytes) if (!d.append(std::string_view(&c, 1), messages, error)) return 1;
    if (messages.size() != 2 || messages[0] != "{\"text\":\"🙂\"}" || messages[1] != "{}" || !d.empty()) return 2;
    d.reset(); messages.clear();
    if (!d.append("content-length: 2\r\nContent-Type: application/vscode-jsonrpc\r\n\r\n{}", messages, error) || messages.size() != 1) return 3;
    d.reset(); if (d.append("Content-Length: 2\r\nContent-Length: 2\r\n\r\n{}", messages, error) || error.empty()) return 4;
    d.reset(); if (d.append("Content-Length: -1\r\n\r\n", messages, error)) return 5;
    d.reset(); if (d.append("Content-Length: 999999999\r\n\r\n", messages, error)) return 6;
    if (jsonString("a\n\"\\") != "\"a\\u000a\\\"\\\\\"") return 7;
    return 0;
}
