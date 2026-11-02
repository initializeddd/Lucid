#include "renderer/vulkan/Surface.hpp"

#include "platform/window/Window.hpp"
#include "renderer/vulkan/Instance.hpp"

namespace lucid::vk {

Surface::Surface(const Instance& instance, const Window& window) : m_instance(instance.handle()) {
    m_surface = window.createSurface(m_instance);
}

Surface::~Surface() {
    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    }
}

} // namespace lucid::vk
