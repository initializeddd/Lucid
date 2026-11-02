#pragma once

#include <vulkan/vulkan.h>

namespace lucid {
class Window;
}

namespace lucid::vk {

class Instance;

class Surface {
public:
    Surface(const Instance& instance, const Window& window);
    ~Surface();

    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;
    Surface(Surface&&) = delete;
    Surface& operator=(Surface&&) = delete;

    [[nodiscard]] VkSurfaceKHR handle() const noexcept { return m_surface; }

private:
    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
};

} // namespace lucid::vk
