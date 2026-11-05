#include "workspace/git/GitReview.hpp"
#include "modules/SecretScan.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
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
    if (cmd("branch branch-test")) return 45;
    GitBranches branches; std::string branchError, branchOutput;
    if (!gitBranches(root, branches, branchError)) return 46;
    const auto selected = std::find_if(branches.branches.begin(), branches.branches.end(), [](const auto& branch) { return branch.name == "branch-test"; });
    if (selected == branches.branches.end() || selected->current) return 47;
    const auto target = *selected;
    { std::ofstream file(root / "untracked-change"); file << "uncommitted"; }
    if (gitSwitchBranch(root, branches, target, branchOutput, branchError) || branchError.find("repository changes") == std::string::npos) return 48;
    // A project can be a subdirectory of a repository: changes outside that
    // project must still prevent a checkout of the whole working tree.
    if (gitSwitchBranch(root / "nested", branches, target, branchOutput, branchError) || branchError.find("repository changes") == std::string::npos) return 57;
    std::filesystem::remove(root / "untracked-change");
    { std::ofstream file(root / "new-commit"); file << "new"; }
    if (cmd("add new-commit") || cmd("commit -qm second")) return 49;
    if (gitSwitchBranch(root, branches, target, branchOutput, branchError) || branchError.find("changed") == std::string::npos) return 50;
    if (!gitBranches(root, branches, branchError) || cmd("branch -f branch-test HEAD")) return 54;
    if (gitSwitchBranch(root, branches, target, branchOutput, branchError) || branchError.find("changed") == std::string::npos) return 55;
    if (cmd("branch -f branch-test HEAD^")) return 56;
    if (!gitBranches(root, branches, branchError) || !gitSwitchBranch(root, branches, target, branchOutput, branchError)) return 51;
    GitBranches switched;
    if (!gitBranches(root, switched, branchError) || std::none_of(switched.branches.begin(), switched.branches.end(), [](const auto& branch) { return branch.name == "branch-test" && branch.current; })) return 52;
    if (cmd("switch -q -")) return 53;
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
    std::string stagedBlob; bool blobPresent = false;
    if (!gitReadReviewedBlob(root / "nested", preview, "a file.txt", stagedBlob, blobPresent, error) || !blobPresent || stagedBlob != "one\nchanged\n++added\n") { std::fprintf(stderr, "staged read: %s, present=%d, bytes=%zu\n", error.c_str(), blobPresent, stagedBlob.size()); return 58; }
    if (gitReadReviewedBlob(root / "nested", preview, "../outside.txt", stagedBlob, blobPresent, error)) return 59;
    { std::ofstream file(root / "nested/a file.txt"); file << "ghp_" << std::string(36, 'A') << "\n" << "glpat-" << std::string(20, 'B') << "\n"; }
    if (cmd("add 'nested/a file.txt'") || !gitCommitPreview(root / "nested", preview, error)) return 60;
    { std::ofstream file(root / "nested/a file.txt"); file << "safe working tree\n"; }
    const auto stagedScan = lucid::modules::scanStagedSecrets(root / "nested", preview);
    if (stagedScan.incomplete || stagedScan.findings.size() != 2 || stagedScan.findings[0].kind != "GitHub token" || stagedScan.findings[1].kind != "GitLab access token") return 61;
    if (cmd("add 'nested/a file.txt'")) return 62;
    if (!lucid::modules::scanStagedSecrets(root / "nested", preview).incomplete) return 63;
    GitCommitPreview cleanPreview;
    if (!gitCommitPreview(root / "nested", cleanPreview, error)) return 65;
    const auto cleanScan = lucid::modules::scanStagedSecrets(root / "nested", cleanPreview);
    if (cleanScan.incomplete || !cleanScan.findings.empty()) return 66;
    { std::ofstream file(root / "nested/a file.txt"); file << "one\nchanged\n++added\n"; }
    if (cmd("add 'nested/a file.txt'") || !gitCommitPreview(root / "nested", preview, error)) return 64;
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
    { std::ofstream hook(emptyRoot / "hooks/pre-commit"); hook << "#!/bin/sh\ndd if=/dev/zero bs=2200000 count=1 2>/dev/null | tr '\\000' x\n"; }
    if (emptyCommand("config core.hooksPath hooks")) return 35;
    if (!gitCommit(emptyRoot, preview, "Initial commit", commitOutput, error)) {
        std::fprintf(stderr, "large hook commit failed: %s\n", error.c_str());
        return 35;
    }
    if (commitOutput.find("[Git output truncated after 2 MiB]") == std::string::npos) {
        std::fprintf(stderr, "large hook output was not truncated: %zu bytes\n", commitOutput.size());
        return 35;
    }
    if (emptyCommand("log -1 --format=%s | grep -q 'Initial commit'")) return 35;
    std::vector<GitFileHistoryEntry> fileHistory;
    if (!gitFileHistory(emptyRoot, "first.txt", fileHistory, error) || fileHistory.size() != 1 ||
        fileHistory[0].subject != "Initial commit" || fileHistory[0].author != "Alice") return 67;
    if (!gitHistoricalFileDiff(emptyRoot, "first.txt", fileHistory[0].hash, review, error) ||
        std::none_of(review.lines.begin(), review.lines.end(), [](const auto& line) { return line.text == "+first" && line.line == 0; })) return 68;
    if (gitFileHistory(emptyRoot, "../first.txt", fileHistory, error) ||
        gitHistoricalFileDiff(emptyRoot, "first.txt", "HEAD", review, error)) return 69;
    std::vector<GitRangeHistoryEntry> rangeHistory;
    if (!gitRangeHistory(emptyRoot, "first.txt", "first", 1, 1, rangeHistory, error) || rangeHistory.size() != 1 ||
        rangeHistory[0].subject != "Initial commit" || std::none_of(rangeHistory[0].review.lines.begin(), rangeHistory[0].review.lines.end(), [](const auto& line) { return line.text == "+first" && line.line == 0; })) return 77;
    if (gitRangeHistory(emptyRoot, "first.txt", "changed", 1, 1, rangeHistory, error) || !rangeHistory.empty() || error.find("commit") == std::string::npos ||
        gitRangeHistory(emptyRoot, "../first.txt", "first", 1, 1, rangeHistory, error) ||
        gitRangeHistory(emptyRoot, "first.txt", "first", 0, 1, rangeHistory, error)) return 78;
    if (emptyCommand("mv first.txt renamed.txt") || emptyCommand("commit -qm Rename")) return 70;
    if (!gitFileHistory(emptyRoot, "renamed.txt", fileHistory, error) || fileHistory.size() != 2 ||
        fileHistory[0].path != "renamed.txt" || fileHistory[0].previousPath != "first.txt" ||
        fileHistory[1].path != "first.txt" || fileHistory[1].subject != "Initial commit") return 71;
    if (!gitHistoricalFileDiff(emptyRoot, fileHistory[1].path, fileHistory[1].hash, review, error) ||
        std::none_of(review.lines.begin(), review.lines.end(), [](const auto& line) { return line.text == "+first"; })) return 72;
    if (!gitHistoricalFileDiff(emptyRoot, fileHistory[0].path, fileHistory[0].hash, review, error, fileHistory[0].previousPath) ||
        std::none_of(review.lines.begin(), review.lines.end(), [](const auto& line) { return line.text == "rename from first.txt"; })) return 73;
    std::filesystem::create_directories(emptyRoot / "sub");
    if (emptyCommand("mv renamed.txt sub/inside.txt") || emptyCommand("commit -qm boundary")) return 74;
    bool stoppedAtBoundary = false;
    if (!gitFileHistory(emptyRoot / "sub", "inside.txt", fileHistory, error, &stoppedAtBoundary) || !stoppedAtBoundary || fileHistory.size() != 1 ||
        fileHistory[0].path != "inside.txt" || !fileHistory[0].previousPath.empty()) return 75;
    if (gitHistoricalFileDiff(emptyRoot / "sub", "inside.txt", fileHistory[0].hash, review, error, "../renamed.txt")) return 76;
    if (gitRangeHistory(emptyRoot / "sub", "inside.txt", "first", 1, 1, rangeHistory, error) || error.find("repository root") == std::string::npos) return 79;
    std::filesystem::remove_all(root); return 0;
#endif
}
