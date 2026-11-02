#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace lucid {
class Window;
}

namespace lucid::vk {

class Device;
class Surface;

class Swapchain {
public:
    Swapchain(const Device& device, const Surface& surface, Window& window);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    Swapchain(Swapchain&&) = delete;
    Swapchain& operator=(Swapchain&&) = delete;

    void recreate(Window& window);

    [[nodiscard]] VkSwapchainKHR handle() const noexcept { return m_swapchain; }
    [[nodiscard]] VkFormat format() const noexcept { return m_format; }
    [[nodiscard]] VkExtent2D extent() const noexcept { return m_extent; }
    [[nodiscard]] VkRenderPass renderPass() const noexcept { return m_renderPass; }
    [[nodiscard]] VkRenderPass compositeRenderPass() const noexcept {
        return m_compositeRenderPass;
    }
    [[nodiscard]] const std::vector<VkFramebuffer>& compositeFramebuffers() const noexcept {
        return m_compositeFramebuffers;
    }
    [[nodiscard]] VkRenderPass overlayRenderPass() const noexcept {
        return m_overlayRenderPass;
    }
    [[nodiscard]] const std::vector<VkFramebuffer>& overlayFramebuffers() const noexcept {
        return m_overlayFramebuffers;
    }
    [[nodiscard]] std::uint32_t imageCount() const noexcept {
        return static_cast<std::uint32_t>(m_images.size());
    }
    [[nodiscard]] const std::vector<VkFramebuffer>& framebuffers() const noexcept {
        return m_framebuffers;
    }
    [[nodiscard]] VkSemaphore renderFinished(std::uint32_t imageIndex) const noexcept {
        return m_renderFinishedSemaphores[imageIndex];
    }

private:
    void create(Window& window);
    void createImageViews();
    void createRenderPass();
    void createFramebuffers();
    void createCompositeResources();
    void createOverlayResources();
    void createRenderFinishedSemaphores();
    void destroyResources();

    const Device& m_device;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent{};
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;
    VkRenderPass m_compositeRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_compositeFramebuffers;
    VkRenderPass m_overlayRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_overlayFramebuffers;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
};

} // namespace lucid::vk
