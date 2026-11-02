#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace lucid::vk {

class Device;

class SceneTarget {
public:
    SceneTarget(const Device& device, VkFormat format, VkExtent2D extent);
    ~SceneTarget();

    SceneTarget(const SceneTarget&) = delete;
    SceneTarget& operator=(const SceneTarget&) = delete;
    SceneTarget(SceneTarget&&) = delete;
    SceneTarget& operator=(SceneTarget&&) = delete;

    [[nodiscard]] VkFormat format() const noexcept { return m_format; }
    [[nodiscard]] VkExtent2D extent() const noexcept { return m_extent; }
    [[nodiscard]] VkRenderPass renderPass() const noexcept { return m_renderPass; }
    [[nodiscard]] VkFramebuffer framebuffer() const noexcept { return m_framebuffer; }
    [[nodiscard]] VkImage image() const noexcept { return m_image; }
    [[nodiscard]] VkImageView view() const noexcept { return m_view; }
    [[nodiscard]] VkSampler sampler() const noexcept { return m_sampler; }

private:
    void destroy() noexcept;

    VkDevice m_device = VK_NULL_HANDLE;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent{};
    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkImageView m_view = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
};

} // namespace lucid::vk
