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
bool git(const std::filesystem::path& root, std::vector<std::string> args, std::string& output, std::string& error) {
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
    const BOOL launched = CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    CloseHandle(writer); CloseHandle(input);
    if (!launched) { CloseHandle(reader); error = "Could not start Git; install Git and try again"; return false; }
    output.clear(); char buffer[4096]; DWORD n = 0; bool truncated = false;
    while (ReadFile(reader, buffer, sizeof(buffer), &n, nullptr) && n > 0) {
        const auto available = 2U * 1024U * 1024U - output.size(); if (n > available) truncated = true;
        output.append(buffer, std::min<std::size_t>(n, available));
    }
    CloseHandle(reader); WaitForSingleObject(process.hProcess, INFINITE); DWORD status = 1; GetExitCodeProcess(process.hProcess, &status);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    if (truncated) { error = "Git output exceeds the 2 MiB review limit"; return false; }
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
    pid_t child; const int result = posix_spawnp(&child, "git", &actions, nullptr, argv.data(), environ);
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
    if (truncated) { error = "Git output exceeds the 2 MiB review limit"; return false; }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) { error = output.empty() ? "Git command failed" : output; return false; }
    error.clear(); return true;
#endif
}
void appendDiff(std::string_view text, std::vector<GitReviewLine>& lines) {
    std::istringstream in{std::string(text)}; std::string line; int current = 0;
    while (std::getline(in, line)) {
        if (line.rfind("@@ ", 0) == 0) {
            auto plus = line.find('+'); if (plus != std::string::npos) std::from_chars(line.data() + plus + 1, line.data() + line.size(), current);
            lines.push_back({line, current});
        } else if (!line.empty() && line[0] == '+' && line.rfind("+++", 0) != 0) {
            lines.push_back({line, current++});
        } else if (!line.empty() && line[0] == '-' && line.rfind("---", 0) != 0) {
            lines.push_back({line, current});
        } else if (!line.empty() && line[0] == ' ' && current > 0) { lines.push_back({line, current++}); }
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
    if (!safePath(change.path)) { error = "Invalid project path"; return false; }
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
            if (!git(root, std::move(args), output, error)) return false;
            if (!output.empty()) { review.lines.push_back({staged ? "Staged changes" : "Working tree changes", 0}); appendDiff(output, review.lines); }
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
    std::string output; return git(root, std::move(args), output, error);
}
bool gitUnstage(const std::filesystem::path& root, const GitChange& change, std::string& error) {
    if (!safePath(change.path) || (!change.oldPath.empty() && !safePath(change.oldPath))) { error = "Invalid project path"; return false; }
    if (change.conflict) { error = "Cannot unstage an unresolved conflict"; return false; }
    std::string output, ignored;
    const bool hasHead = git(root, {"rev-parse", "--verify", "HEAD"}, output, ignored);
    std::vector<std::string> args = hasHead ? std::vector<std::string>{"restore", "--staged", "--"} : std::vector<std::string>{"rm", "--cached", "-r", "--ignore-unmatch", "--"};
    args.push_back(change.path); if (!change.oldPath.empty()) args.push_back(change.oldPath);
    return git(root, std::move(args), output, error);
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
bool gitCommit(const std::filesystem::path& root, const GitCommitPreview& preview, std::string_view message, std::string& output, std::string& error) {
    if (message.empty() || message.size() > 256 || message.find_first_not_of(" \t\r\n") == std::string_view::npos || message.find('\0') != std::string_view::npos) { error = "Enter a commit message of up to 256 bytes"; return false; }
    GitCommitPreview fresh; if (!gitCommitPreview(root, fresh, error)) return false;
    if (fresh.indexTree != preview.indexTree || fresh.head != preview.head || fresh.paths != preview.paths) { error = "Staged changes or HEAD changed; review them again"; return false; }
    return git(root, {"commit", "-m", std::string(message)}, output, error);
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
}
