#include "workspace/recovery/Snapshot.hpp"
#include "workspace/recovery/History.hpp"
#include "workspace/Project.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
using namespace lucid::workspace::recovery;
int main() {
    const auto dir = std::filesystem::temp_directory_path() / ("lucid-recovery-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto path = dir / "one.json";
    Snapshot original; original.project = "/project"; original.file = "src/a.cpp"; original.baseline = "hello\n"; original.contents = "new \"code\"\n🙂"; original.line = 2; original.column = 3; original.firstLine = 1;
    original.openFiles.push_back({"src/b.cpp", 5, 4, 2});
    std::string error; Snapshot restored;
    if (!write(path, original, error) || !read(path, restored, error) || !(restored == original)) return 1;
    auto invalid = original; invalid.openFiles[0].path.clear();
    if (write(path, invalid, error) || error.empty() || !read(path, restored, error) || !(restored == original)) return 10;
    invalid = original; invalid.line = 100000001;
    if (write(path, invalid, error) || error.empty() || !read(path, restored, error) || !(restored == original)) return 11;
    original.contents.assign(lucid::workspace::Project::maxFileSize + 1, 'x');
    if (!write(path, original, error) || !read(path, restored, error) || restored.contents != original.contents) return 9;
    original.baseline.assign(5U * 1024U * 1024U, '\t'); original.contents = original.baseline;
    original.contents.back() = '\n';
    if (!write(path, original, error) || std::filesystem::file_size(path) <= lucid::workspace::Project::maxFileSize ||
        std::filesystem::file_size(path) > 22U * 1024U * 1024U || !read(path, restored, error) || !(restored == original)) return 12;
    original.contents.back() = '\r';
    if (!write(path, original, error) || !read(path, restored, error) || !(restored == original)) return 13;
    original.baseline = "hello\n";
    original.contents = "second";
    if (!write(path, original, error) || !read(path, restored, error) || !(restored == original)) return 2;
    { std::ofstream f(path, std::ios::trunc); f << "{bad"; }
    if (read(path, restored, error) || error.empty()) return 3;
    for (int i = 0; i < 4; ++i) {
        original.contents = original.baseline = "version " + std::to_string(i);
        if (!recordHistory(dir, original, error)) return 4;
    }
    std::vector<std::filesystem::path> history;
    if (!listHistory(dir, original.project, history, error) || history.size() != 3) return 5;
    bool newest = false;
    for (const auto& version : history) {
        if (!read(version, restored, error) || restored.project != original.project) return 6;
        if (restored.contents == "version 3") newest = true;
    }
    if (!newest) return 8;
    std::vector<std::filesystem::path> other;
    if (!listHistory(dir, "/another-project", other, error) || !other.empty()) return 7;
    // Clock skew must not cause the snapshot just written to be pruned, and
    // unknown or unrelated files in the history folder must be preserved.
    const auto folder=history.front().parent_path();
    for (const auto& old:history) std::filesystem::last_write_time(old,std::filesystem::file_time_type::clock::now()+std::chrono::hours(24));
    const auto corrupt=folder/"unrecognized.json"; {std::ofstream out(corrupt);out<<"{broken";}
    auto unrelated=original; unrelated.file="other.cpp";
    const auto foreign=folder/"foreign.json"; if(!write(foreign,unrelated,error)) return 14;
    original.contents=original.baseline="clock-skew-new";
    if(!recordHistory(dir,original,error) || !std::filesystem::exists(corrupt) || !std::filesystem::exists(foreign)) return 15;
    history.clear(); if(!listHistory(dir,original.project,history,error)) return 16;
    bool skewNew=false; for(const auto& item:history) if(read(item,restored,error) && restored.file==original.file && restored.contents=="clock-skew-new") skewNew=true;
    if(!skewNew) return 17;
    std::filesystem::remove_all(dir); return 0;
}
