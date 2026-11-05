#include "modules/SerialCapture.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace lucid::modules {
namespace {
bool supportedBaud(unsigned baud) {
    switch (baud) { case 9600: case 19200: case 38400: case 57600: case 115200: return true; default: return false; }
}
}
SerialMonitorBuffer::SerialMonitorBuffer(std::size_t capacity) : data_(capacity <= 4U*1024U*1024U ? capacity : 0) {}
void SerialMonitorBuffer::append(std::string_view bytes) {
    std::lock_guard lock(mutex_); if (data_.empty()) return;
    for (char c:bytes) { if (size_<data_.size()) {data_[(start_+size_)%data_.size()]=c;++size_;} else {data_[start_]=c;start_=(start_+1)%data_.size();if(dropped_<std::numeric_limits<std::uint64_t>::max()) ++dropped_;} }
}
SerialMonitorSnapshot SerialMonitorBuffer::snapshot() const {
    std::lock_guard lock(mutex_); SerialMonitorSnapshot result; result.dropped=dropped_; result.bytes.reserve(size_);
    for (std::size_t i=0;i<size_;++i) result.bytes.push_back(data_[(start_+i)%data_.size()]);
    return result;
}
namespace {
SerialCaptureResult runSerial(const std::filesystem::path& device, unsigned baud,
                              std::chrono::milliseconds duration, std::size_t limit,
                              const std::atomic_bool* cancel, SerialMonitorBuffer* monitor) {
    SerialCaptureResult result;
    if (cancel && cancel->load()) { result.cancelled = true; return result; }
    if (!device.is_absolute() || !supportedBaud(baud) || duration.count() < 1 || duration > (monitor ? std::chrono::hours(24) : std::chrono::seconds(60)) || !limit || limit > 4U * 1024U * 1024U) {
        result.error = "Invalid serial capture options"; return result;
    }
#ifdef _WIN32
    const auto path = device.wstring();
    if (!path.starts_with(L"\\\\.\\COM") || path.size() > 12) { result.error = "Select a Windows COM port"; return result; }
    const auto suffix = path.substr(7);
    if (suffix.empty() || suffix == L"0" || !std::all_of(suffix.begin(), suffix.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; })) { result.error = "Select a Windows COM port"; return result; }
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) { result.error = "Could not open serial port"; return result; }
    DCB original{}; original.DCBlength = sizeof original; COMMTIMEOUTS timeouts{};
    if (!GetCommState(handle, &original) || !GetCommTimeouts(handle, &timeouts)) { CloseHandle(handle); result.error = "Could not inspect serial settings"; return result; }
    DCB settings = original; settings.BaudRate = baud; settings.ByteSize = 8; settings.Parity = NOPARITY; settings.StopBits = ONESTOPBIT;
    settings.fBinary = TRUE; settings.fParity = FALSE; settings.fOutxCtsFlow = FALSE; settings.fOutxDsrFlow = FALSE; settings.fOutX = FALSE; settings.fInX = FALSE;
    COMMTIMEOUTS capture{}; capture.ReadIntervalTimeout = MAXDWORD; capture.ReadTotalTimeoutConstant = 1;
    if (!SetCommState(handle, &settings) || !SetCommTimeouts(handle, &capture)) result.error = "Could not configure serial port";
    const auto deadline = std::chrono::steady_clock::now() + duration;
    std::array<char, 4096> buffer{};
    while (result.error.empty() && std::chrono::steady_clock::now() < deadline) {
        if (cancel && cancel->load()) { result.cancelled = true; break; }
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0) break;
        capture.ReadTotalTimeoutConstant = static_cast<DWORD>(std::clamp<decltype(remaining)>(remaining, 1, 50));
        if (!SetCommTimeouts(handle, &capture)) { result.error = "Could not configure serial timeout"; break; }
        DWORD read = 0; const auto cap = static_cast<DWORD>((monitor ? buffer.size() : std::min(buffer.size(), limit - result.bytes.size())));
        if (!ReadFile(handle, buffer.data(), cap, &read, nullptr)) { result.error = "Could not read serial port"; break; }
        if (monitor) monitor->append(std::string_view(buffer.data(),read)); else result.bytes.append(buffer.data(), read);
        if (!monitor && result.bytes.size() == limit) { result.truncated = true; break; }
    }
    if (!SetCommState(handle, &original)) result.restoreFailed = true;
    if (!SetCommTimeouts(handle, &timeouts)) result.restoreFailed = true;
    CloseHandle(handle);
#else
    speed_t speed = B9600;
    switch (baud) { case 19200: speed = B19200; break; case 38400: speed = B38400; break; case 57600: speed = B57600; break; case 115200: speed = B115200; break; default: break; }
    int flags = O_RDWR | O_NOCTTY | O_NONBLOCK;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    const int fd = open(device.c_str(), flags);
    if (fd < 0) { result.error = "Could not open serial port"; return result; }
    struct stat metadata{}; termios original{};
    if (fstat(fd, &metadata) != 0 || !S_ISCHR(metadata.st_mode) || !isatty(fd) || tcgetattr(fd, &original) != 0) {
        close(fd); result.error = "Selected path is not a serial terminal"; return result;
    }
    termios settings = original; cfmakeraw(&settings); settings.c_cflag |= CLOCAL | CREAD; settings.c_cflag &= ~(PARENB | CSTOPB | CSIZE); settings.c_cflag |= CS8;
#ifdef CRTSCTS
    settings.c_cflag &= ~CRTSCTS;
#endif
    settings.c_cc[VMIN] = 0; settings.c_cc[VTIME] = 0;
    if (cfsetispeed(&settings, speed) != 0 || cfsetospeed(&settings, speed) != 0 || tcsetattr(fd, TCSANOW, &settings) != 0) result.error = "Could not configure serial port";
    const auto deadline = std::chrono::steady_clock::now() + duration;
    std::array<char, 4096> buffer{};
    while (result.error.empty() && std::chrono::steady_clock::now() < deadline) {
        if (cancel && cancel->load()) { result.cancelled = true; break; }
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        pollfd event{fd, POLLIN, 0}; const int ready = poll(&event, 1, static_cast<int>(std::clamp<decltype(remaining)>(remaining, 0, 50)));
        if (ready < 0) { if (errno == EINTR) continue; result.error = "Could not poll serial port"; break; }
        if (!(event.revents & POLLIN)) {
            if (event.revents & (POLLERR | POLLNVAL | POLLHUP)) { result.error = "Serial port disconnected"; break; }
            continue;
        }
        const auto n = read(fd, buffer.data(), (monitor ? buffer.size() : std::min(buffer.size(), limit - result.bytes.size())));
        if (n < 0) { if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue; result.error = "Could not read serial port"; break; }
        if (n > 0) { if (monitor) monitor->append(std::string_view(buffer.data(),static_cast<std::size_t>(n))); else result.bytes.append(buffer.data(), static_cast<std::size_t>(n)); }
        if (!monitor && result.bytes.size() == limit) { result.truncated = true; break; }
        if (event.revents & (POLLERR | POLLNVAL | POLLHUP)) { result.error = "Serial port disconnected"; break; }
    }
    if (tcsetattr(fd, TCSANOW, &original) != 0) result.restoreFailed = true;
    close(fd);
#endif
    return result;
}
}
SerialCaptureResult captureSerial(const std::filesystem::path& device, unsigned baud, std::chrono::milliseconds duration, std::size_t limit, const std::atomic_bool* cancel) {
    return runSerial(device,baud,duration,limit,cancel,nullptr);
}
SerialCaptureResult monitorSerial(const std::filesystem::path& device, unsigned baud, std::chrono::milliseconds duration, SerialMonitorBuffer& buffer, const std::atomic_bool* cancel) {
    return runSerial(device,baud,duration,buffer.capacity(),cancel,&buffer);
}
}
