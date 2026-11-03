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
    State invalid = state; invalid.files[0].path = "../outside";
    if (write(root / "Session", invalid, error) || error.empty()) return 3;
    if (!read(root / "Session", loaded, error) || loaded.files != state.files) return 4;
    { std::ofstream file(root / "Session/session.json", std::ios::trunc); file << "{broken"; }
    if (read(root / "Session", loaded, error) || error.empty()) return 5;
    fs::remove_all(root); return 0;
}
