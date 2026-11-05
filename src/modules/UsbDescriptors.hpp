#pragma once
#include "modules/BoardIdentification.hpp"
#include <filesystem>
#include <string>
#include <vector>
namespace lucid::modules {
struct UsbDescriptorDevice { std::string location; ObservedUsbDevice ids; };
struct UsbDescriptorScan { std::vector<UsbDescriptorDevice> devices; std::string error; bool incomplete = false; };
// Reads Linux sysfs USB descriptors without opening a device. deviceRoot is the
// trusted sysfs devices root; entries resolving outside it are not read.
[[nodiscard]] UsbDescriptorScan scanLinuxUsbDescriptors(const std::filesystem::path& busDirectory, const std::filesystem::path& deviceRoot);
// Validates raw OS descriptor values; never infers IDs from names.
[[nodiscard]] bool validateUsbDescriptor(std::int64_t vendor, std::int64_t product, ObservedUsbDevice& output);
// Extracts VID/PID from a Windows USB hardware ID; serial names are not accepted.
[[nodiscard]] bool parseWindowsUsbHardwareId(std::string_view hardwareId, ObservedUsbDevice& output);
// Linux, macOS and Windows read-only OS descriptor discovery.
[[nodiscard]] UsbDescriptorScan discoverUsbDescriptors();
}
