#pragma once

#include <cstddef>
#include <chrono>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::platform {

class PtyProcess {
public:
    PtyProcess() = default;
    ~PtyProcess();

    PtyProcess(const PtyProcess&) = delete;
    PtyProcess& operator=(const PtyProcess&) = delete;
    PtyProcess(PtyProcess&&) = delete;
    PtyProcess& operator=(PtyProcess&&) = delete;

    [[nodiscard]] bool start(std::string workingDirectory,
                             std::vector<std::string> commands,
                             std::string& error);
    void update();
    void write(std::string_view text);
    void appendEcho(std::string_view text);
    void close() noexcept;

    [[nodiscard]] bool alive() const noexcept { return m_alive; }
    [[nodiscard]] bool commandsDone() const noexcept { return m_commandsDone; }
    [[nodiscard]] bool hasProcess() const noexcept { return m_fd >= 0 || m_pid > 0; }
    [[nodiscard]] std::string_view output() const noexcept { return m_output; }
    [[nodiscard]] const std::vector<int>& exitCodes() const noexcept { return m_exitCodes; }

private:
    void sendNextCommand();
    void flushInput();
    void pumpOutput();
    void reap() noexcept;

    int m_fd = -1;
    int m_pid = -1;
    bool m_alive = false;
    bool m_commandsDone = false;
    bool m_waitingForEcho = false;
    std::size_t m_nextCommand = 0;
    std::vector<std::string> m_commands;
    std::vector<int> m_exitCodes;
    std::string m_marker;
    std::string m_pendingInput;
    std::string m_rawOutput;
    std::string m_output;
    std::chrono::steady_clock::time_point m_commandStarted{};
};

} // namespace lucid::platform
