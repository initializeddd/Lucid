#include "modules/PythonRuntime.hpp"
#include "editor/lsp/Client.hpp"
#include "editor/json/Json.hpp"
#include "debug/Protocol.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
int main() {
    namespace fs = std::filesystem; using namespace lucid::modules;
    const auto root = fs::temp_directory_path() / ("lucid-python-runtime-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    bool ok = !validatePythonRuntime(root / "missing").ok;
#ifndef _WIN32
    const auto script = root / "python";
    { std::ofstream f(script); f << "#!/bin/sh\nif [ \"$2\" = \"-c\" ]; then echo LUCID_PYTHON_VERSION=3.12.3; exit 0; fi\nif [ \"$2\" = \"-m\" ]; then mkdir -p \"$5/bin\"; cp \"$0\" \"$5/bin/python\"; echo version=3.12.3 > \"$5/pyvenv.cfg\"; exit 0; fi\nexit 1\n"; }
    fs::permissions(script, fs::perms::owner_all, fs::perm_options::add);
    auto valid = validatePythonRuntime(script);
    ok = ok && valid.ok && valid.version == "3.12.3";
    auto made = createAndValidatePythonRuntime(root, script, root / "venv");
    ok = ok && made.ok && made.version == "3.12.3" && !made.destinationMayRemain;
    auto exists = createAndValidatePythonRuntime(root, script, root / "venv");
    ok = ok && !exists.ok && !exists.destinationMayRemain;
    const auto bad = root / "bad";
    { std::ofstream f(bad); f << "#!/bin/sh\nif [ \"$2\" = \"-c\" ]; then echo LUCID_PYTHON_VERSION=3.12.3; exit 0; fi\nexit 1\n"; }
    fs::permissions(bad, fs::perms::owner_all, fs::perm_options::add);
    auto partial = createAndValidatePythonRuntime(root, bad, root / "partial");
    ok = ok && !partial.ok && partial.destinationMayRemain && fs::is_directory(root / "partial");
    const auto provider = root / "provider";
    const auto log = root / "calls";
    { std::ofstream f(provider); f << "#!/bin/sh\nprintf '%s|' \"$@\" >> '" << log.string() << "'\nprintf '\\n' >> '" << log.string() << "'\nexit 0\n"; }
    fs::permissions(provider, fs::perms::owner_all, fs::perm_options::add);
    std::atomic_bool cancelled{true};
    const auto stopped = provisionPythonLanguageServer(provider, true, &cancelled);
    ok = ok && !stopped.ok && stopped.cancelled && !stopped.mayHaveChanged && !fs::exists(log);
    const auto provisioned = provisionPythonLanguageServer(provider, true);
    std::ifstream calls(log); const std::string recorded((std::istreambuf_iterator<char>(calls)), {});
    ok = ok && provisioned.ok && provisioned.mayHaveChanged && provisioned.phase == "verify" &&
         recorded.find("-I|-m|ensurepip|--upgrade|") != std::string::npos &&
         recorded.find("-I|-m|pip|--isolated|install|") != std::string::npos &&
         recorded.find("|--no-user|") != std::string::npos &&
         recorded.find(std::string(pythonLanguageServerRequirement())) != std::string::npos &&
         recorded.find("-I|-c|import pylsp.__main__|") != std::string::npos;
    const auto noPip = provisionPythonLanguageServer(bad, false);
    ok = ok && !noPip.ok && !noPip.cancelled && !noPip.mayHaveChanged && noPip.phase == "check pip" && noPip.error.find("bootstrap") != std::string::npos;
    const auto badInstall = root / "bad-install";
    { std::ofstream f(badInstall); f << "#!/bin/sh\nif [ \"$4\" = \"--version\" ]; then exit 0; fi\nexit 1\n"; }
    fs::permissions(badInstall, fs::perms::owner_all, fs::perm_options::add);
    const auto failed = provisionPythonLanguageServer(badInstall, false);
    ok = ok && !failed.ok && !failed.cancelled && failed.mayHaveChanged && failed.phase == "install" && !failed.error.empty();
#endif
    // Opt-in integration with an explicitly selected real interpreter.
    if (const char* selected = std::getenv("LUCID_TEST_PYTHON"); selected && *selected) {
        auto actual = validatePythonRuntime(fs::path(selected));
        auto created = createAndValidatePythonRuntime(root, fs::path(selected), root / "real environment");
        ok = ok && actual.ok && created.ok && !created.version.empty() && actual.version == created.version &&
            fs::is_regular_file(root / "real environment" / "pyvenv.cfg");
#ifdef _WIN32
        ok = ok && !fs::exists(root / "real environment" / "Scripts" / "pip.exe");
#else
        ok = ok && !fs::exists(root / "real environment" / "bin" / "pip");
#endif
        if (created.ok) {
            auto again = validatePythonRuntime(created.interpreter);
            ok = ok && again.ok && again.version == created.version;
        }
    }
    // Opt-in network integration test: installs into a disposable environment.
    if (const char* selected = std::getenv("LUCID_TEST_PYTHON_PROVISION"); selected && *selected) {
        auto created = createAndValidatePythonRuntime(root, fs::path(selected), root / "server environment");
        if (!created.ok) ok = false;
        else {
            const auto installed = provisionPythonLanguageServer(created.interpreter, true);
            ok = ok && installed.ok && installed.phase == "verify";
            if (installed.ok) {
                lucid::editor::lsp::Client client; std::string error; std::vector<std::string> messages;
                const auto file = root / "lsp-test.py";
                const std::string source = "import os\nos.pa\nundefined_name\n";
                { std::ofstream output(file); output << source; }
                bool completed = false, diagnosed = false, formatted = false; std::string diagnosticLog, messageLog;
                if (!client.startPython(root, created.interpreter, error)) ok = false;
                else {
                    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
                    while (!client.ready() && std::chrono::steady_clock::now() < deadline) {
                        if (!client.poll(messages, error)) break;
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                    if (!client.ready() || !client.open(file, "python", source, error)) ok = false;
                    else {
                        std::uint64_t request = 0;
                        const auto uri = lucid::debug::jsonString(lucid::editor::lsp::fileUri(file));
                        const std::string params = "{\"textDocument\":{\"uri\":" + uri + "},\"position\":{\"line\":1,\"character\":5}}";
                        if (!client.request("textDocument/completion", params, request, error)) ok = false;
                        else while ((!completed || !diagnosed) && std::chrono::steady_clock::now() < deadline) {
                            messages.clear(); if (!client.poll(messages, error)) break;
                            for (const auto& message : messages) {
                                if (messageLog.size() < 8192) messageLog += message.substr(0, 8192 - messageLog.size()) + "\n";
                                lucid::editor::JsonError parseError; const auto json = lucid::editor::parseJson(message, parseError);
                                const auto* id = json ? json->find("id") : nullptr;
                                if (id && id->type == lucid::editor::JsonType::Number && id->numberValue == static_cast<double>(request) && !json->find("error") && message.find("\"path\"") != std::string::npos) completed = true;
                                const auto* method = json ? json->find("method") : nullptr;
                                if (method && method->type == lucid::editor::JsonType::String && method->stringValue == "textDocument/publishDiagnostics") {
                                    if (diagnosticLog.size() < 4096) diagnosticLog += message.substr(0, 4096 - diagnosticLog.size());
                                    if (message.find("undefined_name") != std::string::npos) diagnosed = true;
                                }
                            }
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        }
                        if (completed && diagnosed && client.change("x=1\n", error)) {
                            const std::string formatParams = "{\"textDocument\":{\"uri\":" + uri + "},\"options\":{\"tabSize\":4,\"insertSpaces\":true}}";
                            std::uint64_t formatRequest = 0;
                            if (client.request("textDocument/formatting", formatParams, formatRequest, error)) {
                                while (!formatted && std::chrono::steady_clock::now() < deadline) {
                                    messages.clear(); if (!client.poll(messages, error)) break;
                                    for (const auto& message : messages) {
                                        lucid::editor::JsonError parseError; const auto json = lucid::editor::parseJson(message, parseError);
                                        const auto* id = json ? json->find("id") : nullptr;
                                        if (id && id->type == lucid::editor::JsonType::Number && id->numberValue == static_cast<double>(formatRequest) && !json->find("error") && message.find("x = 1") != std::string::npos) formatted = true;
                                    }
                                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                                }
                            }
                        }
                    }
                    client.close();
                }
                if (!completed || !diagnosed || !formatted) { std::fprintf(stderr, "Python LSP integration: completion=%d diagnostics=%d formatting=%d error=%s diagnostic=%s messages=%s\n", completed, diagnosed, formatted, error.c_str(), diagnosticLog.c_str(), messageLog.c_str()); ok = false; }
            }
        }
    }
    fs::remove_all(root); return ok ? 0 : 1;
}
