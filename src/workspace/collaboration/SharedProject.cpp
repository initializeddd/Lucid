#include "workspace/collaboration/SharedProject.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#else
#define NOMINMAX
#include <windows.h>
#include "workspace/collaboration/ShareLink.hpp"
#endif
namespace lucid::workspace::collaboration {
namespace {
bool atomicSave(Project& project, std::string_view relative, std::string_view expected, std::string_view contents, std::string& error) {
    const auto target = project.resolve(relative, error); if (!error.empty()) return false;
    std::filesystem::path temporary;
#if defined(_WIN32)
    std::string token; if (!newShareToken(token, error)) return false;
    temporary = target.parent_path() / (".lucid-save-" + token + ".tmp");
    HANDLE handle = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) { error = "Could not create temporary shared file"; return false; }
    bool written = true; std::size_t offset = 0;
    while (offset < contents.size()) {
        DWORD count = 0; const auto chunk = static_cast<DWORD>(std::min<std::size_t>(contents.size() - offset, 65536));
        if (!WriteFile(handle, contents.data() + offset, chunk, &count, nullptr) || !count) { written = false; break; }
        offset += count;
    }
    if (!FlushFileBuffers(handle)) written = false;
    if (!CloseHandle(handle)) written = false;
#else
    struct stat metadata{};
    if (stat(target.c_str(), &metadata) != 0 || !S_ISREG(metadata.st_mode) || metadata.st_nlink != 1) { error = "Cannot safely replace shared file (missing, non-regular, or hard-linked)"; return false; }
    std::string pattern = (target.parent_path() / ".lucid-save-XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end()); name.push_back('\0');
    const int fd = mkstemp(name.data()); if (fd < 0) { error = "Could not create temporary shared file"; return false; }
    temporary = name.data(); bool written = fchmod(fd, metadata.st_mode & 0777) == 0; std::size_t offset = 0;
    while (written && offset < contents.size()) {
        const auto n = ::write(fd, contents.data() + offset, contents.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { written = false; break; }
        offset += static_cast<std::size_t>(n);
    }
    if (written && fsync(fd) != 0) written = false;
    if (::close(fd) != 0) written = false;
#endif
    std::error_code ec;
    if (!written) { std::filesystem::remove(temporary, ec); error = "Could not write temporary shared file"; return false; }
    std::string current;
    if (!project.readFile(relative, current, error) || current != expected) {
        std::filesystem::remove(temporary, ec); error = "Shared file changed outside the session; changes remain in memory"; return false;
    }
#if defined(_WIN32)
    if (!ReplaceFileW(target.c_str(), temporary.c_str(), nullptr, REPLACEFILE_WRITE_THROUGH, nullptr, nullptr)) {
        std::filesystem::remove(temporary, ec); error = "Could not replace shared file"; return false;
    }
#else
    std::filesystem::rename(temporary, target, ec);
    if (ec) { const auto message = ec.message(); std::filesystem::remove(temporary, ec); error = "Could not replace shared file: " + message; return false; }
#endif
    error.clear(); return true;
}
}

bool SharedProject::join(std::string id, std::string name, std::string& error) {
    SharedDocument validation;
    if (m_users.size() >= 64 || !validation.join(id, name) || m_users.contains(id)) { error = "Invalid or duplicate participant"; return false; }
    m_users.emplace(std::move(id), User{std::move(name), {}}); error.clear(); return true;
}
void SharedProject::leave(std::string_view id) {
    const auto user = m_users.find(std::string(id)); if (user == m_users.end()) return;
    if (auto file = m_files.find(user->second.file); file != m_files.end()) file->second.document->leave(id);
    m_users.erase(user);
}
bool SharedProject::open(std::string_view id, std::string_view path, std::string& canonicalPath, std::string& error) {
    auto user = m_users.find(std::string(id)); if (user == m_users.end()) { error = "Participant is not in this session"; return false; }
    const auto resolved = m_project.resolve(path, error); if (!error.empty()) return false;
    const auto relative = resolved.lexically_relative(m_project.root());
    if (relative.empty() || *relative.begin() == "..") { error = "File is outside the project"; return false; }
    const std::string key = relative.generic_string();
    auto file = m_files.find(key);
    if (file == m_files.end()) {
        if (m_files.size() >= 32) { error = "Too many shared files are open"; return false; }
        std::string contents;
        if (!m_project.readFile(key, contents, error)) return false;
        if (contents.size() > 1024U * 1024U) { error = "Shared files are limited to 1 MiB"; return false; }
        if (contents.size() > 32U * 1024U * 1024U - m_liveBytes) { error = "Shared project memory limit reached"; return false; }
        if (contents.find('\0') != std::string::npos) { error = "Binary files cannot be edited in a shared session"; return false; }
        auto document = std::make_unique<SharedDocument>(contents);
        if (!document->valid()) { error = "File is not valid UTF-8 or exceeds the collaboration limit"; return false; }
        m_liveBytes += contents.size();
        file = m_files.emplace(key, File{std::move(document), std::move(contents)}).first;
    }
    if (user->second.file != key) {
        if (!file->second.document->join(std::string(id), user->second.name)) { error = "Could not join shared file"; return false; }
        if (auto previous = m_files.find(user->second.file); previous != m_files.end()) previous->second.document->leave(id);
        user->second.file = key;
    }
    canonicalPath = key; error.clear(); return true;
}
const SharedDocument* SharedProject::document(std::string_view path) const {
    const auto file = m_files.find(std::string(path)); return file == m_files.end() ? nullptr : file->second.document.get();
}
bool SharedProject::moveCursor(std::string_view id, Cursor cursor, std::string& error) {
    const auto user = m_users.find(std::string(id));
    if (user == m_users.end()) { error = "Participant is not in this session"; return false; }
    const auto file = m_files.find(user->second.file);
    if (file == m_files.end() || !file->second.document->moveCursor(id, cursor)) { error = "Invalid cursor or no shared file open"; return false; }
    error.clear(); return true;
}
bool SharedProject::edit(std::string_view id, const Edit& change, Edit& applied, std::string& error) {
    const auto user = m_users.find(std::string(id));
    if (user == m_users.end()) { error = "Participant is not in this session"; return false; }
    const auto file = m_files.find(user->second.file);
    if (file == m_files.end()) { error = "No shared file open"; return false; }
    const auto before = file->second.document->text().size();
    const auto growth = change.insert.size() > change.eraseBytes ? change.insert.size() - change.eraseBytes : 0;
    if (growth > 1024U * 1024U - before) { error = "Shared files are limited to 1 MiB"; return false; }
    if (growth > 32U * 1024U * 1024U - m_liveBytes) { error = "Shared project memory limit reached"; return false; }
    if (!file->second.document->applyRebased(id, change, applied, error)) return false;
    m_liveBytes = m_liveBytes - before + file->second.document->text().size();
    return true;
}
bool SharedProject::save(std::string_view path, std::string& error) {
    auto file = m_files.find(std::string(path)); if (file == m_files.end()) { error = "Shared file is not open"; return false; }
    std::string disk; if (!m_project.readFile(path, disk, error)) { error = "Shared file is missing or unreadable; changes remain in memory"; return false; }
    if (disk != file->second.baseline) { error = "Shared file changed outside the session; changes remain in memory"; return false; }
    const auto contents = file->second.document->text();
    if (contents != disk && !atomicSave(m_project, path, disk, contents, error)) return false;
    file->second.baseline = contents; error.clear(); return true;
}
}
