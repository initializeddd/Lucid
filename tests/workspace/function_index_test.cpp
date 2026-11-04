#include "workspace/FunctionIndex.hpp"
#include "workspace/Project.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>

int main() {
    namespace fs = std::filesystem;
    using lucid::workspace::Project;
    using lucid::workspace::loadFunctionIndex;
    using lucid::workspace::searchFunctions;

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path base =
        fs::temp_directory_path() / ("lucid-function-index-test-" + std::to_string(stamp));
    std::error_code filesystemError;
    fs::remove_all(base, filesystemError);

    std::string error;
    bool created = false;
    Project project = Project::openOrCreate(base / "demo", created, error);
    if (!error.empty() || !created) {
        return 1;
    }

    const std::string source =
        "/*\n"
        "@build\n"
        "$ cmake --build build\n"
        "@end\n"
        "@win:windows\n"
        "$ build.bat\n"
        "@end\n"
        "*/\n";
    if (!project.writeFile("src/main.cpp", source, error)) {
        return 2;
    }
    if (!project.writeFile("notes.txt", "no functions here\n", error)) {
        return 3;
    }
    project.refresh();

    lucid::workspace::FunctionIndex index = loadFunctionIndex(project, error);
    if (!error.empty() || index.usedCache || index.functions.size() != 2) {
        return 4;
    }
    if (!project.exists(".lucid/cache/functions.json")) {
        return 5;
    }
    for (const std::string& file : project.files()) {
        if (file == ".lucid/cache/functions.json") {
            return 6;
        }
    }

    const std::vector<std::size_t> build = searchFunctions(index.functions, "build", 8);
    if (build.empty() ||
        index.functions[build.front()].name != "build" ||
        index.functions[build.front()].commands.size() != 1 ||
        index.functions[build.front()].commands.front().text !=
            "cmake --build build") {
        return 7;
    }

    const std::vector<std::size_t> windows = searchFunctions(index.functions, "win", 8);
    if (windows.empty() || index.functions[windows.front()].name != "win" ||
        index.functions[windows.front()].platform !=
            lucid::editor::CompilePlatform::Windows) {
        return 8;
    }

    lucid::workspace::FunctionIndex cached = loadFunctionIndex(project, error);
    if (!error.empty() || !cached.usedCache || cached.functions.size() != 2) {
        return 9;
    }

    const fs::file_time_type oldTime =
        fs::last_write_time(project.root() / "src/main.cpp");
    const std::string changed =
        "/*\n"
        "@build\n"
        "$ make all\n"
        "@end\n"
        "@test\n"
        "$ ctest --test-dir build\n"
        "@end\n"
        "*/\n";
    if (!project.writeFile("src/main.cpp", changed, error)) {
        return 10;
    }
    fs::last_write_time(project.root() / "src/main.cpp", oldTime + std::chrono::seconds(1));

    lucid::workspace::FunctionIndex updated = loadFunctionIndex(project, error);
    if (!error.empty() || updated.usedCache || updated.functions.size() != 2) {
        return 11;
    }
    const std::vector<std::size_t> test = searchFunctions(updated.functions, "test", 8);
    if (test.empty() || updated.functions[test.front()].commands.front().text !=
                            "ctest --test-dir build") {
        return 12;
    }

    if (!project.writeFile("src/bsd.cpp", "/*\n@open:openbsd\n$ echo open\n@end\n@free:freebsd\n$ echo free\n@end\n*/\n", error)) return 13;
    project.refresh();
    auto bsdIndex = loadFunctionIndex(project, error);
    if (!error.empty()) return 14;
    bsdIndex = loadFunctionIndex(project, error);
    const auto open = searchFunctions(bsdIndex.functions, "open", 8);
    const auto free = searchFunctions(bsdIndex.functions, "free", 8);
    if (!bsdIndex.usedCache || open.empty() || free.empty() ||
        bsdIndex.functions[open.front()].platform != lucid::editor::CompilePlatform::OpenBSD ||
        bsdIndex.functions[free.front()].platform != lucid::editor::CompilePlatform::FreeBSD) return 15;
    std::vector<lucid::workspace::ProjectFunction> many;
    for (int i = 99; i >= 0; --i) {
        lucid::workspace::ProjectFunction function;
        function.name = "build" + std::to_string(i % 11);
        function.file = "build.cpp";
        function.line = i;
        many.push_back(std::move(function));
    }
    const auto all = searchFunctions(many, "build", many.size());
    const auto first = searchFunctions(many, "build", 13);
    if (all.size() != many.size() || first.size() != 13 || !std::equal(first.begin(), first.end(), all.begin())) return 16;
    if (!searchFunctions(many, "build", 0).empty()) return 17;
    lucid::workspace::ProjectFunction longName;
    longName.name = "build" + std::string(1000, 'x');
    longName.file = "src/example.cpp";
    lucid::workspace::ProjectFunction longPath;
    longPath.name = "compile";
    longPath.file = std::string(500, 'x') + "/target.cpp";
    const std::vector<lucid::workspace::ProjectFunction> longFunctions{longName, longPath};
    if (searchFunctions(longFunctions, "build", 8) != std::vector<std::size_t>{0}) return 18;
    if (searchFunctions(longFunctions, "target", 8) != std::vector<std::size_t>{1}) return 19;
    fs::remove_all(base, filesystemError);
    return 0;
}
