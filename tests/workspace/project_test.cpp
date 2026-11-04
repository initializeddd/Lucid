#include "workspace/Project.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#if defined(__linux__) || defined(__APPLE__)
#include <sys/xattr.h>
#endif

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
    (void)Project::openOrCreate(base / "missing", created, error, false, false);
    if (error.empty() || created || fs::exists(base / "missing")) return 28;
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
    bool deferredCreated = false;
    auto deferred = Project::openOrCreate(base / "demo", deferredCreated, error, false);
    if (!error.empty() || deferredCreated || !deferred.files().empty()) return 26;
    deferred.refresh();
    if (std::find(deferred.files().begin(), deferred.files().end(), "src/main.cpp") == deferred.files().end()) return 27;

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
    const auto executable = base / "demo/src/main.cpp";
#if !defined(_WIN32)
    fs::permissions(executable, fs::perms::owner_exec, fs::perm_options::add, filesystemError);
#endif
    if (filesystemError || !project.writeFile("src/main.cpp", "updated\n", error) ||
        !project.readFile("src/main.cpp", content, error) || content != "updated\n") return 20;
    const std::string stale = "old contents";
    if (project.writeFile("src/main.cpp", "must not replace\n", error, &stale) || error.empty() ||
        !project.readFile("src/main.cpp", content, error) || content != "updated\n") return 23;
    const std::string expected = content;
    if (!project.writeFile("src/main.cpp", "updated again\n", error, &expected) ||
        !project.readFile("src/main.cpp", content, error) || content != "updated again\n") return 24;
    if (!project.createFile("src/main.cpp", error) ||
        !project.readFile("src/main.cpp", content, error) || content != "updated again\n") return 25;
    if (project.createFile("src/main.cpp", error, false) || error.empty() ||
        !project.readFile("src/main.cpp", content, error) || content != "updated again\n") return 32;
#if !defined(_WIN32)
    const auto hardLink = base / "demo/src/linked.cpp";
    fs::create_hard_link(executable, hardLink, filesystemError);
    if (!filesystemError) {
        if (project.writeFile("src/main.cpp", "replacement", error) || error.empty() ||
            !project.readFile("src/main.cpp", content, error) || content != "updated again\n") return 28;
        fs::remove(hardLink, filesystemError);
        if (filesystemError) return 29;
    }
    filesystemError.clear();
#endif
#if defined(__linux__) || defined(__APPLE__)
#if defined(__APPLE__)
    const char* attribute = "dev.lucid.test";
    const int setResult = setxattr(executable.c_str(), attribute, "kept", 4, 0, 0);
#else
    const char* attribute = "user.lucid-test";
    const int setResult = setxattr(executable.c_str(), attribute, "kept", 4, 0);
#endif
    if (setResult == 0) {
        if (!project.writeFile("src/main.cpp", "metadata test\n", error)) return 30;
        char value[8]{};
#if defined(__APPLE__)
        const auto size = getxattr(executable.c_str(), attribute, value, sizeof(value), 0, 0);
#else
        const auto size = getxattr(executable.c_str(), attribute, value, sizeof(value));
#endif
        if (size != 4 || std::string(value, 4) != "kept") return 31;
    }
#endif
#if !defined(_WIN32)
    if ((fs::status(executable).permissions() & fs::perms::owner_exec) == fs::perms::none) return 21;
#endif
    for (const auto& entry : fs::directory_iterator(executable.parent_path()))
        if (entry.path().filename().string().starts_with(".lucid-save-")) return 22;

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

    { std::ofstream file(base / "demo" / "size-limit.bin", std::ios::binary); }
    fs::resize_file(base / "demo" / "size-limit.bin", Project::maxFileSize, filesystemError);
    if (filesystemError || !project.readFile("size-limit.bin", content, error) || content.size() != Project::maxFileSize) return 40;
    fs::resize_file(base / "demo" / "size-limit.bin", Project::maxFileSize + 1, filesystemError);
    if (filesystemError || project.readFile("size-limit.bin", content, error) || !content.empty() || error.empty()) return 41;
    if (!project.writeFile("exclusive.txt", "first", error, nullptr, true) ||
        project.writeFile("exclusive.txt", "second", error, nullptr, true) ||
        !project.readFile("exclusive.txt", content, error) || content != "first") return 42;
    std::atomic_bool cancelScan{true}; bool scanComplete = true;
    project.refresh(&scanComplete, &cancelScan);
    if (scanComplete) return 43;
    fs::remove_all(base, filesystemError);
    return 0;
}
