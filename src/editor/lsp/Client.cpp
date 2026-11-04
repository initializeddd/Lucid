#include "editor/lsp/Client.hpp"
#include "editor/json/Json.hpp"
#include <cmath>

namespace lucid::editor::lsp {
std::string fileUri(const std::filesystem::path& path) {
    const auto raw = std::filesystem::absolute(path).generic_string();
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string uri = "file://";
    if (raw.empty() || raw.front() != '/') uri += '/';
    for (unsigned char c : raw) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '/' || c == ':' || c == '-' || c == '_' || c == '.' || c == '~') uri += static_cast<char>(c);
        else { uri += '%'; uri += hex[c >> 4]; uri += hex[c & 15]; }
    }
    return uri;
}
bool pathFromFileUri(std::string_view uri, std::filesystem::path& path) {
    if (!uri.starts_with("file://")) return false;
    uri.remove_prefix(7);
    if (!uri.starts_with('/')) return false;
    auto hex = [](char c) -> int { if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return c - 'a' + 10; if (c >= 'A' && c <= 'F') return c - 'A' + 10; return -1; };
    std::string decoded;
    for (std::size_t i = 0; i < uri.size(); ++i) {
        if (uri[i] == '%') {
            if (i + 2 >= uri.size()) return false;
            const int a = hex(uri[i + 1]), b = hex(uri[i + 2]); if (a < 0 || b < 0 || (a == 0 && b == 0)) return false;
            decoded.push_back(static_cast<char>((a << 4) | b)); i += 2;
        } else { if (uri[i] == '?' || uri[i] == '#' || uri[i] == '\0') return false; decoded.push_back(uri[i]); }
    }
#if defined(_WIN32)
    if (decoded.size() >= 3 && decoded[0] == '/' && decoded[2] == ':') decoded.erase(0, 1);
#endif
    path = std::filesystem::u8path(decoded); return path.is_absolute();
}
bool Client::send(std::string_view body, std::string& error) { return m_process.send(debug::frame(body), error); }
bool Client::start(const std::filesystem::path& root, std::string& error) {
    close();
    if (!m_process.start("clangd", {"--background-index"}, error)) { error = "Could not start clangd: " + error; return false; }
    const auto uri = debug::jsonString(fileUri(root));
    if (!send("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"processId\":null,\"rootUri\":" + uri + ",\"capabilities\":{\"general\":{\"positionEncodings\":[\"utf-16\"]},\"textDocument\":{\"publishDiagnostics\":{\"versionSupport\":true}}}}}", error)) { close(); return false; }
    m_initializationDeadline = std::chrono::steady_clock::now() + m_initializationTimeout;
    return true;
}
bool Client::open(const std::filesystem::path& file, std::string_view language, std::string_view text, std::string& error) {
    if (!m_ready) { error = "Language server is not initialized"; return false; }
    if (!m_document.empty() && !send("{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didClose\",\"params\":{\"textDocument\":{\"uri\":" + debug::jsonString(m_document) + "}}}", error)) return false;
    const auto uri = fileUri(file);
    const int version = m_version + 1;
    if (!send("{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":" + debug::jsonString(uri) + ",\"languageId\":" + debug::jsonString(language) + ",\"version\":" + std::to_string(version) + ",\"text\":" + debug::jsonString(text) + "}}}", error)) return false;
    m_document = uri; m_version = version; return true;
}
bool Client::change(std::string_view text, std::string& error) {
    if (m_document.empty()) { error = "No document is open on the language server"; return false; }
    const int version = m_version + 1;
    if (!send("{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{\"textDocument\":{\"uri\":" + debug::jsonString(m_document) + ",\"version\":" + std::to_string(version) + "},\"contentChanges\":[{\"text\":" + debug::jsonString(text) + "}]}}", error)) return false;
    m_version = version; return true;
}
bool Client::request(std::string_view method, std::string_view params, std::uint64_t& id, std::string& error) {
    if (!m_ready) { error = "Language server is not initialized"; return false; }
    id = m_nextId++;
    return send("{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"method\":" + debug::jsonString(method) + ",\"params\":" + std::string(params) + "}", error);
}
bool Client::poll(std::vector<std::string>& messages, std::string& error) {
    std::string output; std::vector<std::string> incoming;
    if (!m_process.poll(output, error) || !m_decoder.append(output, incoming, error)) return false;
    for (auto& body : incoming) {
        JsonError parseError; const auto json = parseJson(body, parseError);
        if (!json || json->type != JsonType::Object) { error = "Invalid language server response"; return false; }
        const auto* id = json->find("id"); const auto* method = json->find("method");
        if (!m_ready && id && id->type == JsonType::Number && id->numberValue == 1 && !method) {
            if (json->find("error")) { error = "clangd initialization failed"; return false; }
            const auto* result = json->find("result");
            const auto* capabilities = result && result->type == JsonType::Object ? result->find("capabilities") : nullptr;
            if (!capabilities || capabilities->type != JsonType::Object) { error = "Invalid language server initialization response"; return false; }
            const auto* encoding = capabilities->find("positionEncoding");
            if (encoding && (encoding->type != JsonType::String || encoding->stringValue != "utf-16")) {
                error = "Language server uses an unsupported position encoding"; return false;
            }
            if (!send("{\"jsonrpc\":\"2.0\",\"method\":\"initialized\",\"params\":{}}", error)) return false;
            m_ready = true;
        } else if (method && id) {
            // Acknowledge unsupported server-to-client requests without hanging clangd.
            const std::string identifier = id->type == JsonType::String ? debug::jsonString(id->stringValue) : id->type == JsonType::Number ? std::to_string(static_cast<long long>(id->numberValue)) : "null";
            if (!send("{\"jsonrpc\":\"2.0\",\"id\":" + identifier + ",\"error\":{\"code\":-32601,\"message\":\"Method not supported\"}}", error)) return false;
        } else messages.push_back(std::move(body));
    }
    if (!m_ready && std::chrono::steady_clock::now() >= m_initializationDeadline) {
        error = "Language server did not finish starting; check clangd and try again";
        close(); return false;
    }
    return true;
}
void Client::close() { m_process.close(); m_decoder.reset(); m_document.clear(); m_ready = false; m_version = 0; m_nextId = 2; m_initializationDeadline = {}; }
}
