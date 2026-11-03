#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace lucid::debug {
class AdapterProcess {
public:
    AdapterProcess() = default;
    ~AdapterProcess();
    AdapterProcess(const AdapterProcess&) = delete;
    AdapterProcess& operator=(const AdapterProcess&) = delete;
    [[nodiscard]] bool startDefault(std::string& error);
    [[nodiscard]] bool start(const std::string& executable, const std::vector<std::string>& arguments, std::string& error);
    [[nodiscard]] bool send(std::string_view bytes, std::string& error);
    [[nodiscard]] bool poll(std::string& output, std::string& error);
    [[nodiscard]] bool alive() const noexcept { return m_alive; }
    void close() noexcept;
private:
#if defined(_WIN32)
    struct WindowsState;
    WindowsState* m_windows = nullptr;
#endif
    int m_fd = -1;
    int m_pid = -1;
    bool m_alive = false;
    std::string m_pending;
};
}
