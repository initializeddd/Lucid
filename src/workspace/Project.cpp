#include "workspace/Project.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace lucid::workspace {
namespace {

constexpr std::uintmax_t kMaxFileSize = 16U * 1024U * 1024U;

[[nodiscard]] bool isWithin(const std::filesystem::path& root,
                            const std::filesystem::path& path) {
    auto rootPart = root.begin();
    auto pathPart = path.begin();

    for (; rootPart != root.end(); ++rootPart, ++pathPart) {
        if (pathPart == path.end() || *rootPart != *pathPart) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool isSkippedDirectory(const std::string& name) {
    static constexpr std::array<std::string_view, 13> kSkippedDirectories{
        ".git",         ".hg",       ".svn",      "node_modules", "build",
        "dist",         "out",       "target",    ".cache",       ".venv",
        "venv",         "__pycache__", ".lucid"};

    return std::find(kSkippedDirectories.begin(), kSkippedDirectories.end(), name) !=
           kSkippedDirectories.end();
}

[[nodiscard]] std::string pathError(const std::string& prefix,
                                    const std::filesystem::path& path) {
    return prefix + ": " + path.string();
}

} // namespace

Project::Project(std::filesystem::path root) : m_root(std::move(root)) {}

Project Project::openOrCreate(const std::filesystem::path& path, bool& created,
                              std::string& error) {
    created = false;
    error.clear();

    if (path.empty()) {
        error = "Project path is empty";
        return {};
    }

    std::error_code filesystemError;
    const bool exists = std::filesystem::exists(path, filesystemError);
    if (filesystemError) {
        error = filesystemError.message();
        return {};
    }

    if (exists) {
        if (!std::filesystem::is_directory(path, filesystemError) || filesystemError) {
            error = pathError("Project path is not a directory", path);
            return {};
        }
    } else {
        if (!std::filesystem::create_directories(path, filesystemError) || filesystemError) {
            error = pathError("Could not create project directory", path);
            return {};
        }
        created = true;
    }

    const std::filesystem::path root = std::filesystem::weakly_canonical(path, filesystemError);
    if (filesystemError) {
        error = filesystemError.message();
        return {};
    }

    Project project(root);
    project.refresh();
    return project;
}

std::string Project::name() const {
    const std::string filename = m_root.filename().string();
    return filename.empty() ? m_root.string() : filename;
}

void Project::refresh() {
    m_files.clear();
    m_directories.clear();

    std::error_code error;
    if (m_root.empty() || !std::filesystem::is_directory(m_root, error) || error) {
        return;
    }

    std::filesystem::recursive_directory_iterator iterator(
        m_root, std::filesystem::directory_options::skip_permission_denied, error);
    const std::filesystem::recursive_directory_iterator end;

    while (!error && iterator != end) {
        const std::filesystem::directory_entry& entry = *iterator;
        const std::string filename = entry.path().filename().string();

        std::error_code entryError;
        if (entry.is_directory(entryError) && !entryError) {
            if (isSkippedDirectory(filename)) {
                iterator.disable_recursion_pending();
            } else {
                const std::string relative =
                    entry.path().lexically_relative(m_root).generic_string();
                if (!relative.empty()) {
                    m_directories.push_back(relative);
                }
            }
        } else if (!entryError && entry.is_regular_file(entryError) && !entryError &&
                   filename != ".DS_Store") {
            const std::string relative =
                entry.path().lexically_relative(m_root).generic_string();
            if (!relative.empty()) {
                m_files.push_back(relative);
            }
        }

        iterator.increment(error);
    }

    std::sort(m_files.begin(), m_files.end());
    std::sort(m_directories.begin(), m_directories.end());
}

std::filesystem::path Project::resolve(std::string_view relative, std::string& error) const {
    error.clear();

    if (m_root.empty()) {
        error = "No project is open";
        return {};
    }
    if (relative.empty()) {
        error = "Path is empty";
        return {};
    }

    const std::filesystem::path requested{std::string(relative)};
    if (requested.is_absolute()) {
        error = "Path must be relative to the project";
        return {};
    }

    const std::filesystem::path normalized = requested.lexically_normal();
    if (normalized.empty() || normalized == ".") {
        error = "Path is empty";
        return {};
    }

    for (const std::filesystem::path& part : normalized) {
        if (part == "..") {
            error = "Path cannot leave the project";
            return {};
        }
    }

    std::error_code filesystemError;
    const std::filesystem::path root = std::filesystem::weakly_canonical(m_root, filesystemError);
    if (filesystemError) {
        error = filesystemError.message();
        return {};
    }

    const std::filesystem::path candidate =
        std::filesystem::weakly_canonical(m_root / normalized, filesystemError);
    if (filesystemError) {
        error = filesystemError.message();
        return {};
    }

    if (!isWithin(root, candidate)) {
        error = "Path cannot leave the project";
        return {};
    }

    return candidate;
}

bool Project::exists(std::string_view relative) const {
    std::string error;
    const std::filesystem::path path = resolve(relative, error);
    if (!error.empty()) {
        return false;
    }

    std::error_code filesystemError;
    return std::filesystem::exists(path, filesystemError) && !filesystemError;
}

bool Project::isDirectory(std::string_view relative) const {
    std::string error;
    const std::filesystem::path path = resolve(relative, error);
    if (!error.empty()) {
        return false;
    }

    std::error_code filesystemError;
    return std::filesystem::is_directory(path, filesystemError) && !filesystemError;
}

bool Project::readFile(std::string_view relative, std::string& content,
                       std::string& error) const {
    content.clear();
    error.clear();

    const std::filesystem::path path = resolve(relative, error);
    if (!error.empty()) {
        return false;
    }

    std::error_code filesystemError;
    if (!std::filesystem::is_regular_file(path, filesystemError) || filesystemError) {
        error = pathError("Not a file", path);
        return false;
    }

    const std::uintmax_t size = std::filesystem::file_size(path, filesystemError);
    if (filesystemError) {
        error = filesystemError.message();
        return false;
    }
    if (size > kMaxFileSize) {
        error = "File is too large to open";
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = pathError("Could not open file", path);
        return false;
    }

    content.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    if (!file.eof() && file.fail()) {
        error = pathError("Could not read file", path);
        content.clear();
        return false;
    }

    return true;
}

bool Project::writeFile(std::string_view relative, std::string_view content,
                        std::string& error) {
    error.clear();

    const std::filesystem::path path = resolve(relative, error);
    if (!error.empty()) {
        return false;
    }

    std::error_code filesystemError;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), filesystemError);
        if (filesystemError) {
            error = filesystemError.message();
            return false;
        }
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = pathError("Could not write file", path);
        return false;
    }

    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    file.close();
    if (!file) {
        error = pathError("Could not write file", path);
        return false;
    }

    return true;
}

bool Project::createFile(std::string_view relative, std::string& error) {
    error.clear();

    const std::filesystem::path path = resolve(relative, error);
    if (!error.empty()) {
        return false;
    }

    std::error_code filesystemError;
    if (std::filesystem::exists(path, filesystemError) && !filesystemError) {
        if (std::filesystem::is_regular_file(path, filesystemError) && !filesystemError) {
            refresh();
            return true;
        }
        error = pathError("Path is not a file", path);
        return false;
    }

    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), filesystemError);
        if (filesystemError) {
            error = filesystemError.message();
            return false;
        }
    }

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        error = pathError("Could not create file", path);
        return false;
    }
    file.close();

    refresh();
    return true;
}

bool Project::deleteFile(std::string_view relative, std::string& error) {
    error.clear();

    const std::filesystem::path path = resolve(relative, error);
    if (!error.empty()) {
        return false;
    }

    std::error_code filesystemError;
    if (!std::filesystem::is_regular_file(path, filesystemError) || filesystemError) {
        error = pathError("Not a file", path);
        return false;
    }

    if (!std::filesystem::remove(path, filesystemError) || filesystemError) {
        error = pathError("Could not delete file", path);
        return false;
    }

    refresh();
    return true;
}

bool Project::createDirectory(std::string_view relative, std::string& error) {
    error.clear();

    const std::filesystem::path path = resolve(relative, error);
    if (!error.empty()) {
        return false;
    }

    std::error_code filesystemError;
    std::filesystem::create_directories(path, filesystemError);
    if (filesystemError) {
        error = filesystemError.message();
        return false;
    }

    return true;
}

} // namespace lucid::workspace
