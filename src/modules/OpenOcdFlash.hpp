#pragma once
#include "modules/BoardProfiles.hpp"
#include "modules/ModuleProcess.hpp"
#include "workspace/Project.hpp"
#include <atomic>
#include <filesystem>
#include <string>
#include <optional>
#include <cstdint>

namespace lucid::modules {
struct FirmwareSnapshot { BoardProfile profile; std::string bytes, sha256, extension; std::optional<std::uint32_t> binaryAddress; std::string reviewDigest; };
// Read an explicit project-contained ELF/HEX or BIN file; BIN requires an explicit address. No hardware or process access.
[[nodiscard]] bool prepareFirmware(const workspace::Project& project, const BoardProfile& profile,
                                   FirmwareSnapshot& snapshot, std::string& error, const std::atomic_bool* cancel = nullptr,
                                   std::optional<std::uint32_t> binaryAddress = std::nullopt);
// Explicit user confirmation is required: runs the selected executable and its
// OpenOCD scripts, erases/writes/verifies hardware and may execute arbitrary code.
// Cancellation/failure may leave the device partially flashed. This is not a sandbox.
[[nodiscard]] ModuleProcessResult flashFirmware(const std::filesystem::path& executable,
                                                const FirmwareSnapshot& snapshot,
                                                const std::atomic_bool* cancel = nullptr);
}
