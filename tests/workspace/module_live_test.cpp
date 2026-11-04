#include "modules/Modules.hpp"
#include "modules/Registry.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>

int main() {
    using namespace lucid::modules;
    std::string catalog, manifest, error;
    std::vector<RegistryEntry> entries;
    if (!fetchPackage("initializeddd/lucid-module-catalog", "latest", "application/vnd.lucid.catalog.v1+json", catalog, error) ||
        !parseCatalog(catalog, entries, error)) { std::cerr << error << '\n'; return 1; }
    const auto entry = std::find_if(entries.begin(), entries.end(), [](const auto& e) { return e.id == "lucid-cpp-starter"; });
    if (entry == entries.end()) { std::cerr << "Starter missing from catalog\n"; return 2; }
    if (!fetchPackage(entry->package, entry->version, "application/vnd.lucid.module.v1+json", manifest, error)) { std::cerr << error << '\n'; return 3; }
    const auto candidate = preview(entry->id, manifest);
    if (!candidate.valid || !candidate.error.empty() || candidate.commands.empty()) { std::cerr << candidate.error << '\n'; return 4; }
    const auto root = std::filesystem::temp_directory_path() / ("lucid-live-module-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::vector<Module> installed;
    const bool ok = install(root, entry->id, manifest, error) && list(root, installed, error) &&
        installed.size() == 1 && !installed[0].enabled && installed[0].commands.size() == candidate.commands.size();
    std::error_code ignored; std::filesystem::remove_all(root, ignored);
    if (!ok) { std::cerr << error << '\n'; return 5; }
    std::cout << "Downloaded, previewed, and installed " << candidate.name << " " << candidate.version << " disabled\n";
    return 0;
}
