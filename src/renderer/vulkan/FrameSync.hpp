#pragma once

#include <vulkan/vulkan.h>

namespace lucid::vk {

class Device;

class FrameSync {
public:
    FrameSync() = default;
    ~FrameSync();

    FrameSync(const FrameSync&) = delete;
    FrameSync& operator=(const FrameSync&) = delete;
    FrameSync(FrameSync&&) = delete;
    FrameSync& operator=(FrameSync&&) = delete;

    void init(const Device& device);

    [[nodiscard]] VkSemaphore imageAvailable() const noexcept { return m_imageAvailable; }
    [[nodiscard]] VkFence inFlight() const noexcept { return m_inFlight; }

    void wait() const;
    void reset() const;

private:
    void destroy() noexcept;

    VkDevice m_device = VK_NULL_HANDLE;
    VkSemaphore m_imageAvailable = VK_NULL_HANDLE;
    VkFence m_inFlight = VK_NULL_HANDLE;
};

} // namespace lucid::vk
