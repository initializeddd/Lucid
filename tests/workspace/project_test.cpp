#include "workspace/Project.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    namespace fs = std::filesystem;
    using lucid::workspace::Project;

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path base =
        fs::temp_directory_path() / ("lucid-project-test-" + std::to_string(stamp));

    std::error_code filesystemError;
    fs::remove_all(base, filesystemError);

    std::string error;
    bool created = false;
    Project project =
        Project::openOrCreate(base / "demo", created, error);
    if (!error.empty() || !created) {
        return 1;
    }
    if (!fs::is_directory(base / "demo")) {
        return 2;
    }
    if (project.name() != "demo" || !project.root().is_absolute()) {
        return 3;
    }

    if (!project.writeFile("src/main.cpp", "int main() {}\n", error)) {
        return 4;
    }
    if (!project.exists("src/main.cpp") || !project.isDirectory("src")) {
        return 5;
    }

    project.refresh();
    bool found = false;
    for (const std::string& file : project.files()) {
        if (file == "src/main.cpp") {
            found = true;
        }
    }
    if (!found) {
        return 6;
    }

    std::string content;
    if (!project.readFile("src/main.cpp", content, error) ||
        content != "int main() {}\n") {
        return 7;
    }

    if (!project.createFile("docs/notes.txt", error) ||
        !project.exists("docs/notes.txt")) {
        return 8;
    }
    if (!project.createDirectory("assets/icons", error) ||
        !project.isDirectory("assets/icons")) {
        return 9;
    }

    project.refresh();
    bool hasSourceDirectory = false;
    bool hasAssetsDirectory = false;
    for (const std::string& directory : project.directories()) {
        if (directory == "src") {
            hasSourceDirectory = true;
        }
        if (directory == "assets") {
            hasAssetsDirectory = true;
        }
    }
    if (!hasSourceDirectory || !hasAssetsDirectory) {
        return 17;
    }

    if (project.exists("../escape.txt")) {
        return 10;
    }
    error.clear();
    if (project.readFile("../escape.txt", content, error) || error.empty()) {
        return 11;
    }
    if (project.exists("/tmp")) {
        return 12;
    }
    if (project.exists("")) {
        return 13;
    }

    bool createdAgain = true;
    Project reopened =
        Project::openOrCreate(base / "demo", createdAgain, error);
    if (!error.empty() || createdAgain || reopened.name() != "demo") {
        return 14;
    }

    {
        std::ofstream file(base / "not-a-dir");
        file << "x";
    }
    bool createdFile = false;
    Project invalid =
        Project::openOrCreate(base / "not-a-dir", createdFile, error);
    if (error.empty()) {
        return 15;
    }

    Project empty =
        Project::openOrCreate("", createdFile, error);
    if (error.empty()) {
        return 16;
    }

    if (!project.writeFile("temp/delete-me.txt", "x", error)) {
        return 18;
    }
    if (!project.deleteFile("temp/delete-me.txt", error) ||
        project.exists("temp/delete-me.txt")) {
        return 19;
    }
    error.clear();
    if (project.deleteFile("temp/delete-me.txt", error) || error.empty()) {
        return 20;
    }
    error.clear();
    if (project.deleteFile("src", error) || error.empty()) {
        return 21;
    }

    fs::remove_all(base, filesystemError);
    return 0;
}
