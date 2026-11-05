#include "modules/ModuleProcess.hpp"
#include <algorithm>
#include <chrono>
#include <limits>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
namespace lucid::modules {
namespace {
void append(ModuleProcessResult& r, const char* data, std::size_t n, std::size_t limit) {
    const auto take = std::min(n, limit - r.output.size()); r.output.append(data, take); if (take != n) r.outputTruncated = true;
}
#ifdef _WIN32
std::wstring widen(const std::string& s) {
    if (s.empty() || s.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (!n) return {};
    std::wstring out(n, L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), out.data(), n)) return {};
    return out;
}
std::wstring quote(const std::wstring& s) {
    std::wstring out = L"\""; unsigned slash = 0;
    for (auto c : s) { if (c == L'\\') { ++slash; continue; } if (c == L'\"') { out.append(slash * 2 + 1, L'\\'); out += c; } else { out.append(slash, L'\\'); out += c; } slash = 0; }
    out.append(slash * 2, L'\\'); return out + L"\"";
}
#endif
}
ModuleProcessResult runModuleProcess(const std::filesystem::path& executable, const std::vector<std::string>& args, std::chrono::milliseconds timeout, std::size_t outputLimit, const std::atomic_bool* cancel, const std::filesystem::path& workingDirectory) {
    ModuleProcessResult result;
    if (cancel && cancel->load()) { result.cancelled = true; return result; }
    if (!executable.is_absolute() || timeout.count() <= 0) { result.error = "Executable must be absolute and timeout positive"; return result; }
    std::error_code directoryError;
    if (!workingDirectory.empty() && (workingDirectory.native().find(std::filesystem::path::value_type{})!=std::filesystem::path::string_type::npos || !workingDirectory.is_absolute() || !std::filesystem::is_directory(workingDirectory,directoryError) || directoryError)) { result.error="Working directory must be an existing absolute directory";return result; }
    for (const auto& arg : args) if (arg.find('\0') != std::string::npos) { result.error = "Argument contains NUL"; return result; }
#ifdef _WIN32
    if (executable.native().find(L'\0') != std::wstring::npos) { result.error = "Executable contains NUL"; return result; }
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE}; HANDLE readPipe = nullptr, writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) { result.error = "Cannot create output pipe"; return result; }
    if (!SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0)) { CloseHandle(readPipe); CloseHandle(writePipe); result.error = "Cannot protect output read handle"; return result; }
    HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (input == INVALID_HANDLE_VALUE) { CloseHandle(readPipe); CloseHandle(writePipe); result.error = "Cannot open null input"; return result; }
    std::wstring command = quote(executable.wstring());
    for (const auto& a : args) { auto w = widen(a); if (!a.empty() && w.empty()) { result.error = "Invalid UTF-8 argument"; CloseHandle(input); CloseHandle(readPipe); CloseHandle(writePipe); return result; } command += L" " + quote(w); }
    SIZE_T attributeBytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeBytes);
    std::vector<unsigned char> attributeStorage(attributeBytes);
    auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeStorage.data());
    if (!attributeBytes || !InitializeProcThreadAttributeList(attributes, 1, 0, &attributeBytes)) { CloseHandle(input); CloseHandle(readPipe); CloseHandle(writePipe); result.error = "Cannot initialize process handle list"; return result; }
    HANDLE handles[] = {input, writePipe};
    if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles, sizeof(handles), nullptr, nullptr)) { DeleteProcThreadAttributeList(attributes); CloseHandle(input); CloseHandle(readPipe); CloseHandle(writePipe); result.error = "Cannot set process handle list"; return result; }
    STARTUPINFOEXW si{}; si.StartupInfo.cb = sizeof(si); si.StartupInfo.dwFlags = STARTF_USESTDHANDLES; si.StartupInfo.hStdInput = input; si.StartupInfo.hStdOutput = writePipe; si.StartupInfo.hStdError = writePipe; si.lpAttributeList = attributes; PROCESS_INFORMATION pi{};
    HANDLE job = CreateJobObjectW(nullptr, nullptr); JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{}; limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) { if (job) CloseHandle(job); DeleteProcThreadAttributeList(attributes); CloseHandle(input); CloseHandle(readPipe); CloseHandle(writePipe); result.error = "Cannot create process job"; return result; }
    BOOL ok = CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr, workingDirectory.empty()?nullptr:workingDirectory.c_str(), &si.StartupInfo, &pi);
    DeleteProcThreadAttributeList(attributes);
    CloseHandle(input); CloseHandle(writePipe);
    if (!ok) { CloseHandle(job); CloseHandle(readPipe); result.error = "Cannot start process"; return result; }
    if (!AssignProcessToJobObject(job, pi.hProcess)) { TerminateProcess(pi.hProcess, 1); WaitForSingleObject(pi.hProcess, INFINITE); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); CloseHandle(job); CloseHandle(readPipe); result.error = "Cannot assign process job"; return result; }
    if (ResumeThread(pi.hThread) == static_cast<DWORD>(-1)) { TerminateJobObject(job, 1); WaitForSingleObject(pi.hProcess, INFINITE); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); CloseHandle(job); CloseHandle(readPipe); result.error = "Cannot resume process"; return result; }
    CloseHandle(pi.hThread); auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;) {
        DWORD available = 0; if (PeekNamedPipe(readPipe, nullptr, 0, nullptr, &available, nullptr) && available) { char buf[4096]; DWORD got = 0; if (ReadFile(readPipe, buf, std::min<DWORD>(sizeof(buf), available), &got, nullptr) && got) append(result, buf, got, outputLimit); }
        if (WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) { DWORD remaining = 0; if (!PeekNamedPipe(readPipe, nullptr, 0, nullptr, &remaining, nullptr) || !remaining) break; }
        if ((cancel && cancel->load()) || std::chrono::steady_clock::now() >= deadline) { result.cancelled = cancel && cancel->load(); result.timedOut = !result.cancelled; TerminateJobObject(job, 1); WaitForSingleObject(pi.hProcess, INFINITE); break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    DWORD code = 0; if (GetExitCodeProcess(pi.hProcess, &code)) result.exitCode = static_cast<int>(code); CloseHandle(job); CloseHandle(pi.hProcess); CloseHandle(readPipe);
#else
    int pipefd[2]; if (pipe(pipefd) != 0) { result.error = "Cannot create output pipe"; return result; }
    // Ensure pipe descriptors cannot overlap standard handles when the caller has closed one.
    for (int i = 0; i < 2; ++i) {
        if (pipefd[i] < 3) {
            int moved = fcntl(pipefd[i], F_DUPFD, 3);
            if (moved < 0) { close(pipefd[0]); close(pipefd[1]); result.error = "Cannot move output pipe"; return result; }
            close(pipefd[i]); pipefd[i] = moved;
        }
        int flags = fcntl(pipefd[i], F_GETFD);
        if (flags < 0 || fcntl(pipefd[i], F_SETFD, flags | FD_CLOEXEC) < 0) { close(pipefd[0]); close(pipefd[1]); result.error = "Cannot protect output pipe"; return result; }
    }
    int readFlags = fcntl(pipefd[0], F_GETFL);
    if (readFlags < 0 || fcntl(pipefd[0], F_SETFL, readFlags | O_NONBLOCK) < 0) { close(pipefd[0]); close(pipefd[1]); result.error = "Cannot configure output pipe"; return result; }
    std::string program = executable.string(), directory=workingDirectory.string(); if (program.find('\0') != std::string::npos || directory.find('\0') != std::string::npos) { close(pipefd[0]); close(pipefd[1]); result.error = "Executable or working directory contains NUL"; return result; }
    std::vector<char*> argv; argv.reserve(args.size() + 2); argv.push_back(program.data()); for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str())); argv.push_back(nullptr);
    long maxFd = sysconf(_SC_OPEN_MAX); if (maxFd < 0) maxFd = 1024;
    pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); result.error = "Cannot start process"; return result; }
    if (!pid) {
        setpgid(0, 0); if(!directory.empty() && chdir(directory.c_str())!=0) _exit(126); int nullfd = open("/dev/null", O_RDONLY); if (nullfd < 0 || dup2(nullfd, STDIN_FILENO) < 0 || dup2(pipefd[1], STDOUT_FILENO) < 0 || dup2(pipefd[1], STDERR_FILENO) < 0) _exit(126);
        for (long fd = 3; fd < maxFd; ++fd) close(static_cast<int>(fd));
        execv(program.c_str(), argv.data()); _exit(127);
    }
    setpgid(pid, pid); close(pipefd[1]);
    auto deadline = std::chrono::steady_clock::now() + timeout; bool exited = false, eof = false; int status = 0;
    while (!exited || !eof) {
        char buf[4096]; for (unsigned batch = 0; batch < 64; ++batch) { auto n = read(pipefd[0], buf, sizeof(buf)); if (n > 0) append(result, buf, static_cast<std::size_t>(n), outputLimit); else { if (n == 0) eof = true; break; } }
        if (!exited) { auto w = waitpid(pid, &status, WNOHANG); if (w == pid) exited = true; else if (w < 0 && errno != EINTR) { exited = true; result.error = "Cannot wait for process"; } }
        if (exited && eof) break;
        if ((cancel && cancel->load()) || std::chrono::steady_clock::now() >= deadline) { result.cancelled = cancel && cancel->load(); result.timedOut = !result.cancelled; kill(-pid, SIGKILL); kill(pid, SIGKILL); if (!exited) while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {} break; }
        pollfd pfd{pipefd[0], POLLIN | POLLHUP, 0}; poll(&pfd, 1, 10);
    }
    close(pipefd[0]); if (WIFEXITED(status)) result.exitCode = WEXITSTATUS(status); else if (WIFSIGNALED(status)) result.exitCode = 128 + WTERMSIG(status);
#endif
    return result;
}
}
