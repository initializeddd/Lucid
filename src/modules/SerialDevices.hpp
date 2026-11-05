#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
enum class SerialDevicePlatform { Linux, MacOS, Windows };
struct SerialDevice { std::string name; std::filesystem::path path; };
struct SerialDeviceScan { std::vector<SerialDevice> devices; bool incomplete = false; std::string error; };
// Conservative name-based match; does not identify hardware, availability or ownership.
[[nodiscard]] bool isSerialDeviceName(SerialDevicePlatform platform, std::string_view name);
// POSIX directory scan for testing and custom device roots. Never opens devices.
[[nodiscard]] SerialDeviceScan scanSerialDeviceDirectory(SerialDevicePlatform platform, const std::filesystem::path& directory);
// Scan /dev on Linux/macOS or the Windows DOS device namespace. No port is opened.
[[nodiscard]] SerialDeviceScan discoverSerialDevices();
}
