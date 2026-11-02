#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace lucid::vk {

class Instance;
class Surface;

struct QueueFamilyIndices {
    std::optional<std::uint32_t> graphics;
    std::optional<std::uint32_t> present;

    [[nodiscard]] bool complete() const noexcept { return graphics.has_value() && present.has_value(); }
};

class Device {
public:
    Device(const Instance& instance, const Surface& surface);
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    [[nodiscard]] VkPhysicalDevice physical() const noexcept { return m_physical; }
    [[nodiscard]] VkDevice handle() const noexcept { return m_device; }
    [[nodiscard]] VkQueue graphicsQueue() const noexcept { return m_graphicsQueue; }
    [[nodiscard]] VkQueue presentQueue() const noexcept { return m_presentQueue; }
    [[nodiscard]] std::uint32_t graphicsFamily() const noexcept { return *m_families.graphics; }
    [[nodiscard]] std::uint32_t presentFamily() const noexcept { return *m_families.present; }
    [[nodiscard]] const VkPhysicalDeviceProperties& properties() const noexcept {
        return m_properties;
    }

    [[nodiscard]] std::uint32_t findMemoryType(std::uint32_t typeFilter,
                                               VkMemoryPropertyFlags properties) const;

    void waitIdle() const;

private:
    static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physical, VkSurfaceKHR surface);
    static bool supportsExtensions(VkPhysicalDevice physical,
                                   const std::vector<const char*>& required);
    static bool supportsExtension(VkPhysicalDevice physical, const char* name);

    VkPhysicalDevice m_physical = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    QueueFamilyIndices m_families;
    VkPhysicalDeviceProperties m_properties{};
};

} // namespace lucid::vk
