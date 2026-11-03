#include "editor/lsp/Client.hpp"
#include "editor/lsp/TextEdits.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>
int main() {
    using namespace lucid::editor::lsp;
    if (fileUri(std::filesystem::path("/tmp/lucid test#file.cpp")).find("lucid%20test%23file.cpp") == std::string::npos) return 1;
    std::filesystem::path decoded;
    const auto sample = std::filesystem::absolute(std::filesystem::path("lucid test#file.cpp"));
    if (!pathFromFileUri(fileUri(sample), decoded) || decoded != sample || pathFromFileUri("file:///tmp/%00bad", decoded) || pathFromFileUri("https://example.com/x", decoded)) return 14;
    if (!std::getenv("LUCID_TEST_CLANGD")) return 0;
    Client client; std::string error; std::vector<std::string> messages;
    const auto root = std::filesystem::temp_directory_path();
    if (!client.start(root, error)) return 2;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (!client.ready() && std::chrono::steady_clock::now() < deadline) {
        if (!client.poll(messages, error)) return 3;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!client.ready()) return 4;
    const auto path = root / "lucid-lsp-test.cpp";
    if (!client.open(path, "cpp", "int main() { return 0; }\n", error)) return 5;
    if (!client.change("int main() { return missing_name; }\n", error)) return 6;
    std::uint64_t id = 0;
    const auto uri = fileUri(path);
    const std::string params = "{\"textDocument\":{\"uri\":\"" + uri + "\"},\"position\":{\"line\":0,\"character\":5}}";
    if (!client.request("textDocument/definition", params, id, error)) return 7;
    bool response = false, diagnostic = false;
    while ((!response || !diagnostic) && std::chrono::steady_clock::now() < deadline) {
        messages.clear(); if (!client.poll(messages, error)) return 8;
        for (const auto& message : messages) {
            if (message.find("\"id\":" + std::to_string(id)) != std::string::npos || message.find("\"id\": " + std::to_string(id)) != std::string::npos) response = true;
            if (message.find("textDocument/publishDiagnostics") != std::string::npos && message.find("missing_name") != std::string::npos) {
                lucid::editor::JsonError parseError; const auto parsed = lucid::editor::parseJson(message, parseError);
                const auto* params = parsed ? parsed->find("params") : nullptr;
                const auto* version = params ? params->find("version") : nullptr;
                diagnostic = version && version->type == lucid::editor::JsonType::Number && version->numberValue == static_cast<double>(client.documentVersion());
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!response || !diagnostic) return 9;
    if (!client.change("int lucidExample();\nint main() { lucidEx }\n", error)) return 10;
    const std::string completion = "{\"textDocument\":{\"uri\":\"" + uri + "\"},\"position\":{\"line\":1,\"character\":20}}";
    if (!client.request("textDocument/completion", completion, id, error)) return 11;
    bool found = false;
    while (!found && std::chrono::steady_clock::now() < deadline) {
        messages.clear(); if (!client.poll(messages, error)) return 12;
        for (const auto& message : messages) if (message.find("lucidExample") != std::string::npos) found = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!found) return 13;
    const std::string source = "int main(){return 0;}\n";
    if (!client.change(source, error)) return 15;
    const std::string formatting = "{\"textDocument\":{\"uri\":\"" + uri + "\"},\"options\":{\"tabSize\":4,\"insertSpaces\":true}}";
    if (!client.request("textDocument/formatting", formatting, id, error)) return 16;
    bool formatted = false;
    while (!formatted && std::chrono::steady_clock::now() < deadline) {
        messages.clear(); if (!client.poll(messages, error)) return 17;
        for (const auto& message : messages) {
            lucid::editor::JsonError parseError; const auto parsed = lucid::editor::parseJson(message, parseError);
            const auto* responseId = parsed ? parsed->find("id") : nullptr;
            if (!responseId || responseId->type != lucid::editor::JsonType::Number || responseId->numberValue != static_cast<double>(id)) continue;
            const auto* result = parsed->find("result"); std::string output;
            if (!result || !applyTextEdits(source, *result, output, error) || output == source || output.find("return 0;") == std::string::npos) return 18;
            formatted = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return formatted ? 0 : 19;
}
