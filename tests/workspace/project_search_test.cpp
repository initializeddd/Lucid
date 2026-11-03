#include "workspace/search/ProjectSearch.hpp"
#include "workspace/recovery/History.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("lucid-project-search-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::string error; bool created = false;
    auto project = lucid::workspace::Project::openOrCreate(root, created, error);
    if (!error.empty() || !created) return 1;
    if (!project.writeFile("a.cpp", "hello target\r\ntarget end\r\n", error) ||
        !project.writeFile("src/b.cpp", "other target\n", error) ||
        !project.writeFile("binary.bin", std::string("\0target", 7), error)) return 2;
    project.refresh();
    auto results = lucid::workspace::searchProject(project, "target");
    if (results.matches.size() != 3 || results.files.size() != 2 || results.skipped != 1) return 3;
    if (results.matches[1].line != 1 || results.matches[1].column != 0) return 4;
    const auto limited = lucid::workspace::searchProject(project, "target", 1);
    std::size_t changed = 0;
    if (!limited.truncated || lucid::workspace::replaceProject(project, limited, "target", "x", changed, error)) return 5;
    if (!project.writeFile("src/b.cpp", "changed elsewhere\n", error)) return 6;
    if (lucid::workspace::replaceProject(project, results, "target", "x", changed, error) || changed != 0) return 7;
    std::string text;
    if (!project.readFile("a.cpp", text, error) || text != "hello target\r\ntarget end\r\n") return 8;
    if (!project.writeFile("src/b.cpp", "other target\n", error)) return 9;
    const auto unavailableHistory = root / "not-a-directory";
    { std::ofstream blocked(unavailableHistory); blocked << "x"; }
    if (lucid::workspace::replaceProject(project, results, "target", "name", changed, error, nullptr, unavailableHistory) || changed != 0 || error.empty()) return 14;
    if (!project.readFile("a.cpp", text, error) || text != "hello target\r\ntarget end\r\n") return 15;
    const auto historyRoot = root / ".lucid-safety";
    if (!lucid::workspace::replaceProject(project, results, "target", "name", changed, error, nullptr, historyRoot) || changed != 2) return 10;
    if (!project.readFile("a.cpp", text, error) || text != "hello name\r\nname end\r\n") return 11;
    if (!project.readFile("src/b.cpp", text, error) || text != "other name\n") return 12;
    std::vector<fs::path> backups;
    if (!lucid::workspace::recovery::listHistory(historyRoot, project.root().generic_string(), backups, error) || backups.size() != 2) return 13;
    fs::remove_all(root); return 0;
}
