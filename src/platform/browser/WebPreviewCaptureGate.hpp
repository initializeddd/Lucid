#pragma once
#include <chrono>
#include <cstdint>
#include <optional>
namespace lucid::platform {
// Coordinates snapshot timing on the UI thread. Navigation completion starts a
// short settling interval; a newer navigation invalidates pending and in-flight
// captures. This is still a point-in-time snapshot, not an async-idle guarantee.
class WebPreviewCaptureGate {
public:
    using Clock=std::chrono::steady_clock;
    void navigationStarted(std::uint64_t id) noexcept { current_=id; ++generation_; pending_.reset(); }
    void navigationCompleted(std::uint64_t id,Clock::time_point now) noexcept {
        if (current_ && *current_==id) pending_=now+std::chrono::milliseconds(100);
    }
    [[nodiscard]] std::optional<std::uint64_t> takeReady(Clock::time_point now) noexcept {
        if (!current_ || !pending_ || now<*pending_) return std::nullopt;
        pending_.reset(); return generation_;
    }
    [[nodiscard]] bool isCurrentNavigation(std::uint64_t id) const noexcept { return current_ && *current_==id; }
    [[nodiscard]] bool isCurrentCapture(std::uint64_t generation) const noexcept { return current_ && generation_==generation; }
    void cancel() noexcept { current_.reset(); pending_.reset(); ++generation_; }
private:
    std::optional<std::uint64_t> current_;
    std::optional<Clock::time_point> pending_;
    std::uint64_t generation_=0;
};
}
