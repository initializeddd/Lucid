#include "modules/SerialDevices.hpp"
#include <algorithm>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace lucid::modules {
namespace fs = std::filesystem;
namespace {
constexpr std::size_t maxEntries = 10000, maxDevices = 1024;
bool digits(std::string_view s) { return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; }); }
bool suffixNumber(std::string_view name, std::string_view prefix) { return name.starts_with(prefix) && digits(name.substr(prefix.size())); }
}
bool isSerialDeviceName(SerialDevicePlatform platform, std::string_view name) {
    switch (platform) {
    case SerialDevicePlatform::Windows: return suffixNumber(name, "COM") && name.substr(3) != "0" && name.size() <= 8;
    case SerialDevicePlatform::Linux: return suffixNumber(name, "ttyUSB") || suffixNumber(name, "ttyACM") || suffixNumber(name, "ttyS") || suffixNumber(name, "ttyAMA");
    case SerialDevicePlatform::MacOS:
        return (name.starts_with("cu.usbserial") && name.size() > 12) || (name.starts_with("cu.usbmodem") && name.size() > 11);
    }
    return false;
}
SerialDeviceScan scanSerialDeviceDirectory(SerialDevicePlatform platform, const fs::path& directory) {
    SerialDeviceScan scan;
    if (platform == SerialDevicePlatform::Windows) { scan.error = "Directory scanning is unsupported for Windows ports."; return scan; }
    std::error_code ec; fs::directory_iterator it(directory, fs::directory_options::skip_permission_denied, ec), end;
    if (ec) { scan.error = "Could not read the device directory."; return scan; }
    std::size_t examined = 0;
    for (; it != end && !ec; it.increment(ec)) {
        if (examined++ >= maxEntries) { scan.incomplete = true; break; }
        auto name = it->path().filename().string();
        if (!isSerialDeviceName(platform, name)) continue;
        std::error_code statusError;
        if (it->is_directory(statusError) || statusError) continue;
        if (scan.devices.size() >= maxDevices) { scan.incomplete = true; break; }
        scan.devices.push_back({name, it->path()});
    }
    if (ec) { scan.incomplete = true; scan.error = "Device directory could not be fully read."; }
    std::sort(scan.devices.begin(), scan.devices.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    return scan;
}
SerialDeviceScan discoverSerialDevices() {
#ifdef _WIN32
    SerialDeviceScan scan; std::vector<wchar_t> buffer(65536);
    auto size = QueryDosDeviceW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!size) { scan.error = "Could not enumerate Windows DOS devices."; return scan; }
    std::size_t examined = 0;
    for (std::size_t i = 0; i < size && buffer[i];) {
        std::wstring name; while (i < size && buffer[i]) name += buffer[i++]; ++i;
        if (++examined > maxEntries) { scan.incomplete = true; break; }
        std::string ascii; bool ok = true;
        for (wchar_t c : name) { if (c > 127) { ok = false; break; } ascii.push_back(static_cast<char>(c)); }
        if (!ok || !isSerialDeviceName(SerialDevicePlatform::Windows, ascii)) continue;
        if (scan.devices.size() >= maxDevices) { scan.incomplete = true; break; }
        scan.devices.push_back({ascii, fs::path(L"\\\\.\\" + name)});
    }
    std::sort(scan.devices.begin(), scan.devices.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    return scan;
#elif defined(__APPLE__)
    return scanSerialDeviceDirectory(SerialDevicePlatform::MacOS, "/dev");
#elif defined(__linux__)
    return scanSerialDeviceDirectory(SerialDevicePlatform::Linux, "/dev");
#else
    SerialDeviceScan scan; scan.error = "Serial device discovery is unsupported on this platform."; return scan;
#endif
}
}
