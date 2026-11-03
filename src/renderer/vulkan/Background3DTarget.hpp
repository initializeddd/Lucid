#pragma once
#include <vulkan/vulkan.h>
namespace lucid::vk {
class Device;
// A separate color/depth render target: it leaves the editor's UI render pass
// unchanged and can be sampled when composing the background.
class Background3DTarget {
public:
    Background3DTarget(const Device& device, VkFormat colorFormat, VkExtent2D extent);
    ~Background3DTarget();
    Background3DTarget(const Background3DTarget&) = delete;
    Background3DTarget& operator=(const Background3DTarget&) = delete;
    [[nodiscard]] VkRenderPass renderPass() const noexcept { return m_renderPass; }
    [[nodiscard]] VkFramebuffer framebuffer() const noexcept { return m_framebuffer; }
    [[nodiscard]] VkImageView colorView() const noexcept { return m_colorView; }
    [[nodiscard]] VkImage colorImage() const noexcept { return m_color; }
    [[nodiscard]] VkSampler sampler() const noexcept { return m_sampler; }
    [[nodiscard]] VkSampler linearSampler() const noexcept { return m_linearSampler; }
    [[nodiscard]] VkExtent2D extent() const noexcept { return m_extent; }
private:
    void destroy() noexcept;
    VkDevice m_device = VK_NULL_HANDLE;
    VkExtent2D m_extent{};
    VkImage m_color = VK_NULL_HANDLE, m_depth = VK_NULL_HANDLE;
    VkDeviceMemory m_colorMemory = VK_NULL_HANDLE, m_depthMemory = VK_NULL_HANDLE;
    VkImageView m_colorView = VK_NULL_HANDLE, m_depthView = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkSampler m_linearSampler = VK_NULL_HANDLE;
};
}
