#include "renderer/vulkan/Background3DTarget.hpp"
#include "renderer/vulkan/Device.hpp"
#include "renderer/vulkan/VulkanCommon.hpp"
#include <array>
#include <stdexcept>
namespace lucid::vk {
Background3DTarget::Background3DTarget(const Device& device, VkFormat colorFormat, VkExtent2D extent)
    : m_device(device.handle()), m_extent(extent) {
    if (!extent.width || !extent.height) throw std::runtime_error("Invalid 3D background extent");
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    for (const auto candidate : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM}) {
        VkFormatProperties props{}; vkGetPhysicalDeviceFormatProperties(device.physical(), candidate, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) { depthFormat = candidate; break; }
    }
    if (depthFormat == VK_FORMAT_UNDEFINED) throw std::runtime_error("No supported depth format for 3D backgrounds");
    try {
        const auto createImage = [&](VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect,
                                     VkImage& image, VkDeviceMemory& memory, VkImageView& view) {
            VkImageCreateInfo info{}; info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            info.imageType = VK_IMAGE_TYPE_2D; info.format = format; info.extent = {extent.width, extent.height, 1};
            info.mipLevels = 1; info.arrayLayers = 1; info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.tiling = VK_IMAGE_TILING_OPTIMAL; info.usage = usage; info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            LUCID_VK_CHECK(vkCreateImage(m_device, &info, nullptr, &image));
            VkMemoryRequirements requirements{}; vkGetImageMemoryRequirements(m_device, image, &requirements);
            VkMemoryAllocateInfo allocation{}; allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = device.findMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            LUCID_VK_CHECK(vkAllocateMemory(m_device, &allocation, nullptr, &memory));
            LUCID_VK_CHECK(vkBindImageMemory(m_device, image, memory, 0));
            VkImageViewCreateInfo viewInfo{}; viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = image; viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; viewInfo.format = format;
            viewInfo.subresourceRange.aspectMask = aspect; viewInfo.subresourceRange.levelCount = 1; viewInfo.subresourceRange.layerCount = 1;
            LUCID_VK_CHECK(vkCreateImageView(m_device, &viewInfo, nullptr, &view));
        };
        createImage(colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT, m_color, m_colorMemory, m_colorView);
        createImage(depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    VK_IMAGE_ASPECT_DEPTH_BIT, m_depth, m_depthMemory, m_depthView);
        VkSamplerCreateInfo sampler{}; sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler.magFilter = VK_FILTER_NEAREST; sampler.minFilter = VK_FILTER_NEAREST;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        LUCID_VK_CHECK(vkCreateSampler(m_device, &sampler, nullptr, &m_sampler));
        sampler.magFilter = VK_FILTER_LINEAR; sampler.minFilter = VK_FILTER_LINEAR;
        LUCID_VK_CHECK(vkCreateSampler(m_device, &sampler, nullptr, &m_linearSampler));
        std::array<VkAttachmentDescription, 2> attachments{};
        attachments[0].format = colorFormat; attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachments[1].format = depthFormat; attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{}; subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &colorRef; subpass.pDepthStencilAttachment = &depthRef;
        VkSubpassDependency dependency{}; dependency.srcSubpass = 0; dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        VkRenderPassCreateInfo render{}; render.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render.attachmentCount = static_cast<std::uint32_t>(attachments.size()); render.pAttachments = attachments.data();
        render.subpassCount = 1; render.pSubpasses = &subpass; render.dependencyCount = 1; render.pDependencies = &dependency;
        LUCID_VK_CHECK(vkCreateRenderPass(m_device, &render, nullptr, &m_renderPass));
        const std::array<VkImageView, 2> views{m_colorView, m_depthView};
        VkFramebufferCreateInfo framebuffer{}; framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer.renderPass = m_renderPass; framebuffer.attachmentCount = static_cast<std::uint32_t>(views.size());
        framebuffer.pAttachments = views.data(); framebuffer.width = extent.width; framebuffer.height = extent.height; framebuffer.layers = 1;
        LUCID_VK_CHECK(vkCreateFramebuffer(m_device, &framebuffer, nullptr, &m_framebuffer));
    } catch (...) { destroy(); throw; }
}
Background3DTarget::~Background3DTarget() { destroy(); }
void Background3DTarget::destroy() noexcept {
    if (m_framebuffer) vkDestroyFramebuffer(m_device, m_framebuffer, nullptr);
    if (m_sampler) vkDestroySampler(m_device, m_sampler, nullptr);
    if (m_linearSampler) vkDestroySampler(m_device, m_linearSampler, nullptr);
    if (m_renderPass) vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    if (m_depthView) vkDestroyImageView(m_device, m_depthView, nullptr);
    if (m_colorView) vkDestroyImageView(m_device, m_colorView, nullptr);
    if (m_depth) vkDestroyImage(m_device, m_depth, nullptr);
    if (m_color) vkDestroyImage(m_device, m_color, nullptr);
    if (m_depthMemory) vkFreeMemory(m_device, m_depthMemory, nullptr);
    if (m_colorMemory) vkFreeMemory(m_device, m_colorMemory, nullptr);
    m_framebuffer = VK_NULL_HANDLE; m_sampler = VK_NULL_HANDLE; m_linearSampler = VK_NULL_HANDLE; m_renderPass = VK_NULL_HANDLE; m_depthView = VK_NULL_HANDLE; m_colorView = VK_NULL_HANDLE;
    m_depth = VK_NULL_HANDLE; m_color = VK_NULL_HANDLE; m_depthMemory = VK_NULL_HANDLE; m_colorMemory = VK_NULL_HANDLE;
}
}
