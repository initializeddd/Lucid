#pragma once
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>
namespace lucid::workspace::recovery {
struct FileContext { std::string path; std::size_t line = 0, column = 0; int firstLine = 0; bool operator==(const FileContext&) const = default; };
struct Snapshot {
    std::string project, file, baseline, contents;
    std::size_t line = 0, column = 0;
    int firstLine = 0;
    std::vector<FileContext> openFiles;
    bool operator==(const Snapshot&) const = default;
};
[[nodiscard]] bool write(const std::filesystem::path& path, const Snapshot& snapshot, std::string& error);
[[nodiscard]] bool read(const std::filesystem::path& path, Snapshot& snapshot, std::string& error);
}
