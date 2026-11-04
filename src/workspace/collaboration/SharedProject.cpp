#include "workspace/collaboration/SharedProject.hpp"
#include <algorithm>
namespace lucid::workspace::collaboration {
void SharedProject::releaseCleanFile(std::string_view path) {
    const auto file = m_files.find(std::string(path));
    if (file == m_files.end() || !file->second.document->participants().empty() ||
        file->second.document->text() != file->second.baseline) return;
    m_liveBytes -= file->second.document->text().size();
    m_files.erase(file);
}
bool SharedProject::join(std::string id, std::string name, std::string& error) {
    SharedDocument validation;
    if (m_users.size() >= 64 || !validation.join(id, name) || m_users.contains(id)) { error = "Invalid or duplicate participant"; return false; }
    m_users.emplace(std::move(id), User{std::move(name), {}}); error.clear(); return true;
}
void SharedProject::leave(std::string_view id) {
    const auto user = m_users.find(std::string(id)); if (user == m_users.end()) return;
    if (auto file = m_files.find(user->second.file); file != m_files.end()) file->second.document->leave(id);
    releaseCleanFile(user->second.file);
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
        // A clean file occupied only by this user will be released when the
        // switch succeeds. Count that space without closing it prematurely.
        const auto previous = m_files.find(user->second.file);
        const bool reclaimPrevious = previous != m_files.end() &&
            previous->second.document->participants().size() == 1 &&
            previous->second.document->participants().contains(std::string(id)) &&
            previous->second.document->text() == previous->second.baseline;
        const auto reclaimBytes = reclaimPrevious ? previous->second.document->text().size() : 0;
        if (m_files.size() >= 32 && !reclaimPrevious) { error = "Too many shared files are open"; return false; }
        std::string contents;
        if (!m_project.readFile(key, contents, error)) return false;
        if (contents.size() > 1024U * 1024U) { error = "Shared files are limited to 1 MiB"; return false; }
        if (contents.size() > 32U * 1024U * 1024U - (m_liveBytes - reclaimBytes)) { error = "Shared project memory limit reached"; return false; }
        if (contents.find('\0') != std::string::npos) { error = "Binary files cannot be edited in a shared session"; return false; }
        auto document = std::make_unique<SharedDocument>(contents);
        if (!document->valid()) { error = "File is not valid UTF-8 or exceeds the collaboration limit"; return false; }
        m_liveBytes += contents.size();
        file = m_files.emplace(key, File{std::move(document), std::move(contents)}).first;
    }
    if (user->second.file != key) {
        if (!file->second.document->join(std::string(id), user->second.name)) { error = "Could not join shared file"; return false; }
        if (auto previous = m_files.find(user->second.file); previous != m_files.end()) previous->second.document->leave(id);
        releaseCleanFile(user->second.file);
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
    if (change.insert.find('\0') != std::string::npos) { error = "Binary content cannot be inserted in a shared file"; return false; }
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
    if (contents != disk && !m_project.writeFile(path, contents, error, &disk)) return false;
    file->second.baseline = contents;
    releaseCleanFile(path);
    error.clear(); return true;
}
}
