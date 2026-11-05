#pragma once
#include "debug/AdapterProcess.hpp"
#include "debug/Protocol.hpp"
#include <cstdint>
#include <chrono>
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
    explicit Client(std::chrono::milliseconds initializationTimeout = std::chrono::seconds(15))
        : m_initializationTimeout(initializationTimeout) {}
    [[nodiscard]] bool start(const std::filesystem::path& root, std::string& error);
    // Starts the selected interpreter with `-I -m pylsp`. This executes the
    // interpreter and its installed packages; call only on explicit user intent.
    // The environment must already contain python-lsp-server.
    [[nodiscard]] bool startPython(const std::filesystem::path& root,
                                   const std::filesystem::path& interpreter, std::string& error);
    [[nodiscard]] bool open(const std::filesystem::path& file, std::string_view language,
                            std::string_view text, std::string& error);
    [[nodiscard]] bool change(std::string_view text, std::string& error);
    [[nodiscard]] bool request(std::string_view method, std::string_view params,
                               std::uint64_t& id, std::string& error);
    [[nodiscard]] bool cancel(std::uint64_t id, std::string& error);
    [[nodiscard]] bool poll(std::vector<std::string>& messages, std::string& error);
    void close();
    [[nodiscard]] bool ready() const noexcept { return m_ready; }
    [[nodiscard]] bool supports(std::string_view method) const noexcept;
    [[nodiscard]] bool alive() const noexcept { return m_process.alive(); }
    [[nodiscard]] int documentVersion() const noexcept { return m_version; }
private:
    [[nodiscard]] bool startServer(const std::filesystem::path& root, const std::string& executable,
                                   const std::vector<std::string>& arguments, std::string_view name,
                                   std::string& error);
    [[nodiscard]] bool send(std::string_view body, std::string& error);
    debug::AdapterProcess m_process;
    debug::FrameDecoder m_decoder;
    std::string m_document;
    std::string m_serverName = "clangd";
    std::uint64_t m_nextId = 2;
    int m_version = 0;
    bool m_ready = false;
    bool m_completion = false, m_signature = false, m_workspaceSymbols = false, m_documentSymbols = false, m_definition = false, m_declaration = false, m_references = false, m_formatting = false, m_rangeFormatting = false, m_rename = false, m_codeAction = false;
    std::chrono::milliseconds m_initializationTimeout;
    std::chrono::steady_clock::time_point m_initializationDeadline{};
};
}
