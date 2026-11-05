#include "modules/Modules.hpp"
#include "modules/Registry.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 4) return 1;
    const auto storage = std::filesystem::path(argv[3]);
    std::error_code storageError;
    if (!std::filesystem::is_directory(storage, storageError) || storageError ||
        !std::filesystem::is_empty(storage, storageError) || storageError) return 6;
    std::ifstream stream(std::filesystem::path(argv[2]), std::ios::binary);
    if (!stream) return 2;
    const std::string catalog((std::istreambuf_iterator<char>(stream)), {});
    std::vector<lucid::modules::RegistryEntry> entries;
    std::vector<lucid::modules::Module> modules;
    std::string error;
    if (!lucid::modules::parseCatalog(catalog, entries, error) ||
        !lucid::modules::list(std::filesystem::path(argv[1]), modules, error)) {
        std::cerr << error << '\n'; return 3;
    }
    if (entries.size() != modules.size()) { std::cerr << "Catalog module count mismatch\n"; return 4; }
    for (const auto& module : modules) {
        const auto it = std::find_if(entries.begin(), entries.end(), [&](const auto& e) { return e.id == module.id; });
        bool supported = module.error.empty();
#if !defined(__APPLE__)
        if (module.id == "lucid-web-component-preview" && module.error == "Requires unsupported capability: web-component-preview-macos-v1") supported = true;
#endif
        if (!module.valid || !supported || it == entries.end() || it->version != module.version ||
            it->name != module.name || it->description != module.description || it->requirements != module.requirements) {
            std::cerr << "Catalog differs from installed manifest: " << module.id << '\n'; return 5;
        }
        std::ifstream manifest(std::filesystem::path(argv[1]) / module.id / "module.json", std::ios::binary);
        if (!manifest) return 7;
        const std::string body((std::istreambuf_iterator<char>(manifest)), {});
        if (!module.error.empty()) {
            if (lucid::modules::install(storage, module.id, body, error)) return 8;
            continue;
        }
        if (!lucid::modules::install(storage, module.id, body, error)) { std::cerr << module.id << ": " << error << '\n'; return 9; }
        std::vector<lucid::modules::Module> installed;
        if (!lucid::modules::list(storage, installed, error) || installed.size() != 1 || installed[0].enabled ||
            !lucid::modules::setEnabled(storage, module.id, true, error) ||
            !lucid::modules::list(storage, installed, error) || installed.size() != 1 || !installed[0].enabled ||
            !lucid::modules::setEnabled(storage, module.id, false, error) ||
            !lucid::modules::remove(storage, module.id, error)) { std::cerr << module.id << ": " << error << '\n'; return 10; }
    }
    return 0;
}
