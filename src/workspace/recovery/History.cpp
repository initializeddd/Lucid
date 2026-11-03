#include "workspace/recovery/History.hpp"
#include "workspace/tasks/Tasks.hpp"
#include <picosha2.h>
#include <algorithm>
#include <system_error>
namespace lucid::workspace::recovery {
bool recordHistory(const std::filesystem::path& root, const Snapshot& snapshot, std::string& error) {
    const auto key = picosha2::hash256_hex_string(snapshot.project + '\0' + snapshot.file);
    const auto folder = root / "History" / key;
    if (!write(folder / (workspace::newTaskId() + ".json"), snapshot, error)) return false;
    std::error_code ec; std::vector<std::filesystem::directory_entry> entries;
    for (std::filesystem::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && !it->is_symlink(ec) && it->path().extension() == ".json") entries.push_back(*it);
    }
    if (ec) return true; // The new backup is already safe; pruning can wait.
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        std::error_code ea, eb; const auto ta = a.last_write_time(ea), tb = b.last_write_time(eb);
        if (ea || eb || ta == tb) return a.path() > b.path();
        return ta > tb;
    });
    for (std::size_t i = 3; i < entries.size(); ++i) { std::filesystem::remove(entries[i].path(), ec); ec.clear(); }
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
