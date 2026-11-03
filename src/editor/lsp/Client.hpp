#pragma once
#include "debug/AdapterProcess.hpp"
#include "debug/Protocol.hpp"
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::editor::lsp {
// URI conversion encodes UTF-8 bytes, including spaces and reserved characters.
[[nodiscard]] std::string fileUri(const std::filesystem::path& path);
[[nodiscard]] bool pathFromFileUri(std::string_view uri, std::filesystem::path& path);
class Client {
public:
    [[nodiscard]] bool start(const std::filesystem::path& root, std::string& error);
    [[nodiscard]] bool open(const std::filesystem::path& file, std::string_view language,
                            std::string_view text, std::string& error);
    [[nodiscard]] bool change(std::string_view text, std::string& error);
    [[nodiscard]] bool request(std::string_view method, std::string_view params,
                               std::uint64_t& id, std::string& error);
    [[nodiscard]] bool poll(std::vector<std::string>& messages, std::string& error);
    void close();
    [[nodiscard]] bool ready() const noexcept { return m_ready; }
    [[nodiscard]] bool alive() const noexcept { return m_process.alive(); }
    [[nodiscard]] int documentVersion() const noexcept { return m_version; }
private:
    [[nodiscard]] bool send(std::string_view body, std::string& error);
    debug::AdapterProcess m_process;
    debug::FrameDecoder m_decoder;
    std::string m_document;
    std::uint64_t m_nextId = 2;
    int m_version = 0;
    bool m_ready = false;
};
}
