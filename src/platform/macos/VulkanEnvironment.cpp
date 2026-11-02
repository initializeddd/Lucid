#include "platform/macos/VulkanEnvironment.hpp"

#include <cstdlib>

#if defined(__APPLE__)
#include <filesystem>
#include <vector>
#endif

namespace lucid::platform {

void configureVulkanDriverEnvironment() {
#if defined(__APPLE__)
    if (std::getenv("VK_ICD_FILENAMES") != nullptr || std::getenv("VK_DRIVER_FILES") != nullptr) {
        return;
    }

    std::vector<std::filesystem::path> candidates;
    if (const char* sdk = std::getenv("VULKAN_SDK"); sdk != nullptr) {
        candidates.emplace_back(std::filesystem::path(sdk) / "share/vulkan/icd.d/MoltenVK_icd.json");
    }
    candidates.emplace_back("/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json");
    candidates.emplace_back("/usr/local/share/vulkan/icd.d/MoltenVK_icd.json");

    for (const std::filesystem::path& candidate : candidates) {
        std::error_code error;
        if (!std::filesystem::exists(candidate, error) || error) {
            continue;
        }

        setenv("VK_ICD_FILENAMES", candidate.c_str(), 1);
        return;
    }
#endif
}

} // namespace lucid::platform
