#include "debug/AdapterProcess.hpp"
#include <cerrno>
#include <cstring>
#include <cstdlib>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>
#else
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif
namespace lucid::debug {
#if defined(_WIN32)
struct AdapterProcess::WindowsState {
    HANDLE process = nullptr, job = nullptr, input = nullptr, output = nullptr;
    std::thread reader, writer;
    std::mutex mutex;
    std::condition_variable ready;
    std::string outgoing, incoming, failure;
    bool stopping = false, eof = false;
};
namespace {
std::wstring wide(std::string_view value) {
    if (value.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (!length) return {};
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), length);
    return result;
}
std::wstring quoteArgument(std::wstring_view argument) {
    std::wstring result = L"\""; std::size_t slashes = 0;
    for (const auto c : argument) {
        if (c == L'\\') { ++slashes; continue; }
        if (c == L'\"') { result.append(slashes * 2 + 1, L'\\'); result += c; }
        else { result.append(slashes, L'\\'); result += c; }
        slashes = 0;
    }
    result.append(slashes * 2, L'\\'); result += L'\"'; return result;
}
}
#endif
AdapterProcess::~AdapterProcess() { close(); }
bool AdapterProcess::startDefault(std::string& error) {
#if defined(__APPLE__)
    if (start("xcrun", {"lldb-dap"}, error)) return true;
#else
    if (start("lldb-dap", {}, error)) return true;
#if defined(_WIN32)
    if (const char* programFiles = std::getenv("ProgramFiles")) {
        const std::string path = std::string(programFiles) + "\\LLVM\\bin\\lldb-dap.exe";
        if (start(path, {}, error)) return true;
    }
#else
    for (const char* candidate : {"lldb-dap-20", "lldb-dap-19", "lldb-dap-18", "lldb-dap-17", "lldb-vscode"})
        if (start(candidate, {}, error)) return true;
#endif
#endif
    error = "Could not start LLDB's debug adapter. Install LLVM with lldb-dap and add it to PATH. " + error;
    return false;
}
bool AdapterProcess::start(const std::string& executable, const std::vector<std::string>& arguments, std::string& error) {
    close(); error.clear();
#if defined(_WIN32)
    auto* state = new WindowsState;
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE childInput = nullptr, childOutput = nullptr;
    if (!CreatePipe(&childInput, &state->input, &security, 0) ||
        !CreatePipe(&state->output, &childOutput, &security, 0) ||
        !SetHandleInformation(state->input, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(state->output, HANDLE_FLAG_INHERIT, 0)) {
        if (childInput) CloseHandle(childInput); if (childOutput) CloseHandle(childOutput);
        if (state->input) CloseHandle(state->input); if (state->output) CloseHandle(state->output);
        delete state; error = "Could not create debugger pipes"; return false;
    }
    const auto program = wide(executable);
    if (program.empty()) { CloseHandle(childInput); CloseHandle(childOutput); CloseHandle(state->input); CloseHandle(state->output); delete state; error = "Invalid debugger path"; return false; }
    std::wstring command = quoteArgument(program);
    for (const auto& arg : arguments) { const auto converted = wide(arg); if (!arg.empty() && converted.empty()) { CloseHandle(childInput); CloseHandle(childOutput); CloseHandle(state->input); CloseHandle(state->output); delete state; error = "Invalid debugger argument"; return false; } command += L" " + quoteArgument(converted); }
    HANDLE nullError = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (nullError == INVALID_HANDLE_VALUE) { CloseHandle(childInput); CloseHandle(childOutput); CloseHandle(state->input); CloseHandle(state->output); delete state; error = "Could not redirect debug adapter diagnostics"; return false; }
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = childInput; startup.hStdOutput = childOutput; startup.hStdError = nullError;
    PROCESS_INFORMATION process{};
    state->job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{}; limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    const bool jobReady = state->job && SetInformationJobObject(state->job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
    const bool started = jobReady && CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &startup, &process);
    CloseHandle(childInput); CloseHandle(childOutput); CloseHandle(nullError);
    if (!started || !AssignProcessToJobObject(state->job, process.hProcess)) {
        const DWORD code = GetLastError(); if (started) { TerminateProcess(process.hProcess, 1); CloseHandle(process.hThread); CloseHandle(process.hProcess); }
        if (state->job) CloseHandle(state->job); CloseHandle(state->input); CloseHandle(state->output); delete state;
        error = "Could not start debug adapter (Windows error " + std::to_string(code) + "). Install LLVM lldb-dap and add it to PATH."; return false;
    }
    state->process = process.hProcess; ResumeThread(process.hThread); CloseHandle(process.hThread);
    state->reader = std::thread([state] {
        char buffer[8192]; DWORD count = 0;
        while (ReadFile(state->output, buffer, sizeof(buffer), &count, nullptr) && count) {
            std::lock_guard lock(state->mutex);
            if (state->incoming.size() + count > 8U * 1024U * 1024U) { state->failure = "Debug adapter output exceeds limit"; break; }
            state->incoming.append(buffer, count);
        }
        std::lock_guard lock(state->mutex); state->eof = true;
    });
    state->writer = std::thread([state] {
        for (;;) {
            std::string bytes;
            { std::unique_lock lock(state->mutex); state->ready.wait(lock, [state] { return state->stopping || !state->outgoing.empty(); });
              if (state->stopping) break; bytes.swap(state->outgoing); }
            std::size_t offset = 0;
            while (offset < bytes.size()) { DWORD written = 0;
                if (!WriteFile(state->input, bytes.data() + offset, static_cast<DWORD>(std::min<std::size_t>(bytes.size() - offset, 65536)), &written, nullptr) || !written) {
                    std::lock_guard lock(state->mutex); if (!state->stopping) state->failure = "Debug adapter connection closed"; return;
                } offset += written;
            }
        }
    });
    m_windows = state; m_alive = true; return true;
#else
    int sockets[2]; if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) { error = "Could not create debugger connection"; return false; }
    for (int fd : sockets) (void)fcntl(fd, F_SETFD, FD_CLOEXEC);
#if defined(SO_NOSIGPIPE)
    int enabled = 1; (void)setsockopt(sockets[0], SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#endif
    posix_spawn_file_actions_t actions; posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, sockets[1], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, sockets[1], STDOUT_FILENO);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addclose(&actions, sockets[0]); posix_spawn_file_actions_addclose(&actions, sockets[1]);
    std::vector<std::string> values; values.push_back(executable); values.insert(values.end(), arguments.begin(), arguments.end());
    std::vector<char*> argv; for (auto& v : values) argv.push_back(v.data()); argv.push_back(nullptr);
    pid_t child = -1; const int rc = posix_spawnp(&child, executable.c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions); ::close(sockets[1]);
    if (rc != 0) { ::close(sockets[0]); error = "Could not start debug adapter: " + std::string(std::strerror(rc)); return false; }
    const int flags = fcntl(sockets[0], F_GETFL, 0);
    if (flags < 0 || fcntl(sockets[0], F_SETFL, flags | O_NONBLOCK) < 0) { ::close(sockets[0]); (void)kill(child, SIGKILL); (void)waitpid(child, nullptr, 0); error = "Could not configure debug adapter"; return false; }
    m_fd = sockets[0]; m_pid = static_cast<int>(child); m_alive = true; return true;
#endif
}
bool AdapterProcess::send(std::string_view bytes, std::string& error) {
    if (!m_alive) { error = "Debug adapter is not running"; return false; }
    if (bytes.size() > 1024U * 1024U || m_pending.size() > 1024U * 1024U - bytes.size()) { error = "Debug adapter input exceeds limit"; return false; }
#if defined(_WIN32)
    { std::lock_guard lock(m_windows->mutex);
      if (!m_windows->failure.empty() || m_windows->eof) { error = "Debug adapter connection closed"; return false; }
      if (m_windows->outgoing.size() > 1024U * 1024U - bytes.size()) { error = "Debug adapter input exceeds limit"; return false; }
      m_windows->outgoing.append(bytes); }
    m_windows->ready.notify_one();
#else
    m_pending.append(bytes);
#endif
    error.clear(); return true;
}
bool AdapterProcess::poll(std::string& output, std::string& error) {
    output.clear(); error.clear();
#if defined(_WIN32)
    if (!m_alive || !m_windows) { error = "Debug adapter is not running"; return false; }
    bool ended = false;
    { std::lock_guard lock(m_windows->mutex); output.swap(m_windows->incoming); error = m_windows->failure; ended = m_windows->eof; }
    if (!error.empty()) { close(); return false; }
    if (ended && output.empty()) { error = "Debug adapter exited"; close(); return false; }
    return true;
#else
    if (!m_alive) { error = "Debug adapter is not running"; return false; }
    while (!m_pending.empty()) {
#if defined(MSG_NOSIGNAL)
        const ssize_t n = ::send(m_fd, m_pending.data(), m_pending.size(), MSG_NOSIGNAL);
#else
        const ssize_t n = ::send(m_fd, m_pending.data(), m_pending.size(), 0);
#endif
        if (n > 0) { m_pending.erase(0, static_cast<std::size_t>(n)); continue; }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        error = "Debug adapter connection closed"; close(); return false;
    }
    char buffer[8192];
    for (;;) {
        const ssize_t n = ::recv(m_fd, buffer, sizeof(buffer), 0);
        if (n > 0) {
            if (output.size() + static_cast<std::size_t>(n) > 8U * 1024U * 1024U) { error = "Debug adapter output exceeds limit"; close(); return false; }
            output.append(buffer, static_cast<std::size_t>(n)); continue;
        }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return true;
        // Preserve already-read output; a subsequent poll reports disconnection.
        if (!output.empty()) return true;
        error = "Debug adapter exited"; close(); return false;
    }
#endif
}
void AdapterProcess::close() noexcept {
#if defined(_WIN32)
    if (m_windows) {
        auto* state = m_windows;
        { std::lock_guard lock(state->mutex); state->stopping = true; }
        state->ready.notify_all();
        if (state->job) TerminateJobObject(state->job, 1);
        if (state->reader.joinable()) CancelSynchronousIo(state->reader.native_handle());
        if (state->writer.joinable()) CancelSynchronousIo(state->writer.native_handle());
        if (state->reader.joinable()) state->reader.join();
        if (state->writer.joinable()) state->writer.join();
        if (state->input) CloseHandle(state->input); if (state->output) CloseHandle(state->output);
        if (state->process) CloseHandle(state->process); if (state->job) CloseHandle(state->job);
        delete state; m_windows = nullptr;
    }
#else
    if (m_fd >= 0) { ::close(m_fd); m_fd = -1; }
    if (m_pid > 0) {
        int status = 0; const pid_t pid = static_cast<pid_t>(m_pid);
        if (waitpid(pid, &status, WNOHANG) == 0) { (void)kill(pid, SIGKILL); while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {} }
        m_pid = -1;
    }
#endif
    m_pending.clear(); m_alive = false;
}
}
