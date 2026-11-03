#include "workspace/Project.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>
#include <cerrno>
#include <cstring>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__linux__) || defined(__APPLE__)
#include <sys/xattr.h>
#endif
#endif

namespace lucid::workspace {
namespace {

constexpr std::uintmax_t kMaxFileSize = 16U * 1024U * 1024U;

#if defined(__linux__) || defined(__APPLE__)
bool copyExtendedAttributes(int source, int target) {
#if defined(__APPLE__)
    const auto count = flistxattr(source, nullptr, 0, 0);
#else
    const auto count = flistxattr(source, nullptr, 0);
#endif
    if (count < 0) return errno == ENOTSUP || errno == EOPNOTSUPP;
    if (count == 0) return true;
    if (count > 1024 * 1024) return false;
    std::vector<char> names(static_cast<std::size_t>(count));
#if defined(__APPLE__)
    const auto actual = flistxattr(source, names.data(), names.size(), 0);
#else
    const auto actual = flistxattr(source, names.data(), names.size());
#endif
    if (actual != count) return false;
    for (std::size_t offset = 0; offset < names.size();) {
        const char* name = names.data() + offset;
        const auto end = std::find(names.begin() + static_cast<std::ptrdiff_t>(offset), names.end(), '\0');
        if (end == names.end() || end == names.begin() + static_cast<std::ptrdiff_t>(offset)) return false;
#if defined(__APPLE__)
        const auto size = fgetxattr(source, name, nullptr, 0, 0, 0);
#else
        const auto size = fgetxattr(source, name, nullptr, 0);
#endif
        if (size < 0 || size > 16 * 1024 * 1024) return false;
        std::vector<char> value(static_cast<std::size_t>(size));
#if defined(__APPLE__)
        if (fgetxattr(source, name, value.data(), value.size(), 0, 0) != size || fsetxattr(target, name, value.data(), value.size(), 0, 0) != 0) return false;
#else
        if (fgetxattr(source, name, value.data(), value.size()) != size || fsetxattr(target, name, value.data(), value.size(), 0) != 0) return false;
#endif
        offset = static_cast<std::size_t>(end - names.begin()) + 1;
    }
    return true;
}
#endif

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
                              std::string& error, bool eagerIndex) {
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
    project.m_autoRefresh = eagerIndex;
    if (eagerIndex) project.refresh();
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
                        std::string& error, const std::string* expected) {
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

    const auto unchanged = [&]() {
        if (!expected) return true;
        std::string current, readError;
        if (!readFile(relative, current, readError) || current != *expected) {
            error = "File changed or became unavailable during save; the existing file was preserved";
            return false;
        }
        return true;
    };

#if defined(_WIN32)
    // Create a unique sibling so replacing the destination stays on one volume.
    const DWORD originalAttributes = GetFileAttributesW(path.c_str());
    const bool hadOriginal = originalAttributes != INVALID_FILE_ATTRIBUTES;
    if (!hadOriginal && GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_PATH_NOT_FOUND) { error = pathError("Could not inspect file", path); return false; }
    wchar_t name[MAX_PATH]{};
    if (!GetTempFileNameW(path.parent_path().c_str(), L"lcd", 0, name)) {
        error = pathError("Could not create temporary file", path); return false;
    }
    const std::filesystem::path temporary(name);
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    bool ok = file != INVALID_HANDLE_VALUE;
    std::size_t offset = 0;
    while (ok && offset < content.size()) {
        DWORD written = 0;
        const DWORD count = static_cast<DWORD>(std::min<std::size_t>(content.size() - offset, 1024U * 1024U));
        ok = WriteFile(file, content.data() + offset, count, &written, nullptr) && written != 0;
        offset += written;
    }
    if (ok) ok = FlushFileBuffers(file) != 0;
    if (file != INVALID_HANDLE_VALUE && !CloseHandle(file)) ok = false;
    if (ok) ok = unchanged();
    if (ok) ok = hadOriginal ? ReplaceFileW(path.c_str(), temporary.c_str(), nullptr, 0, nullptr, nullptr) != 0
                             : MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) { std::filesystem::remove(temporary, filesystemError); if (error.empty()) error = pathError("Could not safely save file", path); return false; }
#else
    std::string pattern = (path.parent_path() / ".lucid-save-XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end()); name.push_back('\0');
    const int fd = mkstemp(name.data());
    if (fd < 0) { error = pathError("Could not create temporary file", path); return false; }
    const std::filesystem::path temporary(name.data());
    struct stat existing{};
    bool ok = true;
    const int source = ::open(path.c_str(), O_RDONLY | O_CLOEXEC
#if defined(O_NOFOLLOW)
                              | O_NOFOLLOW
#endif
    );
    const bool hasOriginal = source >= 0;
    if (hasOriginal) {
        ok = ::fstat(source, &existing) == 0 && S_ISREG(existing.st_mode);
        if (ok && existing.st_nlink > 1) { error = "Cannot safely replace a hard-linked file; save a separate copy first"; ok = false; }
    } else if (errno != ENOENT) ok = false;
    std::size_t offset = 0;
    while (ok && offset < content.size()) {
        const auto written = ::write(fd, content.data() + offset, content.size() - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) { ok = false; break; }
        offset += static_cast<std::size_t>(written);
    }
    if (ok && hasOriginal) {
        struct stat target{};
        ok = ::fstat(fd, &target) == 0;
        if (ok && (target.st_uid != existing.st_uid || target.st_gid != existing.st_gid)) ok = ::fchown(fd, existing.st_uid, existing.st_gid) == 0;
        if (ok) ok = ::fchmod(fd, existing.st_mode & 07777) == 0;
#if defined(__linux__) || defined(__APPLE__)
        if (ok) ok = copyExtendedAttributes(source, fd);
#endif
        if (!ok && error.empty()) error = "Could not preserve file permissions or metadata; the original was not replaced";
    }
    if (source >= 0) ::close(source);
    if (ok) ok = ::fsync(fd) == 0;
    if (::close(fd) != 0) ok = false;
    if (ok) ok = unchanged();
    if (ok) {
        struct stat current{};
        const int found = ::lstat(path.c_str(), &current);
        if ((hasOriginal && (found != 0 || !S_ISREG(current.st_mode) || current.st_dev != existing.st_dev || current.st_ino != existing.st_ino || current.st_nlink != existing.st_nlink)) ||
            (!hasOriginal && (found == 0 || errno != ENOENT))) {
            error = "File identity changed during save; the existing file was preserved"; ok = false;
        }
    }
    if (ok) { std::filesystem::rename(temporary, path, filesystemError); ok = !filesystemError; }
    if (!ok) { std::filesystem::remove(temporary, filesystemError); if (error.empty()) error = pathError("Could not safely save file", path); return false; }
    // The replacement is already committed at this point; syncing the parent
    // directory improves crash durability where the filesystem supports it.
    const int directoryFd = ::open(path.parent_path().c_str(), O_RDONLY | O_CLOEXEC);
    if (directoryFd >= 0) { (void)::fsync(directoryFd); (void)::close(directoryFd); }
#endif
    return true;
}

bool Project::createFile(std::string_view relative, std::string& error, bool allowExisting) {
    error.clear();

    const std::filesystem::path path = resolve(relative, error);
    if (!error.empty()) {
        return false;
    }

    std::error_code filesystemError;
    if (std::filesystem::exists(path, filesystemError) && !filesystemError) {
        if (!allowExisting) { error = pathError("File already exists", path); return false; }
        if (std::filesystem::is_regular_file(path, filesystemError) && !filesystemError) {
            if (m_autoRefresh) refresh();
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

#if defined(_WIN32)
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { error = pathError("Could not create file; it may already exist", path); return false; }
    if (!CloseHandle(file)) { error = pathError("Could not finish creating file", path); return false; }
#else
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd < 0) { error = pathError("Could not create file; it may already exist", path); return false; }
    if (::close(fd) != 0) { error = pathError("Could not finish creating file", path); return false; }
#endif

    if (m_autoRefresh) refresh();
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

    if (m_autoRefresh) refresh();
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
