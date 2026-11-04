#include "modules/Modules.hpp"
#include "modules/Registry.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
using namespace lucid::modules;
int main() {
    const auto root = std::filesystem::temp_directory_path() / ("lucid-modules-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root / "example");
    auto manifest = [&](bool network) {
        std::ofstream f(root / "example/module.json", std::ios::trunc);
        f << "{\"name\":\"Example\",\"version\":\"1\",\"permissions\":{\"project\":false,\"network\":" << (network ? "true" : "false") << ",\"processes\":false,\"settings\":false},\"commands\":[{\"title\":\"Hello\",\"insert\":\"hello\\n\"}]}";
    };
    std::string error; std::vector<Module> modules;
    manifest(false);
    if (!list(root, modules, error) || modules.size() != 1 || !modules[0].valid || modules[0].enabled || modules[0].commands[0].insert != "hello\n") return 1;
    if (!setEnabled(root, "example", true, error) || !list(root, modules, error) || !modules[0].enabled) return 2;
    manifest(true);
    if (!list(root, modules, error) || modules[0].enabled || !modules[0].network || setEnabled(root, "example", true, error)) return 3;
    if (!setEnabled(root, "example", false, error)) return 4;
    if (setEnabled(root, "../escape", true, error) || remove(root, "../escape", error)) return 5;
    { std::ofstream f(root / "example/module.json", std::ios::trunc); f << "{}"; }
    if (!list(root, modules, error) || modules[0].valid || setEnabled(root, "example", true, error)) return 6;
    if (!remove(root, "example", error) || std::filesystem::exists(root / "example")) return 7;
    const std::string candidate = R"({"name":"Starter","version":"1.0.0","permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Hello","insert":"hello"}]})";
    const auto candidatePreview = preview("starter", candidate);
    if (!candidatePreview.valid || candidatePreview.enabled || candidatePreview.commands.size() != 1) return 8;
    if (!install(root, "starter", candidate, error) || !list(root, modules, error) || modules.size() != 1 || modules[0].enabled) return 9;
    if (install(root, "starter", candidate, error) || error.empty()) return 10;
    if (install(root, "../escape", candidate, error) || install(root, "bad", "{}", error)) return 11;
    const auto unsupported = R"({"name":"Network","version":"1","permissions":{"project":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"x","insert":"x"}]})";
    if (install(root, "network", unsupported, error) || std::filesystem::exists(root / "network")) return 12;
    const auto binaryCommand = R"({"name":"Unsafe","version":"1","permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Insert","insert":"a\u0000b"}]})";
    if (preview("unsafe", binaryCommand).valid || install(root, "unsafe", binaryCommand, error) ||
        error.empty() || std::filesystem::exists(root / "unsafe")) return 19;
    std::vector<RegistryEntry> entries;
    if (!parseCatalog(R"({"modules":[{"id":"starter","package":"author/lucid-starter","version":"1.0.0","name":"Starter","description":"Handy snippets"}]})", entries, error) || entries.size() != 1 || entries[0].name != "Starter" || entries[0].description != "Handy snippets") return 13;
    if (parseCatalog(R"({"modules":[{"id":"starter","package":"author/../escape","version":"1"}]})", entries, error) || !entries.empty()) return 14;
    if (parseCatalog(R"({"modules":[{"id":"a","package":"author/a","version":"1"},{"id":"a","package":"author/b","version":"1"}]})", entries, error) || !entries.empty()) return 15;
    std::string unused;
    if (fetchPackage("author/../bad", "1", "application/vnd.lucid.module.v1+json", unused, error)) return 16;
    const std::string digest = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    int requests = 0;
    const RegistryRequest transport = [&](std::string_view url, const std::vector<std::string>& headers, std::string& output, std::string&) {
        ++requests;
        if (url == "https://ghcr.io/token?scope=repository:author/starter:pull" && headers.empty()) output = R"({"token":"temporary-token"})";
        else if (url == "https://ghcr.io/v2/author/starter/manifests/1.0.0" && headers.size() == 2) output = "{\"layers\":[{\"mediaType\":\"application/vnd.lucid.module.v1+json\",\"digest\":\"sha256:" + digest + "\"}]}";
        else if (url == "https://ghcr.io/v2/author/starter/blobs/sha256:" + digest && headers.size() == 2) output = "abc";
        else return false;
        return true;
    };
    if (!fetchPackage("author/starter", "1.0.0", "application/vnd.lucid.module.v1+json", unused, error, transport) || unused != "abc" || requests != 3) return 17;
    const RegistryRequest corrupt = [&](std::string_view url, const std::vector<std::string>& headers, std::string& output, std::string& reason) {
        if (!transport(url, headers, output, reason)) return false;
        if (url.find("/blobs/") != std::string_view::npos) output = "tampered";
        return true;
    };
    if (fetchPackage("author/starter", "1.0.0", "application/vnd.lucid.module.v1+json", unused, error, corrupt) || !unused.empty()) return 18;
    std::filesystem::remove_all(root); return 0;
}
