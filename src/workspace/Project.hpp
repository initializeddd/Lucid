#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::workspace {

class Project {
public:
    Project() = default;

    static Project openOrCreate(const std::filesystem::path& path, bool& created,
                                std::string& error);

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return m_root; }
    [[nodiscard]] std::string name() const;
    [[nodiscard]] const std::vector<std::string>& files() const noexcept { return m_files; }
    [[nodiscard]] const std::vector<std::string>& directories() const noexcept {
        return m_directories;
    }

    void refresh();

    [[nodiscard]] bool exists(std::string_view relative) const;
    [[nodiscard]] bool isDirectory(std::string_view relative) const;
    [[nodiscard]] std::filesystem::path resolve(std::string_view relative,
                                                std::string& error) const;

    [[nodiscard]] bool readFile(std::string_view relative, std::string& content,
                                std::string& error) const;
    [[nodiscard]] bool writeFile(std::string_view relative, std::string_view content,
                                 std::string& error);
    [[nodiscard]] bool createFile(std::string_view relative, std::string& error);
    [[nodiscard]] bool createDirectory(std::string_view relative, std::string& error);
    [[nodiscard]] bool deleteFile(std::string_view relative, std::string& error);

private:
    explicit Project(std::filesystem::path root);

    std::filesystem::path m_root;
    std::vector<std::string> m_files;
    std::vector<std::string> m_directories;
};

} // namespace lucid::workspace
