#include "workspace/collaboration/SharedProject.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#if defined(__linux__) || defined(__APPLE__)
#include <sys/xattr.h>
#endif
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
    if (shared.edit("a", {2, 0, 0, std::string("x\0y", 3)}, applied, error) || error.empty() ||
        shared.document("a.txt")->text() != "Hi hello Lucid" || shared.document("a.txt")->revision() != 2) return 16;
    if (!shared.document("a.txt") || shared.document("a.txt")->text() != "Hi hello Lucid" ||
        shared.document("a.txt")->participants().at("b").cursor.caret != 14 || !shared.moveCursor("b", {3, 3}, error)) return 5;
    if (!shared.open("b", "b.txt", file, error) || shared.document("a.txt")->participants().contains("b")) return 6;
    if (shared.open("b", "../outside", file, error)) return 7;
    { std::ofstream f(dir / "a.txt", std::ios::trunc); f << "external"; }
    if (shared.save("a.txt", error) || shared.document("a.txt")->text() != "Hi hello Lucid") return 8;
    { std::ofstream f(dir / "a.txt", std::ios::trunc); f << "hello world"; }
#if defined(__APPLE__)
    const char* attribute = "dev.lucid.shared-test";
    const bool hasAttribute = setxattr((dir / "a.txt").c_str(), attribute, "kept", 4, 0, 0) == 0;
#elif defined(__linux__)
    const char* attribute = "user.lucid-shared-test";
    const bool hasAttribute = setxattr((dir / "a.txt").c_str(), attribute, "kept", 4, 0) == 0;
#endif
    if (!shared.save("a.txt", error)) return 9;
    { std::ifstream f(dir / "a.txt"); std::string text((std::istreambuf_iterator<char>(f)), {}); if (text != "Hi hello Lucid") return 10; }
#if defined(__APPLE__)
    char value[8]{};
    if (hasAttribute && (getxattr((dir / "a.txt").c_str(), attribute, value, sizeof(value), 0, 0) != 4 || std::string(value, 4) != "kept")) return 13;
#elif defined(__linux__)
    char value[8]{};
    if (hasAttribute && (getxattr((dir / "a.txt").c_str(), attribute, value, sizeof(value)) != 4 || std::string(value, 4) != "kept")) return 13;
#endif
    shared.leave("a"); if (shared.edit("a", {2, 0, 0, "x"}, applied, error)) return 11;
    for (int i = 0; i < 40; ++i) {
        const auto name = "file-" + std::to_string(i) + ".txt";
        { std::ofstream f(dir / name); f << "clean"; }
        if (!shared.open("b", name, file, error)) return 14;
    }
    if (!shared.edit("b", {0, 0, 0, "unsaved "}, applied, error) ||
        !shared.open("b", "b.txt", file, error) ||
        !shared.open("b", "file-39.txt", file, error) ||
        !shared.document("file-39.txt") || shared.document("file-39.txt")->text() != "unsaved clean") return 15;
    // At the file limit, a clean current file can be replaced, while dirty
    // files remain available until saved. A failed open keeps the old file.
    bool createdLimit = false;
    auto limitProject = Project::openOrCreate(dir, createdLimit, error);
    if (!error.empty()) return 17;
    SharedProject limit(std::move(limitProject));
    if (!limit.join("c", "Carol", error)) return 18;
    for (int i = 0; i < 31; ++i) {
        const auto name = "limit-" + std::to_string(i) + ".txt";
        { std::ofstream f(dir / name); f << "clean"; }
        if (!limit.open("c", name, file, error) || !limit.edit("c", {0, 0, 0, "dirty "}, applied, error)) return 19;
    }
    { std::ofstream f(dir / "limit-clean.txt"); f << "clean"; }
    { std::ofstream f(dir / "limit-next.txt"); f << "next"; }
    if (!limit.open("c", "limit-clean.txt", file, error) ||
        limit.open("c", "missing.txt", file, error) || !limit.document("limit-clean.txt") ||
        !limit.open("c", "limit-next.txt", file, error) || limit.document("limit-clean.txt") ||
        !limit.document("limit-next.txt")) return 20;
    if (!limit.edit("c", {0, 0, 0, "dirty "}, applied, error) ||
        limit.open("c", "limit-clean.txt", file, error) || !limit.document("limit-next.txt") ||
        !limit.save("limit-0.txt", error) || limit.document("limit-0.txt") ||
        !limit.open("c", "limit-clean.txt", file, error) ||
        !limit.document("limit-next.txt") || limit.document("limit-next.txt")->text() != "dirty next") return 21;
    std::filesystem::remove_all(dir); return 0;
}
