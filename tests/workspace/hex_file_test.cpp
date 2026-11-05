#include "modules/HexFile.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
int main() {
    namespace fs = std::filesystem; using namespace lucid::modules;
    auto dir = fs::temp_directory_path() / ("lucid-hex-file-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())); fs::create_directories(dir);
    auto write = [&](const std::string& s) { std::ofstream f(dir / "data", std::ios::binary | std::ios::trunc); f << s; };
    write("abc"); bool created = false; std::string error; auto project = lucid::workspace::Project::openOrCreate(dir, created, error, false, false); if (!error.empty()) return 1;
    HexFile file; if (!file.open(project, "data", error) || !file.buffer().overwrite(0, "X")) return 2;
    std::string disk; if (!project.readFile("data", disk, error) || disk != "abc") return 3;
    std::atomic_bool cancel{true}; if (file.save(project, error, &cancel) || !project.readFile("data", disk, error) || disk != "abc") return 4;
    write("xyz"); if (file.save(project, error) || !project.readFile("data", disk, error) || disk != "xyz") return 5;
    write("abc"); if (!file.save(project, error) || !project.readFile("data", disk, error) || disk != "Xbc") return 6;
    if (file.open(project, "../data", error)) return 7;
    std::error_code ec; fs::create_symlink(dir / "data", dir / "link", ec); if (!ec && file.open(project, "link", error)) return 8;
    cancel = true; if (file.open(project, "data", error, &cancel)) return 9;
    { std::ofstream f(dir / fs::path(u8"ü.bin"), std::ios::binary); f << "xyz"; }
    cancel=false; if (!file.open(project, "ü.bin", error, &cancel) || !file.buffer().overwrite(0,"A") || !file.save(project,error,&cancel) ||
        !project.readFile("ü.bin",disk,error) || disk != "Ayz") return 10;
    fs::remove_all(dir); return 0;
}
