#include "workspace/recovery/History.hpp"
#include "workspace/tasks/Tasks.hpp"
#include <picosha2.h>
#include <algorithm>
#include <system_error>
namespace lucid::workspace::recovery {
bool recordHistory(const std::filesystem::path& root, const Snapshot& snapshot, std::string& error) {
    const auto key = picosha2::hash256_hex_string(snapshot.project + '\0' + snapshot.file);
    const auto folder = root / "History" / key;
    const auto saved = folder / (workspace::newTaskId() + ".json");
    if (!write(saved, snapshot, error)) return false;
    struct Entry { std::filesystem::path path; std::filesystem::file_time_type time; };
    std::error_code ec; std::vector<Entry> entries; std::size_t inspected=0;
    for (std::filesystem::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
        if (++inspected>256) return true; // Defer pruning unusually large directories.
        if (it->path() == saved || it->is_symlink(ec) || !it->is_regular_file(ec) || it->path().extension() != ".json") continue;
        // Never delete an unreadable or unrelated file simply because it is in
        // the history directory. Preserve uncertain entries for manual recovery.
        Snapshot candidate; std::string readError;
        if (!read(it->path(), candidate, readError) || candidate.project != snapshot.project || candidate.file != snapshot.file) continue;
        const auto time=it->last_write_time(ec); if (ec) break;
        entries.push_back({it->path(),time});
    }
    if (ec) return true; // The new backup is already safe; pruning can wait.
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        return a.time == b.time ? a.path > b.path : a.time > b.time;
    });
    // The newly written snapshot always occupies one of the three slots,
    // even when a filesystem clock moves backwards.
    for (std::size_t i = 2; i < entries.size(); ++i) { std::filesystem::remove(entries[i].path, ec); ec.clear(); }
    error.clear(); return true;
}
bool listHistory(const std::filesystem::path& root, std::string_view project,
                 std::vector<std::filesystem::path>& paths, std::string& error) {
    std::error_code ec; const auto folder = root / "History";
    if (!std::filesystem::exists(folder, ec)) { if (ec) { error = ec.message(); return false; } return true; }
    for (std::filesystem::directory_iterator dir(folder, ec), end; !ec && dir != end; dir.increment(ec)) {
        if (dir->is_symlink(ec) || !dir->is_directory(ec)) continue;
        for (std::filesystem::directory_iterator file(dir->path(), ec), last; !ec && file != last; file.increment(ec)) {
            if (file->is_symlink(ec) || !file->is_regular_file(ec) || file->path().extension() != ".json") continue;
            Snapshot snapshot; std::string readError;
            if (read(file->path(), snapshot, readError) && snapshot.project == project) paths.push_back(file->path());
        }
    }
    if (ec) { error = "Could not list saved versions: " + ec.message(); return false; }
    error.clear(); return true;
}
}
