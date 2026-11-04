#include "workspace/git/GitReview.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
using namespace lucid::workspace;
int main() {
#if defined(_WIN32)
    return 0;
#else
    const auto root = std::filesystem::temp_directory_path() / ("lucid-git-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root / "nested");
    const auto cmd = [&](const std::string& arguments) { return std::system(("git -C '" + root.string() + "' " + arguments + " >/dev/null 2>&1").c_str()); };
    if (cmd("init -q") || cmd("config user.email test@example.com") || cmd("config user.name Alice") || cmd("config commit.gpgsign false") || cmd("config core.hooksPath /dev/null")) return 1;
    { std::ofstream f(root / "nested/a file.txt"); f << "one\ntwo\n--old\n"; }
    if (cmd("add .") || cmd("commit -qm initial")) return 2;
    { std::ofstream f(root / "nested/a file.txt"); f << "one\nchanged\n++added\n"; }
    { std::ofstream f(root / "nested/untracked.txt"); f << "new\n"; }
    std::vector<GitChange> changes; std::string error;
    if (!gitChanges(root / "nested", changes, error) || changes.size() != 2) return 3;
    auto found = std::find_if(changes.begin(), changes.end(), [](const auto& c) { return c.path == "a file.txt"; });
    if (found == changes.end()) return 4;
    GitReview review; if (!gitReview(root / "nested", *found, review, error)) return 5;
    if (std::none_of(review.lines.begin(), review.lines.end(), [](const auto& line) { return line.text == "+changed" && line.line == 2; })) return 6;
    if (std::none_of(review.lines.begin(), review.lines.end(), [](const auto& line) { return line.text == "+++added" && line.line == 3; }) ||
        std::none_of(review.lines.begin(), review.lines.end(), [](const auto& line) { return line.text == "---old"; })) return 38;
    std::string author; if (!gitBlame(root / "nested", "a file.txt", 1, author, error) || author.find("Alice") == std::string::npos) return 7;
    if (!gitBlame(root / "nested", "a file.txt", 2, author, error) || author.find("Uncommitted change") == std::string::npos) return 8;
    const GitChange modified = *found;
    if (gitStage(root / "nested", GitChange{"../outside.txt", "??", false, {}}, error)) return 37;
    if (!gitStage(root / "nested", modified, error)) return 15;
    { std::ofstream f(root / "nested/a file.txt"); f << "inserted\none\nchanged\n++added\n"; }
    if (!gitChanges(root / "nested", changes, error)) return 42;
    found = std::find_if(changes.begin(), changes.end(), [](const auto& c) { return c.path == "a file.txt"; });
    if (found == changes.end() || !gitReview(root / "nested", *found, review, error)) return 43;
    const auto stagedLine = std::find_if(review.lines.begin(), review.lines.end(), [](const auto& line) { return line.text == "+changed"; });
    const auto workLine = std::find_if(review.lines.begin(), review.lines.end(), [](const auto& line) { return line.text == "+inserted"; });
    if (stagedLine == review.lines.end() || stagedLine->line != 0 || workLine == review.lines.end() || workLine->line != 1 ||
        std::none_of(review.lines.begin(), review.lines.end(), [](const auto& line) { return line.text.find("line navigation unavailable") != std::string::npos; })) return 44;
    GitCommitPreview preview;
    if (!gitCommitPreview(root / "nested", preview, error) || preview.paths.size() != 1 || preview.paths[0] != "a file.txt") return 16;
    { std::ofstream f(root / "nested/a file.txt"); f << "one\nnewer\n"; }
    if (!gitStage(root / "nested", modified, error)) return 17;
    std::string commitOutput;
    if (gitCommit(root / "nested", preview, "must not commit", commitOutput, error) || error.empty()) return 18;
    if (!gitUnstage(root / "nested", modified, error)) return 19;
    { std::ofstream f(root / "outside.txt"); f << "outside"; }
    if (cmd("add outside.txt") || gitCommitPreview(root / "nested", preview, error) || error.find("outside") == std::string::npos || cmd("reset -q HEAD -- outside.txt")) return 24;
    std::filesystem::remove(root / "outside.txt");
    { std::ofstream f(root / "nested/a file.txt"); f << "one\nchanged\n"; }
    if (cmd("checkout -qb other") || cmd("add .") || cmd("commit -qm other") || cmd("checkout -q -") ) return 9;
    { std::ofstream f(root / "nested/a file.txt"); f << "one\nconflict\n"; }
    if (cmd("add .") || cmd("commit -qm main")) return 10;
    if (cmd("merge other") == 0) return 11;
    if (!gitChanges(root / "nested", changes, error)) return 12;
    found = std::find_if(changes.begin(), changes.end(), [](const auto& c) { return c.path == "a file.txt"; });
    if (found == changes.end() || !found->conflict || !gitReview(root / "nested", *found, review, error)) return 13;
    if (std::none_of(review.lines.begin(), review.lines.end(), [](const auto& line) { return line.text.rfind("Conflict: <<<<<<<", 0) == 0 && line.line > 0; })) return 14;
    if (gitStage(root / "nested", *found, error)) return 20;
    { std::ofstream f(root / "nested/a file.txt", std::ios::trunc); f << "one\r\n=======\r\n"; }
    if (gitStage(root / "nested", *found, error)) return 36;
    { std::ofstream f(root / "nested/a file.txt", std::ios::trunc); f << "one\nresolved\n"; }
    if (!gitStage(root / "nested", *found, error) || !gitCommitPreview(root / "nested", preview, error)) return 21;
    if (!gitCommit(root / "nested", preview, "Resolve conflict", commitOutput, error)) return 22;
    if (!gitChanges(root / "nested", changes, error) || !changes.empty()) return 23;
    if (cmd("mv 'nested/a file.txt' 'nested/renamed.txt'")) return 25;
    if (!gitChanges(root / "nested", changes, error) || changes.size() != 1 || changes[0].oldPath != "a file.txt") return 26;
    if (!gitReview(root / "nested", changes[0], review, error) ||
        std::none_of(review.lines.begin(), review.lines.end(), [](const auto& line) { return line.text.find("rename from ") == 0; }) ||
        std::none_of(review.lines.begin(), review.lines.end(), [](const auto& line) { return line.text.find("rename to ") == 0; })) return 39;
    if (!gitUnstage(root / "nested", changes[0], error)) return 27;
    if (cmd("reset --hard -q HEAD") || cmd("clean -fdq")) return 28;
    { std::ofstream f(root / "nested/binary.dat", std::ios::binary); f << std::string("a\0b", 3); }
    if (cmd("add nested/binary.dat") || cmd("commit -qm binary")) return 40;
    { std::ofstream f(root / "nested/binary.dat", std::ios::binary | std::ios::trunc); f << std::string("a\0c", 3); }
    if (!gitChanges(root / "nested", changes, error) || changes.size() != 1 ||
        !gitReview(root / "nested", changes[0], review, error) ||
        std::none_of(review.lines.begin(), review.lines.end(), [](const auto& line) { return line.text.find("Binary files ") == 0; })) return 41;
    const auto emptyRoot = root / "empty"; std::filesystem::create_directories(emptyRoot);
    const auto emptyCommand = [&](const std::string& arguments) { return std::system(("git -C '" + emptyRoot.string() + "' " + arguments + " >/dev/null 2>&1").c_str()); };
    if (emptyCommand("init -q") || emptyCommand("config user.email test@example.com") || emptyCommand("config user.name Alice") || emptyCommand("config commit.gpgsign false") || emptyCommand("config core.hooksPath /dev/null")) return 29;
    { std::ofstream f(emptyRoot / "first.txt"); f << "first"; }
    if (!gitChanges(emptyRoot, changes, error) || changes.size() != 1 || !gitStage(emptyRoot, changes[0], error)) return 30;
    if (!gitChanges(emptyRoot, changes, error) || !gitUnstage(emptyRoot, changes[0], error)) return 31;
    if (!gitChanges(emptyRoot, changes, error) || !gitStage(emptyRoot, changes[0], error) || !gitCommitPreview(emptyRoot, preview, error)) return 32;
    std::filesystem::create_directories(emptyRoot / "hooks");
    { std::ofstream hook(emptyRoot / "hooks/pre-commit"); hook << "#!/bin/sh\nexit 1\n"; }
    std::filesystem::permissions(emptyRoot / "hooks/pre-commit", std::filesystem::perms::owner_all);
    if (emptyCommand("config core.hooksPath hooks") || gitCommit(emptyRoot, preview, "Rejected commit", commitOutput, error) || error.empty()) return 33;
    GitCommitPreview afterHook;
    if (!gitCommitPreview(emptyRoot, afterHook, error) || afterHook.indexTree != preview.indexTree || emptyCommand("config core.hooksPath /dev/null")) return 34;
    { std::ofstream hook(emptyRoot / "hooks/pre-commit"); hook << "#!/bin/sh\nhead -c 2200000 /dev/zero | tr '\\000' x\n"; }
    if (emptyCommand("config core.hooksPath hooks") || !gitCommit(emptyRoot, preview, "Initial commit", commitOutput, error) ||
        commitOutput.find("[Git output truncated after 2 MiB]") == std::string::npos ||
        emptyCommand("log -1 --format=%s | grep -q 'Initial commit'")) return 35;
    std::filesystem::remove_all(root); return 0;
#endif
}
