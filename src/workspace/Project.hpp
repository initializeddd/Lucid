#pragma once

#include <filesystem>
#include <atomic>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::workspace {

class Project {
public:
    static constexpr std::size_t maxFileSize = 16U * 1024U * 1024U;
    Project() = default;

    static Project openOrCreate(const std::filesystem::path& path, bool& created,
                                std::string& error, bool eagerIndex = true,
                                bool allowCreate = true);

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return m_root; }
    [[nodiscard]] std::string name() const;
    // Project-relative names and path arguments are UTF-8 (with '/' separators
    // in the index). On POSIX, existing non-UTF-8 filename bytes are retained.
    [[nodiscard]] const std::vector<std::string>& files() const noexcept { return m_files; }
    [[nodiscard]] const std::vector<std::string>& directories() const noexcept {
        return m_directories;
    }

    // When requested, report whether the entire indexable tree was scanned.
    void refresh(bool* complete = nullptr, const std::atomic_bool* cancel = nullptr,
                 bool includeGenerated = false);

    [[nodiscard]] bool exists(std::string_view relative) const;
    [[nodiscard]] bool isDirectory(std::string_view relative) const;
    [[nodiscard]] std::filesystem::path resolve(std::string_view relative,
                                                std::string& error) const;

    [[nodiscard]] bool readFile(std::string_view relative, std::string& content,
                                std::string& error, const std::atomic_bool* cancel = nullptr) const;
    // Explicit larger limits are reserved for read-only tools, not editor buffers.
    [[nodiscard]] bool readFileBounded(std::string_view relative, std::string& content,
                                       std::string& error, std::size_t limit, const std::atomic_bool* cancel = nullptr) const;
    [[nodiscard]] bool writeFile(std::string_view relative, std::string_view content,
                                 std::string& error, const std::string* expected = nullptr,
                                 bool createOnly = false, const std::atomic_bool* cancel = nullptr);
    [[nodiscard]] bool createFile(std::string_view relative, std::string& error, bool allowExisting = true);
    [[nodiscard]] bool createDirectory(std::string_view relative, std::string& error);
    [[nodiscard]] bool deleteFile(std::string_view relative, std::string& error);

private:
    explicit Project(std::filesystem::path root);

    std::filesystem::path m_root;
    std::vector<std::string> m_files;
    std::vector<std::string> m_directories;
    bool m_autoRefresh = true;
};

} // namespace lucid::workspace
