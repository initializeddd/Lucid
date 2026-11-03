#include "debug/Protocol.hpp"
#include <algorithm>
#include <charconv>
#include <cctype>
namespace lucid::debug {
namespace { constexpr std::size_t kMaxPayload = 8U * 1024U * 1024U; constexpr std::size_t kMaxHeader = 8192; }
std::string frame(std::string_view json) { return "Content-Length: " + std::to_string(json.size()) + "\r\n\r\n" + std::string(json); }
std::string jsonString(std::string_view text) {
    constexpr char hex[] = "0123456789abcdef"; std::string out = "\"";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
        else if (c < 32) { out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
        else out += static_cast<char>(c);
    }
    return out + '"';
}
bool FrameDecoder::append(std::string_view bytes, std::vector<std::string>& messages, std::string& error) {
    error.clear();
    if (bytes.size() > kMaxPayload + kMaxHeader || m_buffer.size() > kMaxPayload + kMaxHeader - bytes.size()) { error = "Debug adapter message exceeds limit"; return false; }
    m_buffer.append(bytes);
    for (;;) {
        const auto headerEnd = m_buffer.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            if (m_buffer.size() > kMaxHeader) { error = "Debug adapter header exceeds limit"; return false; }
            return true;
        }
        if (headerEnd > kMaxHeader) { error = "Debug adapter header exceeds limit"; return false; }
        std::size_t length = 0; bool found = false; std::size_t offset = 0;
        while (offset < headerEnd) {
            const auto end = std::min(m_buffer.find("\r\n", offset), headerEnd);
            const std::string_view line(m_buffer.data() + offset, end - offset);
            const auto colon = line.find(':');
            if (colon == std::string_view::npos) { error = "Invalid debug adapter header"; return false; }
            std::string name(line.substr(0, colon));
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (name == "content-length") {
                if (found) { error = "Duplicate debug adapter length"; return false; }
                auto value = line.substr(colon + 1);
                while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
                while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), length);
                if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || length > kMaxPayload) { error = "Invalid debug adapter length"; return false; }
                found = true;
            }
            offset = end + 2;
        }
        if (!found) { error = "Missing debug adapter length"; return false; }
        const auto bodyStart = headerEnd + 4;
        if (m_buffer.size() - bodyStart < length) return true;
        messages.emplace_back(m_buffer.substr(bodyStart, length));
        m_buffer.erase(0, bodyStart + length);
    }
}
}
