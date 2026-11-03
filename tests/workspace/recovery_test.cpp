#include "workspace/recovery/Snapshot.hpp"
#include "workspace/recovery/History.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
using namespace lucid::workspace::recovery;
int main() {
    const auto dir = std::filesystem::temp_directory_path() / ("lucid-recovery-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto path = dir / "one.json";
    Snapshot original; original.project = "/project"; original.file = "src/a.cpp"; original.baseline = "hello\n"; original.contents = "new \"code\"\n🙂"; original.line = 2; original.column = 3; original.firstLine = 1;
    original.openFiles.push_back({"src/b.cpp", 5, 4, 2});
    std::string error; Snapshot restored;
    if (!write(path, original, error) || !read(path, restored, error) || !(restored == original)) return 1;
    original.contents = "second";
    if (!write(path, original, error) || !read(path, restored, error) || !(restored == original)) return 2;
    { std::ofstream f(path, std::ios::trunc); f << "{bad"; }
    if (read(path, restored, error) || error.empty()) return 3;
    for (int i = 0; i < 4; ++i) {
        original.contents = original.baseline = "version " + std::to_string(i);
        if (!recordHistory(dir, original, error)) return 4;
    }
    std::vector<std::filesystem::path> history;
    if (!listHistory(dir, original.project, history, error) || history.size() != 3) return 5;
    bool newest = false;
    for (const auto& version : history) {
        if (!read(version, restored, error) || restored.project != original.project) return 6;
        if (restored.contents == "version 3") newest = true;
    }
    if (!newest) return 8;
    std::vector<std::filesystem::path> other;
    if (!listHistory(dir, "/another-project", other, error) || !other.empty()) return 7;
    std::filesystem::remove_all(dir); return 0;
}
