#include "platform/terminal/PtyProcess.hpp"
#include "text/Utf8.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#if defined(__linux__)
#include <pty.h>
#elif defined(__FreeBSD__)
#include <libutil.h>
#else
#include <util.h>
#endif
#endif

namespace lucid::platform {
namespace {

constexpr std::size_t kMaxRawOutput = 256U * 1024U;
constexpr std::size_t kMaxOutput = 64U * 1024U;
constexpr const char kDisableEcho[] = "stty -echo 2>/dev/null || true\n";

[[nodiscard]] std::string makeMarker() {
    static std::atomic<std::uint64_t> counter{0};
    const auto stamp = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    return "__LUCID_DONE_" + std::to_string(stamp) + "_" +
           std::to_string(counter.fetch_add(1)) + "__";
}

void trimToSize(std::string& text, std::size_t limit) {
    if (text.size() <= limit) {
        return;
    }
    std::size_t cut = text.size() - limit;
    const std::size_t newline = text.find('\n', cut);
    // Prefer complete lines, unless doing so would discard almost the entire
    // retained output (for example, one very long line ending in a newline).
    if (newline != std::string::npos && text.size() - (newline + 1) >= limit / 2) cut = newline + 1;
    else while (cut < text.size() && (static_cast<unsigned char>(text[cut]) & 0xc0U) == 0x80U) ++cut;
    text.erase(0, cut);
}

[[nodiscard]] std::string sanitizeOutput(std::string_view input) {
    std::string output;
    output.reserve(input.size());

    for (std::size_t index = 0; index < input.size(); ++index) {
        const unsigned char character = static_cast<unsigned char>(input[index]);
        if (character == 0x1B) {
            if (index + 1 >= input.size()) {
                break;
            }
            const unsigned char next = static_cast<unsigned char>(input[index + 1]);
            if (next == '[') {
                index += 2;
                while (index < input.size()) {
                    const unsigned char code =
                        static_cast<unsigned char>(input[index]);
                    if (code >= 0x40 && code <= 0x7E) {
                        break;
                    }
                    ++index;
                }
            } else if (next == ']') {
                index += 2;
                while (index < input.size()) {
                    if (input[index] == '\a') {
                        break;
                    }
                    if (input[index] == '\x1B' && index + 1 < input.size() &&
                        input[index + 1] == '\\') {
                        ++index;
                        break;
                    }
                    ++index;
                }
            } else {
                ++index;
            }
            continue;
        }

        if (character == '\r') {
            auto next = index + 1;
            while (next < input.size() && input[next] == '\r') ++next;
            if (next < input.size() && input[next] != '\n') {
                const auto line = output.find_last_of('\n');
                output.erase(line == std::string::npos ? 0 : line + 1);
            }
            continue;
        }
        if (character == '\b') {
            if (!output.empty() && output.back() != '\n') {
                output.resize(lucid::text::lastUtf8Start(output));
            }
            continue;
        }
        if (character == '\t') {
            output += "    ";
            continue;
        }
        if (character < 0x20 && character != '\n') {
            continue;
        }
        output.push_back(static_cast<char>(character));
    }

    return output;
}

} // namespace

PtyProcess::~PtyProcess() {
    close();
}

bool PtyProcess::start(std::string workingDirectory,
                       std::vector<std::string> commands, std::string& error) {
    close();
    error.clear();
    m_commands = std::move(commands);
    m_exitCodes.clear();
    m_nextCommand = 0;
    m_commandsDone = false;
    m_rawOutput.clear();
    m_output.clear();
    m_pendingInput.clear();
    m_marker = makeMarker();

#if defined(_WIN32)
    (void)workingDirectory;
    error = "Compile terminals are not supported on this platform yet";
    return false;
#else
    struct winsize size {};
    size.ws_row = 24;
    size.ws_col = 100;

    int master = -1;
    const pid_t pid = forkpty(&master, nullptr, nullptr, &size);
    if (pid < 0) {
        error = std::string("Could not create terminal: ") + std::strerror(errno);
        return false;
    }

    if (pid == 0) {
        struct termios attributes {};
        if (tcgetattr(STDIN_FILENO, &attributes) == 0) {
            attributes.c_lflag &= static_cast<tcflag_t>(~ECHO);
            (void)tcsetattr(STDIN_FILENO, TCSANOW, &attributes);
        }
        if (!workingDirectory.empty() && chdir(workingDirectory.c_str()) != 0) {
            constexpr char message[] = "Lucid: cannot enter the project directory; commands were not run.\n";
            (void)::write(STDERR_FILENO, message, sizeof(message) - 1);
            _exit(126);
        }
        (void)setenv("PS1", "", 1);
        (void)setenv("PS2", "", 1);
        execl("/bin/sh", "sh", nullptr);
        _exit(127);
    }

    const int flags = fcntl(master, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(master, F_SETFL, flags | O_NONBLOCK);
    }
    struct termios terminalAttributes {};
    if (tcgetattr(master, &terminalAttributes) == 0) {
        terminalAttributes.c_lflag &= static_cast<tcflag_t>(~ECHO);
        (void)tcsetattr(master, TCSANOW, &terminalAttributes);
    }

    m_fd = master;
    m_pid = static_cast<int>(pid);
    m_alive = true;
    write(kDisableEcho);
    sendNextCommand();
    return true;
#endif
}

void PtyProcess::update() {
    if (!m_alive) {
        reap();
        return;
    }
    flushInput();
    pumpOutput();
    if (!m_alive) {
        reap();
    }
}

void PtyProcess::write(std::string_view text) {
#if defined(_WIN32)
    (void)text;
#else
    if (m_fd < 0 || text.empty()) {
        return;
    }
    m_pendingInput.append(text);
    flushInput();
#endif
}

void PtyProcess::flushInput() {
#if !defined(_WIN32)
    if (m_fd < 0 || m_pendingInput.empty()) return;
    std::size_t written = 0;
    while (written < m_pendingInput.size()) {
        const ssize_t count =
            ::write(m_fd, m_pendingInput.data() + written, m_pendingInput.size() - written);
        if (count > 0) {
            written += static_cast<std::size_t>(count);
            continue;
        }
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        if (count < 0 && errno == EINTR) continue;
        break;
    }
    m_pendingInput.erase(0, written);
#endif
}

void PtyProcess::appendEcho(std::string_view text) {
    m_rawOutput.append(text);
    m_rawOutput.push_back('\n');
}

void PtyProcess::close() noexcept {
#if !defined(_WIN32)
    const pid_t foreground = m_fd >= 0 ? ::tcgetpgrp(m_fd) : -1;
    if (foreground > 1 && foreground != ::getpgrp()) {
        (void)kill(-foreground, SIGHUP);
        (void)kill(-foreground, SIGKILL);
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
    if (m_pid > 0) {
        const auto pid = static_cast<pid_t>(m_pid);
        // forkpty creates a session and process group for the shell. Signal the
        // group so its build processes do not outlive the compile window.
        (void)kill(-pid, SIGHUP);
        (void)kill(pid, SIGHUP);
        (void)kill(-pid, SIGKILL);
        int status = 0;
        if (waitpid(pid, &status, WNOHANG) == 0) {
            (void)kill(pid, SIGKILL);
            (void)waitpid(pid, &status, 0);
        }
        m_pid = -1;
    }
#endif
    m_alive = false;
    m_pendingInput.clear();
}

void PtyProcess::sendNextCommand() {
    if (m_fd < 0) {
        return;
    }
    if (m_nextCommand >= m_commands.size()) {
        m_commandsDone = true;
        write("exit\n");
        return;
    }

    const std::string& command = m_commands[m_nextCommand++];
    m_commandStarted = std::chrono::steady_clock::now();
    std::string line = "{\n" + command;
    line += "\n}; command printf '\\n%s:%s\\n' '" + m_marker + "' \"$?\"\n";
    write(line);
}

void PtyProcess::pumpOutput() {
#if defined(_WIN32)
    return;
#else
    char buffer[4096];
    bool closed = false;
    for (;;) {
        const ssize_t count = ::read(m_fd, buffer, sizeof(buffer));
        if (count > 0) {
            m_rawOutput.append(buffer, static_cast<std::size_t>(count));
            trimToSize(m_rawOutput, kMaxRawOutput);
            continue;
        }
        if (count == 0) {
            closed = true;
        }
        if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK &&
            errno != EINTR) {
            closed = true;
        }
        break;
    }

    if (!m_marker.empty()) {
        std::size_t search = 0;
        while (true) {
            const std::size_t marker = m_rawOutput.find(m_marker, search);
            if (marker == std::string::npos) break;
            const std::size_t statusStart = marker + m_marker.size();
            if (statusStart == m_rawOutput.size()) break;
            if (m_rawOutput[statusStart] != ':') { search = statusStart; continue; }
            const std::size_t end = m_rawOutput.find('\n', statusStart);
            if (end == std::string::npos) break;
            if (statusStart < end) {
                const std::string status = m_rawOutput.substr(statusStart + 1, end - statusStart - 1);
                char* parsedEnd = nullptr;
                const long code = std::strtol(status.c_str(), &parsedEnd, 10);
                if (parsedEnd != status.c_str() && (*parsedEnd == '\0' ||
                    (*parsedEnd == '\r' && parsedEnd[1] == '\0')) && code >= 0 && code <= 255) {
                    m_exitCodes.push_back(static_cast<int>(code));
                    m_rawOutput.erase(marker, end - marker + 1);
                    if (marker > 0 && m_rawOutput[marker - 1] == '\n')
                        m_rawOutput.erase(marker - 1, 1);
                    if (code != 0) {
                        m_rawOutput += "\n[Command " + std::to_string(m_exitCodes.size()) +
                            " failed with exit code " + std::to_string(code) + "]\n";
                    }
                    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - m_commandStarted).count();
                    m_rawOutput += "\n[Command " + std::to_string(m_exitCodes.size()) + " finished in " +
                        std::to_string(elapsed) + " ms; exit code " + std::to_string(code) + "]\n";
                    sendNextCommand();
                    break;
                }
            }
            search = statusStart;
        }
    }

    m_output = sanitizeOutput(m_rawOutput);
    if (m_output.starts_with(kDisableEcho)) m_output.erase(0, sizeof(kDisableEcho) - 1);
    trimToSize(m_output, kMaxOutput);

    if (closed) {
        if (!m_commandsDone && m_nextCommand > m_exitCodes.size()) {
            m_exitCodes.push_back(-1);
            m_rawOutput += "\n[Compile shell exited before reporting the command result]\n";
            m_output = sanitizeOutput(m_rawOutput);
            if (m_output.starts_with(kDisableEcho)) m_output.erase(0, sizeof(kDisableEcho) - 1);
            trimToSize(m_output, kMaxOutput);
        }
        if (m_fd >= 0) {
            ::close(m_fd);
            m_fd = -1;
        }
        m_alive = false;
        m_commandsDone = true;
    }
#endif
}

void PtyProcess::reap() noexcept {
#if !defined(_WIN32)
    if (m_pid > 0) {
        int status = 0;
        if (waitpid(static_cast<pid_t>(m_pid), &status, WNOHANG) != 0) {
            m_pid = -1;
        }
    }
#endif
}

} // namespace lucid::platform
