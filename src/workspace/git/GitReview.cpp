#include "workspace/git/GitReview.hpp"
#include <algorithm>
#include <charconv>
#include <cerrno>
#include <fstream>
#include <sstream>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif
namespace lucid::workspace {
namespace {
bool safePath(std::string_view name) {
    if (name.empty() || name.find('\0') != std::string_view::npos) return false;
    std::u8string utf8; utf8.reserve(name.size());
    for (char c : name) utf8.push_back(static_cast<char8_t>(static_cast<unsigned char>(c)));
    const auto path = std::filesystem::path(utf8);
    if (path.is_absolute() || path.has_root_name()) return false;
    for (const auto& part : path) if (part == "..") return false;
    return path.lexically_normal() != ".";
}
bool git(const std::filesystem::path& root, std::vector<std::string> args, std::string& output, std::string& error, bool noninteractive = false, bool requireCompleteOutput = true) {
#if defined(_WIN32)
    const auto wide = [](std::string_view text) {
        if (text.empty()) return std::wstring{};
        const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (size <= 0) return std::wstring{};
        std::wstring result(static_cast<std::size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size);
        return result;
    };
    const auto quoteArg = [](const std::wstring& arg) {
        std::wstring result = L"\""; std::size_t slashes = 0;
        for (wchar_t c : arg) {
            if (c == L'\\') { ++slashes; continue; }
            if (c == L'\"') { result.append(slashes * 2 + 1, L'\\'); result += c; }
            else { result.append(slashes, L'\\'); result += c; }
            slashes = 0;
        }
        result.append(slashes * 2, L'\\'); result += L'\"'; return result;
    };
    std::wstring line = L"git.exe --no-pager --literal-pathspecs -C " + quoteArg(root.wstring()) + L" -c core.quotepath=false";
    for (const auto& arg : args) { const auto converted = wide(arg); if (!arg.empty() && converted.empty()) { error = "Git argument is not valid UTF-8"; return false; } line += L" " + quoteArg(converted); }
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE}; HANDLE reader = nullptr, writer = nullptr;
    if (!CreatePipe(&reader, &writer, &security, 0)) { error = "Could not create Git output pipe"; return false; }
    SetHandleInformation(reader, HANDLE_FLAG_INHERIT, 0);
    HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (input == INVALID_HANDLE_VALUE) { CloseHandle(reader); CloseHandle(writer); error = "Could not start Git"; return false; }
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input; startup.hStdOutput = writer; startup.hStdError = writer;
    PROCESS_INFORMATION process{};
    std::vector<wchar_t> environment;
    if (noninteractive) {
        LPWCH block = GetEnvironmentStringsW();
        if (!block) { CloseHandle(reader); CloseHandle(writer); CloseHandle(input); error = "Could not prepare Git environment"; return false; }
        for (const wchar_t* item = block; *item; item += wcslen(item) + 1) {
            if (_wcsnicmp(item, L"GIT_TERMINAL_PROMPT=", 20) == 0 || _wcsnicmp(item, L"GCM_INTERACTIVE=", 16) == 0 || _wcsnicmp(item, L"GIT_SSH_COMMAND=", 16) == 0) continue;
            environment.insert(environment.end(), item, item + wcslen(item) + 1);
        }
        FreeEnvironmentStringsW(block);
        for (const auto* value : {L"GIT_TERMINAL_PROMPT=0", L"GCM_INTERACTIVE=Never", L"GIT_SSH_COMMAND=ssh -oBatchMode=yes -oConnectTimeout=15"})
            environment.insert(environment.end(), value, value + wcslen(value) + 1);
        environment.push_back(L'\0');
    }
    const BOOL launched = CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | (noninteractive ? CREATE_UNICODE_ENVIRONMENT : 0), noninteractive ? environment.data() : nullptr, nullptr, &startup, &process);
    CloseHandle(writer); CloseHandle(input);
    if (!launched) { CloseHandle(reader); error = "Could not start Git; install Git and try again"; return false; }
    output.clear(); char buffer[4096]; DWORD n = 0; bool truncated = false;
    while (ReadFile(reader, buffer, sizeof(buffer), &n, nullptr) && n > 0) {
        const auto available = 2U * 1024U * 1024U - output.size(); if (n > available) truncated = true;
        output.append(buffer, std::min<std::size_t>(n, available));
    }
    CloseHandle(reader); WaitForSingleObject(process.hProcess, INFINITE); DWORD status = 1; GetExitCodeProcess(process.hProcess, &status);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    if (truncated && !requireCompleteOutput) output += "\n[Git output truncated after 2 MiB]";
    if (truncated && requireCompleteOutput) { error = "Git output exceeds the 2 MiB review limit"; return false; }
    if (status != 0) { error = output.empty() ? "Git command failed" : output; return false; }
    error.clear(); return true;
#else
    std::vector<std::string> command{"git", "--no-pager", "--literal-pathspecs", "-C", root.string(), "-c", "core.quotepath=false"};
    command.insert(command.end(), args.begin(), args.end());
    std::vector<char*> argv; for (auto& s : command) argv.push_back(s.data()); argv.push_back(nullptr);
    int fd[2]; if (pipe(fd) != 0) { error = "Could not start Git"; return false; }
    posix_spawn_file_actions_t actions; posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, fd[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, fd[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, fd[0]); posix_spawn_file_actions_addclose(&actions, fd[1]);
    std::vector<std::string> environment; std::vector<char*> envp;
    if (noninteractive) {
        for (char** item = environ; item && *item; ++item) {
            const std::string_view entry(*item);
            if (!entry.starts_with("GIT_TERMINAL_PROMPT=") && !entry.starts_with("GCM_INTERACTIVE=") && !entry.starts_with("GIT_SSH_COMMAND=")) environment.emplace_back(entry);
        }
        environment.emplace_back("GIT_TERMINAL_PROMPT=0"); environment.emplace_back("GCM_INTERACTIVE=Never");
        environment.emplace_back("GIT_SSH_COMMAND=ssh -oBatchMode=yes -oConnectTimeout=15");
        for (auto& value : environment) envp.push_back(value.data()); envp.push_back(nullptr);
    }
    pid_t child; const int result = posix_spawnp(&child, "git", &actions, nullptr, argv.data(), noninteractive ? envp.data() : environ);
    posix_spawn_file_actions_destroy(&actions); close(fd[1]);
    if (result != 0) { close(fd[0]); error = "Could not start Git; install Git and try again"; return false; }
    output.clear(); char buffer[4096]; ssize_t n; bool truncated = false;
    while ((n = read(fd[0], buffer, sizeof(buffer))) > 0 || (n < 0 && errno == EINTR)) {
        if (n < 0) continue;
        const auto available = 2U * 1024U * 1024U - output.size();
        if (static_cast<std::size_t>(n) > available) truncated = true;
        output.append(buffer, std::min<std::size_t>(static_cast<std::size_t>(n), available));
    }
    close(fd[0]); int status = 0; while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
    if (truncated && !requireCompleteOutput) output += "\n[Git output truncated after 2 MiB]";
    if (truncated && requireCompleteOutput) { error = "Git output exceeds the 2 MiB review limit"; return false; }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) { error = output.empty() ? "Git command failed" : output; return false; }
    error.clear(); return true;
#endif
}
void appendDiff(std::string_view text, std::vector<GitReviewLine>& lines) {
    std::istringstream in{std::string(text)}; std::string line; int current = 0; bool inHunk = false;
    while (std::getline(in, line)) {
        if (line.rfind("diff --git ", 0) == 0) { inHunk = false; }
        else if (line.rfind("@@ ", 0) == 0) {
            auto plus = line.find('+'); if (plus != std::string::npos) std::from_chars(line.data() + plus + 1, line.data() + line.size(), current);
            inHunk = true;
            lines.push_back({line, current});
        } else if (inHunk && !line.empty() && line[0] == '+') {
            lines.push_back({line, current++});
        } else if (inHunk && !line.empty() && line[0] == '-') {
            lines.push_back({line, current});
        } else if (inHunk && !line.empty() && line[0] == ' ' && current > 0) { lines.push_back({line, current++}); }
        else if (!inHunk && (line.rfind("Binary files ", 0) == 0 || line.rfind("old mode ", 0) == 0 ||
                 line.rfind("new mode ", 0) == 0 || line.rfind("new file mode ", 0) == 0 ||
                 line.rfind("deleted file mode ", 0) == 0 || line.rfind("similarity index ", 0) == 0 ||
                 line.rfind("rename from ", 0) == 0 || line.rfind("rename to ", 0) == 0)) {
            lines.push_back({line, 0});
        }
    }
}
}
bool gitChanges(const std::filesystem::path& root, std::vector<GitChange>& changes, std::string& error) {
    std::string prefix, output;
    if (!git(root, {"rev-parse", "--show-prefix"}, prefix, error)) return false;
    while (!prefix.empty() && (prefix.back() == '\n' || prefix.back() == '\r')) prefix.pop_back();
    if (!git(root, {"status", "--porcelain=v1", "-z", "--untracked-files=all", "--", "."}, output, error)) return false;
    changes.clear(); std::size_t pos = 0;
    while (pos < output.size()) {
        const auto end = output.find('\0', pos); if (end == std::string::npos) { error = "Incomplete Git status output"; return false; }
        std::string item = output.substr(pos, end - pos); pos = end + 1;
        if (item.size() < 4 || item[2] != ' ') { error = "Invalid Git status output"; return false; }
        const std::string status = item.substr(0, 2); std::string path = item.substr(3); std::string oldPath;
        if (status.find('R') != std::string::npos || status.find('C') != std::string::npos) {
            const auto oldEnd = output.find('\0', pos); if (oldEnd == std::string::npos) { error = "Incomplete Git rename"; return false; } oldPath = output.substr(pos, oldEnd - pos); pos = oldEnd + 1;
        }
        if (!prefix.empty()) { if (path.rfind(prefix, 0) != 0) continue; path.erase(0, prefix.size()); if (!oldPath.empty()) { if (oldPath.rfind(prefix, 0) != 0) { error = "A rename crosses the open project boundary"; return false; } oldPath.erase(0, prefix.size()); } }
        const bool conflict = status == "DD" || status == "AU" || status == "UD" || status == "UA" || status == "DU" || status == "AA" || status == "UU";
        changes.push_back({std::move(path), status, conflict, std::move(oldPath)});
    }
    return true;
}
bool gitReview(const std::filesystem::path& root, const GitChange& change, GitReview& review, std::string& error) {
    review.lines.clear(); std::string output;
    if (!safePath(change.path) || (!change.oldPath.empty() && !safePath(change.oldPath))) { error = "Invalid project path"; return false; }
    if (change.status == "??") {
        const auto path = root / change.path;
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec || size > 2U * 1024U * 1024U) { error = "Untracked file is unavailable or exceeds the 2 MiB review limit"; return false; }
        std::ifstream file(path, std::ios::binary); if (!file) { error = "Could not read untracked file"; return false; }
        std::string content((std::istreambuf_iterator<char>(file)), {});
        if (content.find('\0') != std::string::npos) { error = "Binary files cannot be displayed as a text diff"; return false; }
        std::istringstream input(content); std::string line; int n = 1;
        while (std::getline(input, line) && n <= 10000) review.lines.push_back({"+" + line, n++});
        if (n > 10000) review.lines.push_back({"Review limited to the first 10000 lines", 0});
    } else {
        for (bool staged : {true, false}) {
            std::vector<std::string> args{"diff", "--no-ext-diff", "--no-textconv", "--no-color", "--unified=3"};
            if (staged) args.push_back("--cached");
            args.push_back("--"); args.push_back(change.path);
            if (!change.oldPath.empty()) args.push_back(change.oldPath);
            if (!git(root, std::move(args), output, error)) return false;
            if (!output.empty()) {
                // Staged line numbers describe the index, not a working file
                // that may contain additional, line-shifting edits.
                const bool stagedFileDiffers = staged && change.status.size() == 2 &&
                    change.status[1] != ' ' && change.status[1] != '?';
                review.lines.push_back({stagedFileDiffers ? "Staged changes (line navigation unavailable: working file differs)" :
                                        staged ? "Staged changes" : "Working tree changes", 0});
                const auto before = review.lines.size();
                appendDiff(output, review.lines);
                if (stagedFileDiffers) {
                    for (auto i = before; i < review.lines.size(); ++i) review.lines[i].line = 0;
                }
                if (review.lines.size() == before) review.lines.push_back({"No textual diff available", 0});
            }
        }
    }
    if (change.conflict) {
        std::ifstream file(root / change.path); std::string line; int n = 0;
        while (std::getline(file, line)) { ++n; if (line.rfind("<<<<<<< ", 0) == 0) review.lines.push_back({"Conflict: " + line, n}); }
    }
    if (review.lines.empty()) review.lines.push_back({change.conflict ? "Conflict: open file to resolve" : "No textual diff available", 0});
    return true;
}
bool gitStage(const std::filesystem::path& root, const GitChange& change, std::string& error) {
    if (!safePath(change.path) || (!change.oldPath.empty() && !safePath(change.oldPath))) { error = "Invalid project path"; return false; }
    if (change.conflict) {
        std::ifstream file(root / change.path, std::ios::binary);
        std::string line; bool marker = false;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            marker |= line.rfind("<<<<<<<", 0) == 0 || line.rfind("=======", 0) == 0 ||
                      line.rfind(">>>>>>>", 0) == 0 || line.rfind("|||||||", 0) == 0;
        }
        if (marker) { error = "Resolve conflict markers in the file before staging"; return false; }
    }
    std::vector<std::string> args{"add", "-A", "--", change.path};
    if (!change.oldPath.empty()) args.push_back(change.oldPath);
    std::string output; return git(root, std::move(args), output, error, false, false);
}
bool gitUnstage(const std::filesystem::path& root, const GitChange& change, std::string& error) {
    if (!safePath(change.path) || (!change.oldPath.empty() && !safePath(change.oldPath))) { error = "Invalid project path"; return false; }
    if (change.conflict) { error = "Cannot unstage an unresolved conflict"; return false; }
    std::string output, ignored;
    const bool hasHead = git(root, {"rev-parse", "--verify", "HEAD"}, output, ignored);
    std::vector<std::string> args = hasHead ? std::vector<std::string>{"restore", "--staged", "--"} : std::vector<std::string>{"rm", "--cached", "-r", "--ignore-unmatch", "--"};
    args.push_back(change.path); if (!change.oldPath.empty()) args.push_back(change.oldPath);
    return git(root, std::move(args), output, error, false, false);
}
bool gitCommitPreview(const std::filesystem::path& root, GitCommitPreview& preview, std::string& error) {
    preview = {}; std::string prefix, output;
    if (!git(root, {"rev-parse", "--show-prefix"}, prefix, error)) return false;
    while (!prefix.empty() && (prefix.back() == '\n' || prefix.back() == '\r')) prefix.pop_back();
    if (!git(root, {"diff", "--cached", "--name-only", "-z", "--no-ext-diff", "--no-renames"}, output, error)) return false;
    for (std::size_t i = 0; i < output.size();) {
        const auto end = output.find('\0', i); if (end == std::string::npos) { error = "Incomplete staged file list"; return false; }
        std::string path = output.substr(i, end - i); i = end + 1;
        if (!prefix.empty() && path.rfind(prefix, 0) != 0) { error = "Staged changes outside this project: open the repository root before committing"; return false; }
        if (!prefix.empty()) path.erase(0, prefix.size()); preview.paths.push_back(std::move(path));
    }
    if (preview.paths.empty()) { error = "No staged changes"; return false; }
    if (!git(root, {"write-tree"}, preview.indexTree, error)) return false;
    std::string ignored; if (!git(root, {"rev-parse", "--verify", "HEAD"}, preview.head, ignored)) preview.head.clear();
    error.clear(); return true;
}
bool gitReadReviewedBlob(const std::filesystem::path& root, const GitCommitPreview& preview,
                         std::string_view path, std::string& contents, bool& present, std::string& error) {
    contents.clear(); present = false;
    auto tree = preview.indexTree;
    while (!tree.empty() && (tree.back() == '\n' || tree.back() == '\r')) tree.pop_back();
    if (!safePath(path) || std::find(preview.paths.begin(), preview.paths.end(), path) == preview.paths.end() ||
        (tree.size() != 40 && tree.size() != 64) || !std::all_of(tree.begin(), tree.end(), [](unsigned char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); })) {
        error = "Invalid reviewed Git path or tree"; return false;
    }
    std::string prefix, record;
    if (!git(root, {"rev-parse", "--show-prefix"}, prefix, error)) return false;
    while (!prefix.empty() && (prefix.back() == '\n' || prefix.back() == '\r')) prefix.pop_back();
    const std::string fullPath = prefix + std::string(path);
    if (!git(root, {"ls-tree", "--full-tree", "-z", tree, "--", fullPath}, record, error)) return false;
    if (record.empty()) { error.clear(); return true; }
    const auto tab = record.find('\t'), nul = record.find('\0');
    if (tab == std::string::npos || nul != record.size() - 1 || tab >= nul || record.substr(tab + 1, nul - tab - 1) != fullPath) {
        error = "Unexpected staged tree entry"; return false;
    }
    const auto metadata = record.substr(0, tab);
    if (!(metadata.starts_with("100644 blob ") || metadata.starts_with("100755 blob "))) { error = "Staged entry is not a regular file"; return false; }
    const auto hash = metadata.substr(12);
    if ((hash.size() != 40 && hash.size() != 64) || !std::all_of(hash.begin(), hash.end(), [](unsigned char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); })) { error = "Invalid staged blob ID"; return false; }
    if (!git(root, {"cat-file", "blob", hash}, contents, error)) { contents.clear(); return false; }
    present = true; return true;
}

bool gitCommit(const std::filesystem::path& root, const GitCommitPreview& preview, std::string_view message, std::string& output, std::string& error) {
    if (message.empty() || message.size() > 256 || message.find_first_not_of(" \t\r\n") == std::string_view::npos || message.find('\0') != std::string_view::npos) { error = "Enter a commit message of up to 256 bytes"; return false; }
    GitCommitPreview fresh; if (!gitCommitPreview(root, fresh, error)) return false;
    if (fresh.indexTree != preview.indexTree || fresh.head != preview.head || fresh.paths != preview.paths) { error = "Staged changes or HEAD changed; review them again"; return false; }
    return git(root, {"commit", "-m", std::string(message)}, output, error, false, false);
}

bool gitBlame(const std::filesystem::path& root, std::string_view path, int line, std::string& author, std::string& error) {
    if (line < 1 || !safePath(path)) { error = "No valid source line selected"; return false; }
    std::string output; const auto range = std::to_string(line) + "," + std::to_string(line);
    if (!git(root, {"blame", "--line-porcelain", "--contents", (root / std::string(path)).string(), "-L", range, "--", std::string(path)}, output, error)) return false;
    std::istringstream in(output); std::string row, name, summary;
    while (std::getline(in, row)) { if (row.rfind("author ", 0) == 0) name = row.substr(7); else if (row.rfind("summary ", 0) == 0) summary = row.substr(8); }
    if (output.rfind("0000000000000000000000000000000000000000 ", 0) == 0) { author = "Uncommitted change"; return true; }
    author = name.empty() ? "Unknown author" : name; if (!summary.empty()) author += " — " + summary; return true;
}

bool gitBranches(const std::filesystem::path& root, GitBranches& result, std::string& error) {
    GitBranches value; std::string raw;
    if (!git(root, {"rev-parse", "--verify", "HEAD"}, value.head, error)) return false;
    while (!value.head.empty() && (value.head.back() == '\n' || value.head.back() == '\r')) value.head.pop_back();
    if (!git(root, {"for-each-ref", "--format=%(refname:strip=2)%00%(objectname)%00%(HEAD)", "refs/heads/"}, raw, error)) return false;
    std::istringstream lines(raw); std::string row;
    while (std::getline(lines, row)) {
        const auto first = row.find('\0'), second = first == std::string::npos ? first : row.find('\0', first + 1);
        if (first == std::string::npos || second == std::string::npos || first == 0 || second == first + 1) { error = "Invalid Git branch output"; return false; }
        value.branches.push_back({row.substr(0, first), row.substr(first + 1, second - first - 1), row.substr(second + 1) == "*"});
        if (value.branches.size() > 10000) { error = "Too many branches to review"; return false; }
    }
    result = std::move(value); error.clear(); return true;
}
bool gitSwitchBranch(const std::filesystem::path& root, const GitBranches& expected,
                     const GitBranch& target, std::string& output, std::string& error) {
    output.clear(); error.clear();
    const auto matches = [&target](const GitBranch& value) { return value.name == target.name && value.commit == target.commit; };
    if (target.name.empty() || target.name.find('\0') != std::string::npos ||
        std::none_of(expected.branches.begin(), expected.branches.end(), matches)) { error = "Branch was not reviewed"; return false; }
    GitBranches current; if (!gitBranches(root, current, error)) return false;
    const auto active = [](const GitBranches& list) {
        const auto found = std::find_if(list.branches.begin(), list.branches.end(), [](const GitBranch& branch) { return branch.current; });
        return found == list.branches.end() ? std::string{} : found->name;
    };
    if (current.head != expected.head || active(current) != active(expected) ||
        std::none_of(current.branches.begin(), current.branches.end(), matches)) { error = "Branch or HEAD changed; review branches again"; return false; }
    std::string status;
    if (!git(root, {"status", "--porcelain=v1", "-z", "--untracked-files=normal", "--ignore-submodules=none"}, status, error)) return false;
    if (!status.empty()) { error = "Commit, stash, or remove all repository changes before switching branches"; return false; }
    return git(root, {"switch", "--no-guess", "--", target.name}, output, error, true, false);
}

bool gitHistory(const std::filesystem::path& root, std::vector<GitHistoryEntry>& entries, std::string& error) {
    std::string output; entries.clear();
    if (!git(root, {"log", "-n", "50", "--format=%h%x09%an%x09%as%x09%s"}, output, error)) return false;
    std::istringstream lines(output); std::string row;
    while (std::getline(lines, row)) {
        const auto first = row.find('\t'), second = first == std::string::npos ? first : row.find('\t', first + 1), third = second == std::string::npos ? second : row.find('\t', second + 1);
        if (third == std::string::npos) continue;
        entries.push_back({row.substr(0, first), row.substr(first + 1, second - first - 1), row.substr(second + 1, third - second - 1), row.substr(third + 1)});
    }
    return true;
}

bool gitFileHistory(const std::filesystem::path& root, std::string_view path, std::vector<GitFileHistoryEntry>& entries, std::string& error, bool* stoppedAtBoundary) {
    entries.clear();
    if (stoppedAtBoundary) *stoppedAtBoundary = false;
    if (!safePath(path)) { error = "Invalid project path"; return false; }
    std::string output, prefix;
    if (!git(root, {"rev-parse", "--show-prefix"}, prefix, error)) return false;
    while (!prefix.empty() && (prefix.back() == '\n' || prefix.back() == '\r')) prefix.pop_back();
    if (!git(root, {"log", "--follow", "--name-status", "--no-show-signature", "-z", "-n", "100", "--format=%H%x00%an%x00%as%x00%s", "--", std::string(path)}, output, error)) return false;
    std::size_t pos = 0;
    auto field = [&](std::string& value) {
        const auto end = output.find('\0', pos); if (end == std::string::npos) return false;
        value = output.substr(pos, end - pos); pos = end + 1; return true;
    };
    auto relative = [&](std::string_view name) -> std::string {
        if (!prefix.empty()) { if (!name.starts_with(prefix)) return {}; name.remove_prefix(prefix.size()); }
        return safePath(name) ? std::string(name) : std::string{};
    };
    while (pos < output.size()) {
        GitFileHistoryEntry entry; std::string status, first, second;
        if (!field(entry.hash) || !field(entry.author) || !field(entry.date) || !field(entry.subject) ||
            !field(status) || !status.starts_with('\n') || status.size() < 2 || !field(first)) {
            entries.clear(); error = "Incomplete Git history output"; return false;
        }
        const bool renamed = status[1] == 'R';
        if (renamed && !field(second)) { entries.clear(); error = "Incomplete Git rename output"; return false; }
        entry.path = relative(renamed ? second : first);
        if (entry.path.empty()) { if (stoppedAtBoundary) *stoppedAtBoundary = true; error.clear(); break; }
        if (renamed) entry.previousPath = relative(first);
        const bool crossedBoundary = renamed && entry.previousPath.empty();
        entries.push_back(std::move(entry));
        if (crossedBoundary) { if (stoppedAtBoundary) *stoppedAtBoundary = true; break; }
    }
    return true;
}

bool gitRangeHistory(const std::filesystem::path& root, std::string_view path,
                     std::string_view expectedSource, std::size_t first, std::size_t last,
                     std::vector<GitRangeHistoryEntry>& entries, std::string& error) {
    entries.clear(); error.clear();
    if (!safePath(path) || path.find_first_of("\r\n") != std::string_view::npos || !first || last < first || last > 10000000 || expectedSource.size() > 2U * 1024U * 1024U || expectedSource.find('\0') != std::string_view::npos) { error = "Invalid source path, text or selected range"; return false; }
    std::string prefix, head, committed;
    if (!git(root, {"rev-parse", "--show-prefix"}, prefix, error)) return false;
    while (!prefix.empty() && (prefix.back() == '\n' || prefix.back() == '\r')) prefix.pop_back();
    if (!prefix.empty()) { error = "Range history currently requires the project to be the repository root"; return false; }
    if (!git(root, {"rev-parse", "--verify", "HEAD"}, head, error)) return false;
    while (!head.empty() && (head.back() == '\n' || head.back() == '\r')) head.pop_back();
    if ((head.size() != 40 && head.size() != 64) || !std::all_of(head.begin(), head.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); })) { error = "Invalid Git HEAD"; return false; }
    if (!git(root, {"show", "--no-textconv", head + ":" + std::string(path)}, committed, error)) return false;
    if (committed != expectedSource) { error = "Save and commit this exact file before tracing selected lines"; return false; }
    std::string output;
    const auto range = std::to_string(first) + "," + std::to_string(last) + ":" + std::string(path);
    if (!git(root, {"log", "-n", "100", "--no-color", "--no-ext-diff", "--no-textconv", "--no-show-signature", "--format=%x1e%H%x00%an%x00%as%x00%s%x00", "-L", range, head}, output, error)) return false;
    std::vector<GitRangeHistoryEntry> result;
    std::size_t pos = 0;
    while (pos < output.size()) {
        if (output[pos++] != '\x1e') { error = "Unexpected Git range history output"; return false; }
        GitRangeHistoryEntry entry;
        const auto field = [&](std::string& value) { const auto end = output.find('\0', pos); if (end == std::string::npos) return false; value = output.substr(pos, end - pos); pos = end + 1; return true; };
        if (!field(entry.hash) || !field(entry.author) || !field(entry.date) || !field(entry.subject) ||
            (entry.hash.size() != 40 && entry.hash.size() != 64) || !std::all_of(entry.hash.begin(), entry.hash.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); })) { error = "Invalid Git range history metadata"; return false; }
        const auto next = output.find("\n\x1e", pos);
        const auto end = next == std::string::npos ? output.size() : next + 1;
        auto patch = std::string_view(output).substr(pos, end - pos);
        if (patch.find('\0') != std::string_view::npos) { error = "Invalid Git range history patch"; return false; }
        appendDiff(patch, entry.review.lines);
        for (auto& line : entry.review.lines) line.line = 0;
        result.push_back(std::move(entry)); pos = end;
    }
    entries = std::move(result); return true;
}

bool gitHistoricalFileDiff(const std::filesystem::path& root, std::string_view path, std::string_view commit, GitReview& review, std::string& error, std::string_view previousPath) {
    review.lines.clear();
    const bool validHash = (commit.size() == 40 || commit.size() == 64) && std::all_of(commit.begin(), commit.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
    if (!safePath(path) || (!previousPath.empty() && !safePath(previousPath)) || !validHash) { error = "Invalid project path or commit ID"; return false; }
    std::string output;
    std::vector<std::string> args{"show", "--format=", "--no-color", "--no-ext-diff", "--no-textconv", "--find-renames", "--no-show-signature", "-U3", std::string(commit), "--", std::string(path)};
    if (!previousPath.empty()) args.emplace_back(previousPath);
    if (!git(root, std::move(args), output, error)) return false;
    appendDiff(output, review.lines);
    // Historical line numbers must never be presented as locations in the current buffer.
    for (auto& line : review.lines) line.line = 0;
    return true;
}

bool gitUpstream(const std::filesystem::path& root, GitUpstream& upstream, std::string& error) {
    GitUpstream value;
    auto read = [&](std::vector<std::string> args, std::string& result) {
        if (!git(root, std::move(args), result, error)) return false;
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
        return true;
    };
    if (!read({"symbolic-ref", "--quiet", "--short", "HEAD"}, value.branch) || value.branch.empty()) { error = "A checked-out branch is required"; return false; }
    if (!read({"config", "--get", "branch." + value.branch + ".remote"}, value.remote) ||
        !read({"config", "--get", "branch." + value.branch + ".merge"}, value.mergeRef)) { error = "This branch has no configured upstream"; return false; }
    if (value.remote.empty() || value.remote.front() == '-' || value.remote == "." ||
        !std::string_view(value.mergeRef).starts_with("refs/heads/") || value.mergeRef.find_first_of("\r\n") != std::string::npos) {
        error = "Unsupported upstream configuration"; return false;
    }
    if (!read({"rev-parse", "--verify", "HEAD"}, value.head)) return false;
    upstream = std::move(value); error.clear(); return true;
}

bool gitRemote(const std::filesystem::path& root, const GitUpstream& expected, GitRemoteAction action, std::string& output, std::string& error) {
    GitUpstream current;
    if (!gitUpstream(root, current, error)) return false;
    if (current.branch != expected.branch || current.remote != expected.remote || current.mergeRef != expected.mergeRef || current.head != expected.head) {
        error = "Branch, upstream or HEAD changed; review the operation again"; return false;
    }
    if (action == GitRemoteAction::Pull) {
        std::string status;
        if (!git(root, {"status", "--porcelain=v1", "--untracked-files=normal"}, status, error)) return false;
        if (!status.empty()) { error = "Commit or otherwise resolve all working-tree changes before pulling"; return false; }
    }
    std::vector<std::string> args{"-c", "credential.interactive=never", "-c", "http.lowSpeedLimit=1", "-c", "http.lowSpeedTime=30"};
    if (action == GitRemoteAction::Fetch) args.insert(args.end(), {"fetch", "--no-tags", "--", current.remote});
    else if (action == GitRemoteAction::Pull) args.insert(args.end(), {"pull", "--ff-only", "--no-rebase", "--", current.remote, current.mergeRef});
    else args.insert(args.end(), {"push", "--no-follow-tags", "--recurse-submodules=no", "--", current.remote, current.head + ":" + current.mergeRef});
    return git(root, std::move(args), output, error, true, false);
}
}
