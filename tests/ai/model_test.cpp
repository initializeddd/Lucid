#include "ai/LocalModel.hpp"
#include "ai/CodeCleanup.hpp"
#include "ai/OpenRouter.hpp"
#include "ai/ModelDownload.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

int main() {
    std::string output, error;
    if (lucid::ai::generate(std::filesystem::path("missing-lucid-model.gguf"), "test", output, error) || error.empty()) return 1;
    std::filesystem::path path;
    if (lucid::ai::ensureModel({}, path, error) || error.empty()) return 2;
    std::atomic_bool cancelled{true};
    std::atomic<int> progress{0};
    if (lucid::ai::ensureModel(std::filesystem::temp_directory_path(), path, error, &cancelled, &progress) ||
        error != "Model download cancelled") return 17;
    if (lucid::ai::generate("unused.gguf", "test", output, error, &cancelled) ||
        error != "AI request cancelled") return 3;
    if (const char* model = std::getenv("LUCID_TEST_MODEL"); model && *model) {
        const auto modelPath = std::filesystem::path(model);
        std::filesystem::path verified;
        if (!lucid::ai::ensureModel(modelPath.parent_path(), verified, error) || verified != modelPath) return 4;
        std::atomic_bool stopVerification{false}; std::atomic<int> verificationProgress{0};
        std::string verificationError; std::filesystem::path verificationPath; bool verificationResult = true;
        std::thread verification([&] { verificationResult = lucid::ai::ensureModel(modelPath.parent_path(), verificationPath, verificationError, &stopVerification, &verificationProgress); });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (verificationProgress.load() != lucid::ai::kModelVerifying && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
        stopVerification = true; verification.join();
        if (verificationResult || verificationError != "Model download cancelled") return 18;
        if (!lucid::ai::generate(verified, "Write a Python function that adds two numbers.", output, error) ||
            output.empty()) return 5;
    }
    auto doc = lucid::editor::Document::fromUtf8("style_before();\nold_code();\nstyle_after();");
    std::string request;
    const auto context=lucid::ai::diagnosticContext(doc,1,0,"Expected semicolon");
    if(context.find("Expected semicolon")==std::string::npos || context.find("1 | style_before();")==std::string::npos || context.find("2 > old_code();")==std::string::npos || context.find("3 | style_after();")==std::string::npos) return 19;
    const auto unicodeDoc=lucid::editor::Document::fromUtf8(std::string(300,'a')+"β"+std::string(5000,'b'));
    const auto bounded=lucid::ai::diagnosticContext(unicodeDoc,0,300,std::string(2047,'x')+"β");
    if(bounded.size()>4000 || bounded.find("β")==std::string::npos || bounded.find("[truncated]")==std::string::npos || bounded.find("...")==std::string::npos) return 20;
    if(lucid::ai::diagnosticContext(doc,99,0,"oops").find("Nearby source")!=std::string::npos) return 21;
    if (lucid::ai::cleanupRequest(doc, "sample.cpp", request, error)) return 6;
    doc.setCursor(1, 0); doc.extendSelectionTo(1, 11);
    if (!lucid::ai::cleanupRequest(doc, "sample.cpp", request, error) ||
        request.find("style_before();") == std::string::npos ||
        request.find("old_code();") == std::string::npos ||
        request.find("style_after();") == std::string::npos) return 7;
    if (const char* model = std::getenv("LUCID_TEST_MODEL"); model && *model) {
        if (!lucid::ai::generate(model, request, output, error) ||
            !lucid::ai::cleanupResponse(output, error) || output.empty()) return 11;
        if (!lucid::ai::generate(model, "Explain in at most three sentences: paused in main at line 3, n = 7. State uncertainty.", output, error) || output.empty()) return 16;
    }
    std::string fenced = "```cpp\nclean();\n```\n";
    if (!lucid::ai::cleanupResponse(fenced, error) || fenced != "clean();\n") return 8;
    std::string incomplete = "```cpp\npartial";
    if (lucid::ai::cleanupResponse(incomplete, error) || error.empty()) return 9;
    std::string empty = "  ";
    if (lucid::ai::cleanupResponse(empty, error)) return 10;
    const auto payload = lucid::ai::openRouterRequest("vendor/model", "quoted \"state\"");
    if (payload.find("vendor/model") == std::string::npos || payload.find("\\\"state\\\"") == std::string::npos ||
        payload.find("Follow the user's requested task") == std::string::npos || payload.find("@end") == std::string::npos ||
        payload.find(":freebsd") == std::string::npos || payload.find("not currently executable on Windows") == std::string::npos ||
        payload.find("Explain debugging state concisely") != std::string::npos) return 12;
    if (!lucid::ai::parseOpenRouterResponse(R"({"choices":[{"finish_reason":"stop","message":{"content":"explanation"}}]})", output, error) || output != "explanation") return 13;
    if (lucid::ai::parseOpenRouterResponse(R"({"choices":[{"finish_reason":"length","message":{"content":"partial"}}]})", output, error)) return 14;
    if (lucid::ai::generateOpenRouter("bad\nkey", "model", "prompt", output, error) || error.empty()) return 15;
    return 0;
}
