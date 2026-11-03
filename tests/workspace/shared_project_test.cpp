#include "workspace/collaboration/SharedProject.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
using namespace lucid::workspace;
using namespace lucid::workspace::collaboration;
int main() {
    const auto dir = std::filesystem::temp_directory_path() / ("lucid-share-project-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    { std::ofstream f(dir / "a.txt"); f << "hello world"; }
    { std::ofstream f(dir / "b.txt"); f << "other"; }
    bool created = false; std::string error; auto project = Project::openOrCreate(dir, created, error); if (!error.empty()) return 1;
    SharedProject shared(std::move(project)); std::string file;
    if (!shared.join("a", "Alice", error) || !shared.join("b", "Bob", error) || shared.join("a", "Other", error)) return 2;
    if (!shared.open("a", "a.txt", file, error) || file != "a.txt" || !shared.open("b", "a.txt", file, error)) return 3;
    if (!shared.moveCursor("b", {6, 6}, error)) return 12;
    Edit applied; if (!shared.edit("a", {0, 0, 0, "Hi "}, applied, error) || !shared.edit("b", {0, 6, 5, "Lucid"}, applied, error)) return 4;
    if (!shared.document("a.txt") || shared.document("a.txt")->text() != "Hi hello Lucid" ||
        shared.document("a.txt")->participants().at("b").cursor.caret != 14 || !shared.moveCursor("b", {3, 3}, error)) return 5;
    if (!shared.open("b", "b.txt", file, error) || shared.document("a.txt")->participants().contains("b")) return 6;
    if (shared.open("b", "../outside", file, error)) return 7;
    { std::ofstream f(dir / "a.txt", std::ios::trunc); f << "external"; }
    if (shared.save("a.txt", error) || shared.document("a.txt")->text() != "Hi hello Lucid") return 8;
    { std::ofstream f(dir / "a.txt", std::ios::trunc); f << "hello world"; }
    if (!shared.save("a.txt", error)) return 9;
    { std::ifstream f(dir / "a.txt"); std::string text((std::istreambuf_iterator<char>(f)), {}); if (text != "Hi hello Lucid") return 10; }
    shared.leave("a"); if (shared.edit("a", {2, 0, 0, "x"}, applied, error)) return 11;
    std::filesystem::remove_all(dir); return 0;
}
