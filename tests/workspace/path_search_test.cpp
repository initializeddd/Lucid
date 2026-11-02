#include "workspace/PathSearch.hpp"

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

    return 0;
}
