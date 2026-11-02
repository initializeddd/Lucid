#include "renderer/vulkan/Swapchain.hpp"

#include "logging/Log.hpp"
#include "platform/window/Window.hpp"
#include "renderer/vulkan/Device.hpp"
#include "renderer/vulkan/Surface.hpp"
#include "renderer/vulkan/VulkanCommon.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace lucid::vk {
namespace {

struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

[[nodiscard]] SwapchainSupport querySupport(const Device& device, VkSurfaceKHR surface) {
    SwapchainSupport support;

    LUCID_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physical(), surface,
                                                             &support.capabilities));

    std::uint32_t formatCount = 0;
    LUCID_VK_CHECK(
        vkGetPhysicalDeviceSurfaceFormatsKHR(device.physical(), surface, &formatCount, nullptr));
    if (formatCount == 0) {
        throw std::runtime_error("The window surface exposes no color formats");
    }
    support.formats.resize(formatCount);
    LUCID_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device.physical(), surface, &formatCount,
                                                        support.formats.data()));
    support.formats.resize(formatCount);

    std::uint32_t presentModeCount = 0;
    LUCID_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device.physical(), surface,
                                                             &presentModeCount, nullptr));
    if (presentModeCount == 0) {
        throw std::runtime_error("The window surface exposes no presentation modes");
    }
    support.presentModes.resize(presentModeCount);
    LUCID_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        device.physical(), surface, &presentModeCount, support.presentModes.data()));
    support.presentModes.resize(presentModeCount);

    return support;
}

[[nodiscard]] VkSurfaceFormatKHR chooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats) {
    if (formats.size() == 1 && formats.front().format == VK_FORMAT_UNDEFINED) {
        return {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    for (const VkSurfaceFormatKHR& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    for (const VkSurfaceFormatKHR& format : formats) {
        if (isSrgbFormat(format.format) &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    for (const VkSurfaceFormatKHR& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM) {
            return format;
        }
    }

    return formats.front();
}

[[nodiscard]] VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes) {
    const auto fifo = std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_FIFO_KHR);
    if (fifo != modes.end()) {
        return *fifo;
    }
    return modes.front();
}

[[nodiscard]] VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                                     const Window& window) {
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width = 0;
    int height = 0;
    window.framebufferSize(width, height);

    VkExtent2D extent{static_cast<std::uint32_t>(std::max(width, 1)),
                      static_cast<std::uint32_t>(std::max(height, 1))};
    extent.width =
        std::clamp(extent.width, capabilities.minImageExtent.width,
                   capabilities.maxImageExtent.width);
    extent.height =
        std::clamp(extent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);
    return extent;
}

[[nodiscard]] VkCompositeAlphaFlagBitsKHR chooseCompositeAlpha(
    const VkSurfaceCapabilitiesKHR& capabilities) {
    constexpr VkCompositeAlphaFlagBitsKHR candidates[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };

    for (const VkCompositeAlphaFlagBitsKHR candidate : candidates) {
        if ((capabilities.supportedCompositeAlpha & candidate) != 0U) {
            return candidate;
        }
    }
    return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
}

} // namespace

Swapchain::Swapchain(const Device& device, const Surface& surface, Window& window)
    : m_device(device), m_surface(surface.handle()) {
    create(window);
}

Swapchain::~Swapchain() {
    destroyResources();
}

void Swapchain::recreate(Window& window) {
    m_device.waitIdle();
    destroyResources();
    create(window);
}

void Swapchain::create(Window& window) {
    try {
        const SwapchainSupport support = querySupport(m_device, m_surface);
        const VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(support.formats);
        const VkPresentModeKHR presentMode = choosePresentMode(support.presentModes);
        m_format = surfaceFormat.format;
        m_extent = chooseExtent(support.capabilities, window);

        std::uint32_t imageCount = support.capabilities.minImageCount + 1;
        if (support.capabilities.maxImageCount > 0 &&
            imageCount > support.capabilities.maxImageCount) {
            imageCount = support.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = m_extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.preTransform = support.capabilities.currentTransform;
        createInfo.compositeAlpha = chooseCompositeAlpha(support.capabilities);
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        const std::uint32_t graphicsFamily = m_device.graphicsFamily();
        const std::uint32_t presentFamily = m_device.presentFamily();
        const std::uint32_t queueFamilies[] = {graphicsFamily, presentFamily};
        if (graphicsFamily != presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilies;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        LUCID_VK_CHECK(vkCreateSwapchainKHR(m_device.handle(), &createInfo, nullptr, &m_swapchain));

        std::uint32_t swapchainImageCount = 0;
        LUCID_VK_CHECK(vkGetSwapchainImagesKHR(m_device.handle(), m_swapchain,
                                               &swapchainImageCount, nullptr));
        m_images.resize(swapchainImageCount);
        LUCID_VK_CHECK(vkGetSwapchainImagesKHR(m_device.handle(), m_swapchain,
                                               &swapchainImageCount, m_images.data()));
        m_images.resize(swapchainImageCount);

        createImageViews();
        createRenderPass();
        createFramebuffers();
        createCompositeResources();
        createOverlayResources();
        createRenderFinishedSemaphores();

        log::info("Swapchain: " + std::to_string(m_extent.width) + "x" +
                  std::to_string(m_extent.height) + ", " + std::to_string(m_images.size()) +
                  " images.");
    } catch (...) {
        destroyResources();
        throw;
    }
}

void Swapchain::createCompositeResources() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorReference{};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    LUCID_VK_CHECK(vkCreateRenderPass(m_device.handle(), &renderPassInfo, nullptr,
                                      &m_compositeRenderPass));

    m_compositeFramebuffers.resize(m_imageViews.size(), VK_NULL_HANDLE);
    for (std::size_t index = 0; index < m_imageViews.size(); ++index) {
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_compositeRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &m_imageViews[index];
        framebufferInfo.width = m_extent.width;
        framebufferInfo.height = m_extent.height;
        framebufferInfo.layers = 1;
        LUCID_VK_CHECK(vkCreateFramebuffer(m_device.handle(), &framebufferInfo,
                                           nullptr, &m_compositeFramebuffers[index]));
    }
}

void Swapchain::createOverlayResources() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorReference{};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    LUCID_VK_CHECK(vkCreateRenderPass(m_device.handle(), &renderPassInfo, nullptr,
                                      &m_overlayRenderPass));

    m_overlayFramebuffers.resize(m_imageViews.size(), VK_NULL_HANDLE);
    for (std::size_t index = 0; index < m_imageViews.size(); ++index) {
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_overlayRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &m_imageViews[index];
        framebufferInfo.width = m_extent.width;
        framebufferInfo.height = m_extent.height;
        framebufferInfo.layers = 1;
        LUCID_VK_CHECK(vkCreateFramebuffer(m_device.handle(), &framebufferInfo,
                                           nullptr, &m_overlayFramebuffers[index]));
    }
}

void Swapchain::createImageViews() {
    m_imageViews.resize(m_images.size(), VK_NULL_HANDLE);

    for (std::size_t index = 0; index < m_images.size(); ++index) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_images[index];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        LUCID_VK_CHECK(vkCreateImageView(m_device.handle(), &createInfo, nullptr,
                                         &m_imageViews[index]));
    }
}

void Swapchain::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorReference{};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &colorAttachment;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dependency;

    LUCID_VK_CHECK(vkCreateRenderPass(m_device.handle(), &createInfo, nullptr, &m_renderPass));
}

void Swapchain::createFramebuffers() {
    m_framebuffers.resize(m_imageViews.size(), VK_NULL_HANDLE);

    for (std::size_t index = 0; index < m_imageViews.size(); ++index) {
        VkFramebufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = m_renderPass;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &m_imageViews[index];
        createInfo.width = m_extent.width;
        createInfo.height = m_extent.height;
        createInfo.layers = 1;

        LUCID_VK_CHECK(
            vkCreateFramebuffer(m_device.handle(), &createInfo, nullptr, &m_framebuffers[index]));
    }
}

void Swapchain::createRenderFinishedSemaphores() {
    m_renderFinishedSemaphores.resize(m_images.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (VkSemaphore& semaphore : m_renderFinishedSemaphores) {
        LUCID_VK_CHECK(vkCreateSemaphore(m_device.handle(), &createInfo, nullptr, &semaphore));
    }
}

void Swapchain::destroyResources() {
    for (const VkSemaphore semaphore : m_renderFinishedSemaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device.handle(), semaphore, nullptr);
        }
    }
    m_renderFinishedSemaphores.clear();

    for (const VkFramebuffer framebuffer : m_framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_device.handle(), framebuffer, nullptr);
        }
    }
    m_framebuffers.clear();

    for (VkFramebuffer& framebuffer : m_overlayFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_device.handle(), framebuffer, nullptr);
        }
    }
    m_overlayFramebuffers.clear();

    if (m_overlayRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device.handle(), m_overlayRenderPass, nullptr);
        m_overlayRenderPass = VK_NULL_HANDLE;
    }

    for (VkFramebuffer& framebuffer : m_compositeFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_device.handle(), framebuffer, nullptr);
        }
    }
    m_compositeFramebuffers.clear();

    if (m_compositeRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device.handle(), m_compositeRenderPass, nullptr);
        m_compositeRenderPass = VK_NULL_HANDLE;
    }

    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device.handle(), m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }

    for (const VkImageView view : m_imageViews) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device.handle(), view, nullptr);
        }
    }
    m_imageViews.clear();
    m_images.clear();

    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device.handle(), m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

} // namespace lucid::vk
