#include "workspace/session/Session.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
int main() {
    namespace fs = std::filesystem;
    using namespace lucid::workspace::session;
    const auto root = fs::temp_directory_path() / ("lucid-session-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    State state; state.project = (root / "project").string(); state.recentProjects = {state.project, (root / "other project").string()};
    state.files = {{"src/main.cpp", 12, 8, 5}, {"src/ä.cpp", 2, 4, 0}};
    std::string error;
    if (!write(root / "Session", state, error)) return 1;
    State loaded;
    if (!read(root / "Session", loaded, error) || loaded.project != state.project || loaded.recentProjects != state.recentProjects || loaded.files != state.files) return 2;
    State merged; merged.project = state.project;
    merged.files = {{"src/main.cpp", 20, 3, 0}};
    if (!mergeUnrestored(merged, state.project, state.files) || merged.files.size() != 2 ||
        merged.files[0].line != 20 || merged.files[1] != state.files[1]) return 9;
    State other; other.project = "/other";
    if (!mergeUnrestored(other, state.project, state.files) || !other.files.empty()) return 10;
    State full; full.project = state.project;
    for (std::size_t i = 0; i < kMaxSessionFiles; ++i) full.files.push_back({"file" + std::to_string(i), 0, 0, 0});
    const auto before = full.files;
    if (mergeUnrestored(full, state.project, state.files) || full.files != before) return 11;
    if (!write(root / "LargeSession", full, error) || !read(root / "LargeSession", loaded, error) || loaded.files != full.files) return 12;
    full.files.push_back({"one-too-many", 0, 0, 0});
    if (write(root / "LargeSession", full, error)) return 13;
    State invalid = state; invalid.files[0].path = "../outside";
    if (write(root / "Session", invalid, error) || error.empty()) return 3;
    if (!read(root / "Session", loaded, error) || loaded.files != state.files) return 4;
    invalid = state; invalid.files[0].firstLine = 100000001;
    if (write(root / "Session", invalid, error) || error.empty() || !read(root / "Session", loaded, error) || loaded.files != state.files) return 14;
    invalid = state; invalid.project.push_back('\0');
    if (write(root / "Session", invalid, error) || error.empty() || !read(root / "Session", loaded, error) || loaded.project != state.project) return 15;
    invalid = state; invalid.recentProjects[0].push_back('\0');
    if (write(root / "Session", invalid, error) || error.empty() || !read(root / "Session", loaded, error) || loaded.recentProjects != state.recentProjects) return 16;
    { std::ofstream file(root / "Session/session.json", std::ios::trunc); file << "{broken"; }
    if (read(root / "Session", loaded, error) || error.empty()) return 5;
    fs::remove_all(root); return 0;
}
