#pragma once
#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <mutex>
#include <vector>
#include <cstdint>

namespace lucid::modules {
struct SerialCaptureResult {
    std::string bytes, error;
    bool cancelled = false, truncated = false, restoreFailed = false;
};
struct SerialMonitorSnapshot { std::string bytes; std::uint64_t dropped = 0; };
class SerialMonitorBuffer {
public:
    explicit SerialMonitorBuffer(std::size_t capacity);
    [[nodiscard]] std::size_t capacity() const noexcept { return data_.size(); }
    void append(std::string_view bytes);
    [[nodiscard]] SerialMonitorSnapshot snapshot() const;
private:
    mutable std::mutex mutex_;
    std::vector<char> data_;
    std::size_t start_ = 0, size_ = 0;
    std::uint64_t dropped_ = 0;
};
// Opens once and monitors on the calling worker thread until cancellation or the
// bounded duration (maximum 24 hours). The thread-safe buffer retains only its
// latest capacity bytes and counts overwritten bytes. Consent and port caveats
// below apply; no text encoding or application data transmission is assumed.
[[nodiscard]] SerialCaptureResult monitorSerial(const std::filesystem::path& device, unsigned baud,
    std::chrono::milliseconds duration, SerialMonitorBuffer& buffer, const std::atomic_bool* cancel = nullptr);
// Explicitly opens a selected device for bounded, read-only capture at 8N1.
// Opening or configuring a serial device may reset attached hardware or affect
// another process; callers must explain this and obtain consent. The original
// port settings are restored best-effort. No application data is sent. Opening
// a port can itself change modem-control signals (including DTR/RTS); it cannot
// guarantee the attached device is unchanged. POSIX symlink paths are rejected.
// Run off the UI thread. Duration is 1ms–60s; output limit is 1 byte–4 MiB.
[[nodiscard]] SerialCaptureResult captureSerial(const std::filesystem::path& device, unsigned baud,
    std::chrono::milliseconds duration, std::size_t limit = 1024 * 1024,
    const std::atomic_bool* cancel = nullptr);
}
