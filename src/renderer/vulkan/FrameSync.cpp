#include "renderer/vulkan/FrameSync.hpp"

#include "renderer/vulkan/Device.hpp"
#include "renderer/vulkan/VulkanCommon.hpp"

#include <cstdint>

namespace lucid::vk {

FrameSync::~FrameSync() {
    destroy();
}

void FrameSync::init(const Device& device) {
    m_device = device.handle();

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    try {
        LUCID_VK_CHECK(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailable));
        LUCID_VK_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlight));
    } catch (...) {
        destroy();
        m_device = VK_NULL_HANDLE;
        throw;
    }
}

void FrameSync::wait() const {
    LUCID_VK_CHECK(vkWaitForFences(m_device, 1, &m_inFlight, VK_TRUE, UINT64_MAX));
}

void FrameSync::reset() const {
    LUCID_VK_CHECK(vkResetFences(m_device, 1, &m_inFlight));
}

void FrameSync::destroy() noexcept {
    if (m_device == VK_NULL_HANDLE) {
        return;
    }

    if (m_inFlight != VK_NULL_HANDLE) {
        vkDestroyFence(m_device, m_inFlight, nullptr);
        m_inFlight = VK_NULL_HANDLE;
    }
    if (m_imageAvailable != VK_NULL_HANDLE) {
        vkDestroySemaphore(m_device, m_imageAvailable, nullptr);
        m_imageAvailable = VK_NULL_HANDLE;
    }
}

} // namespace lucid::vk
