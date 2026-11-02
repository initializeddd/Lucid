#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace lucid::vk {

class Instance {
public:
    Instance(std::string_view appName, std::uint32_t appVersion, bool enableValidation,
             const std::vector<const char*>& requiredExtensions);
    ~Instance();

    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    Instance(Instance&&) = delete;
    Instance& operator=(Instance&&) = delete;

    [[nodiscard]] VkInstance handle() const noexcept { return m_instance; }
    [[nodiscard]] bool validationEnabled() const noexcept { return m_validationEnabled; }

private:
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData);

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    PFN_vkDestroyDebugUtilsMessengerEXT m_destroyDebugMessenger = nullptr;
    bool m_validationEnabled = false;
};

} // namespace lucid::vk
