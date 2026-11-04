#include "editor/lsp/Client.hpp"
#include "editor/lsp/TextEdits.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
int main() {
    using namespace lucid::editor::lsp;
    if (fileUri(std::filesystem::path("/tmp/lucid test#file.cpp")).find("lucid%20test%23file.cpp") == std::string::npos) return 1;
    std::filesystem::path decoded;
    const auto sample = std::filesystem::absolute(std::filesystem::path("lucid test#file.cpp"));
    if (!pathFromFileUri(fileUri(sample), decoded) || decoded != sample || pathFromFileUri("file:///tmp/%00bad", decoded) || pathFromFileUri("https://example.com/x", decoded)) return 14;
#if !defined(_WIN32)
    {
        const auto fakeDir = std::filesystem::temp_directory_path() /
            ("lucid-lsp-capabilities-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(fakeDir);
        const std::string response = R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{"completionProvider":{},"definitionProvider":true,"declarationProvider":true,"referencesProvider":false,"documentFormattingProvider":{},"documentRangeFormattingProvider":false}}})";
        { std::ofstream script(fakeDir / "clangd"); script << "#!/bin/sh\nprintf 'Content-Length: " << response.size() << "\\r\\n\\r\\n%s' '" << response << "'\nexec /bin/cat > '" << (fakeDir / "received").string() << "'\n"; }
        std::filesystem::permissions(fakeDir / "clangd", std::filesystem::perms::owner_all);
        const char* old = std::getenv("PATH"); const bool hadPath = old != nullptr; const std::string oldPath = old ? old : "";
        if (setenv("PATH", fakeDir.c_str(), 1) != 0) return 30;
        Client capabilities; std::string error; std::vector<std::string> messages;
        const bool started = capabilities.start(fakeDir, error);
        if (hadPath) setenv("PATH", oldPath.c_str(), 1); else unsetenv("PATH");
        if (!started) return 31;
        const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!capabilities.ready() && std::chrono::steady_clock::now() < limit) {
            if (!capabilities.poll(messages, error)) return 32;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::uint64_t requestId = 123;
        if (!capabilities.ready() || !capabilities.supports("textDocument/completion") || !capabilities.supports("textDocument/definition") || !capabilities.supports("textDocument/declaration") ||
            !capabilities.supports("textDocument/formatting") || capabilities.supports("textDocument/references") ||
            capabilities.supports("textDocument/rangeFormatting") || capabilities.request("textDocument/references", "{}", requestId, error) ||
            requestId != 0 || error.find("does not support") == std::string::npos) return 33;
        if (!capabilities.request("textDocument/definition", "{}", requestId, error) || !capabilities.cancel(requestId, error)) return 35;
        bool receivedCancel = false;
        while (std::chrono::steady_clock::now() < limit) {
            messages.clear(); if (!capabilities.poll(messages, error)) return 37;
            std::ifstream recorded(fakeDir / "received"); const std::string data((std::istreambuf_iterator<char>(recorded)), {});
            if (data.find("\"method\":\"$/cancelRequest\"") != std::string::npos && data.find("\"params\":{\"id\":" + std::to_string(requestId) + "}") != std::string::npos) { receivedCancel = true; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!receivedCancel) return 36;
        capabilities.close(); std::filesystem::remove_all(fakeDir);
        if (capabilities.supports("textDocument/completion")) return 34;
    }
    {
        const auto fakeDir = std::filesystem::temp_directory_path() /
            ("lucid-lsp-timeout-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(fakeDir);
        { std::ofstream script(fakeDir / "clangd"); script << "#!/bin/sh\nexec /bin/sleep 5\n"; }
        std::filesystem::permissions(fakeDir / "clangd", std::filesystem::perms::owner_all);
        const char* existingPath = std::getenv("PATH");
        const std::string oldPath = existingPath ? existingPath : "";
        if (setenv("PATH", fakeDir.c_str(), 1) != 0) return 20;
        Client stalled(std::chrono::milliseconds(50)); std::string error; std::vector<std::string> messages;
        const bool started = stalled.start(fakeDir, error);
        if (existingPath) setenv("PATH", oldPath.c_str(), 1); else unsetenv("PATH");
        if (!started) return 21;
        bool timedOut = false;
        const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < limit) {
            if (!stalled.poll(messages, error)) { timedOut = true; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::filesystem::remove_all(fakeDir);
        if (!timedOut || stalled.alive() || error.find("did not finish starting") == std::string::npos) return 22;
    }
    {
        const auto fakeDir = std::filesystem::temp_directory_path() /
            ("lucid-lsp-encoding-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(fakeDir);
        const std::string response = R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{"positionEncoding":"utf-8"}}})";
        { std::ofstream script(fakeDir / "clangd");
          script << "#!/bin/sh\nprintf 'Content-Length: " << response.size() << "\\r\\n\\r\\n%s' '" << response << "'\nexec /bin/sleep 5\n"; }
        std::filesystem::permissions(fakeDir / "clangd", std::filesystem::perms::owner_all);
        const char* pathValue = std::getenv("PATH"); const bool hadPath = pathValue != nullptr;
        const std::string oldPath = pathValue ? pathValue : "";
        if (setenv("PATH", fakeDir.c_str(), 1) != 0) return 23;
        Client wrongEncoding; std::string error; std::vector<std::string> messages;
        const bool started = wrongEncoding.start(fakeDir, error);
        if (hadPath) setenv("PATH", oldPath.c_str(), 1); else unsetenv("PATH");
        if (!started) return 24;
        bool rejected = false;
        const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < limit) {
            if (!wrongEncoding.poll(messages, error)) { rejected = true; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        wrongEncoding.close(); std::filesystem::remove_all(fakeDir);
        if (!rejected || error.find("unsupported position encoding") == std::string::npos) {
            std::fprintf(stderr, "encoding test failed: rejected=%d error='%s' messages=%zu\n",
                         rejected ? 1 : 0, error.c_str(), messages.size());
            for (const auto& message : messages) std::fprintf(stderr, "  message: %s\n", message.c_str());
            return 25;
        }
    }
#endif
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
    if (client.supports("textDocument/declaration")) {
        if (!client.request("textDocument/declaration", params, id, error)) return 38;
        bool declarationReceived = false;
        while (!declarationReceived && std::chrono::steady_clock::now() < deadline) {
            messages.clear(); if (!client.poll(messages, error)) return 39;
            for (const auto& message : messages) {
                lucid::editor::JsonError jsonError; const auto value = lucid::editor::parseJson(message, jsonError);
                const auto* responseId = value ? value->find("id") : nullptr;
                if (responseId && responseId->type == lucid::editor::JsonType::Number && responseId->numberValue == static_cast<double>(id)) {
                    if (value->find("error") || !value->find("result")) return 40;
                    declarationReceived = true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!declarationReceived) return 41;
    }
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
    if (!formatted) return 19;
    const std::string rangeSource = "int first(){return 1;}\nint second(){return 2;}\n";
    if (!client.change(rangeSource, error)) return 26;
    const std::string rangeFormatting = "{\"textDocument\":{\"uri\":\"" + uri + "\"},\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":22}},\"options\":{\"tabSize\":4,\"insertSpaces\":true}}";
    if (!client.request("textDocument/rangeFormatting", rangeFormatting, id, error)) return 27;
    bool rangeFormatted = false;
    while (!rangeFormatted && std::chrono::steady_clock::now() < deadline) {
        messages.clear(); if (!client.poll(messages, error)) return 28;
        for (const auto& message : messages) {
            lucid::editor::JsonError parseError; const auto parsed = lucid::editor::parseJson(message, parseError);
            const auto* responseId = parsed ? parsed->find("id") : nullptr;
            if (!responseId || responseId->type != lucid::editor::JsonType::Number || responseId->numberValue != static_cast<double>(id)) continue;
            const auto* result = parsed->find("result"); std::string output;
            if (!result || !applyTextEdits(rangeSource, *result, output, error) || output == rangeSource ||
                output.find("int second(){return 2;}") == std::string::npos) return 29;
            rangeFormatted = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return rangeFormatted ? 0 : 30;
}
