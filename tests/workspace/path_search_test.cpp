#include "workspace/PathSearch.hpp"

#include <algorithm>
#include <string>
#include <vector>

using lucid::workspace::filterPaths;

int main() {
    const std::vector<std::string> paths{
        "README.md",
        "docs/architecture.md",
        "src/app/Application.cpp",
        "src/ui/search/SearchBar.cpp",
        "src/workspace/Project.cpp",
    };

    auto results = filterPaths(paths, "app", 8);
    if (results.empty() || results.front() != "src/app/Application.cpp") {
        return 1;
    }

    results = filterPaths(paths, "appcpp", 8);
    if (results.empty() || results.front() != "src/app/Application.cpp") {
        return 2;
    }

    results = filterPaths(paths, "README", 8);
    if (results.empty() || results.front() != "README.md") {
        return 3;
    }

    results = filterPaths(paths, "searchbar", 8);
    if (results.empty() || results.front() != "src/ui/search/SearchBar.cpp") {
        return 4;
    }

    results = filterPaths(paths, "Project.cpp", 8);
    if (results.empty() || results.front() != "src/workspace/Project.cpp") {
        return 5;
    }

    results = filterPaths(paths, "SRC", 8);
    if (results.empty()) {
        return 6;
    }

    results = filterPaths(paths, "zzz", 8);
    if (!results.empty()) {
        return 7;
    }

    results = filterPaths(paths, "", 3);
    if (results.size() != 3) {
        return 8;
    }

    results = filterPaths(paths, "s", 2);
    if (results.size() != 2) {
        return 9;
    }

    const std::vector<std::string> files{"main.cpp", "src/main.cpp", "src/lib/helper.cpp", "other/main.cpp"};
    results = lucid::workspace::filterChildPaths(files, "", 8);
    if (results.size() != 1 || results[0] != "main.cpp") return 10;
    results = lucid::workspace::filterChildPaths(files, "src/", 8);
    if (results.size() != 1 || results[0] != "src/main.cpp") return 11;
    results = lucid::workspace::filterChildPaths(files, "src/ma", 8);
    if (results.size() != 1 || results[0] != "src/main.cpp") return 12;
    const std::vector<std::string> directories{"src", "src/lib", "src/lib/nested", "other"};
    results = lucid::workspace::filterChildPaths(directories, "src/", 8);
    if (results.size() != 1 || results[0] != "src/lib") return 13;

    const std::vector<std::string> longPaths{std::string(180, 'x') + "a.cpp", "short.txt"};
    results = filterPaths(longPaths, "a", 8);
    if (results.size() != 1 || results[0] != longPaths[0]) return 14;
    results = lucid::workspace::filterChildPaths(longPaths, "a", 8);
    if (results.size() != 1 || results[0] != longPaths[0]) return 15;

    std::vector<std::string> manyPaths;
    for (int i = 999; i >= 0; --i) manyPaths.push_back("folder/file" + std::to_string(i) + ".cpp");
    const auto allMatches = filterPaths(manyPaths, "file", manyPaths.size());
    const auto topMatches = filterPaths(manyPaths, "file", 12);
    if (allMatches.size() != manyPaths.size() || topMatches.size() != 12 ||
        !std::equal(topMatches.begin(), topMatches.end(), allMatches.begin())) return 16;
    if (!filterPaths(manyPaths, "file", 0).empty()) return 17;

    return 0;
}
