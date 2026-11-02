#include "renderer/vulkan/SceneTarget.hpp"

#include "renderer/vulkan/Device.hpp"
#include "renderer/vulkan/VulkanCommon.hpp"

#include <stdexcept>

namespace lucid::vk {
namespace {

[[nodiscard]] VkRenderPass createSceneRenderPass(VkDevice device, VkFormat format) {
    VkAttachmentDescription color{};
    color.format = format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorReference{};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = 0;
    dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &color;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dependency;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    LUCID_VK_CHECK(
        vkCreateRenderPass(device, &createInfo, nullptr, &renderPass));
    return renderPass;
}

} // namespace

SceneTarget::SceneTarget(const Device& device, VkFormat format, VkExtent2D extent)
    : m_device(device.handle()), m_format(format), m_extent(extent) {
    if (extent.width == 0 || extent.height == 0) {
        throw std::runtime_error("Invalid scene target extent");
    }

    try {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {extent.width, extent.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        LUCID_VK_CHECK(vkCreateImage(m_device, &imageInfo, nullptr, &m_image));

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(m_device, m_image, &requirements);

        VkMemoryAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = requirements.size;
        allocateInfo.memoryTypeIndex = device.findMemoryType(
            requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        LUCID_VK_CHECK(
            vkAllocateMemory(m_device, &allocateInfo, nullptr, &m_memory));
        LUCID_VK_CHECK(vkBindImageMemory(m_device, m_image, m_memory, 0));

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        LUCID_VK_CHECK(vkCreateImageView(m_device, &viewInfo, nullptr, &m_view));

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod = 0.0F;
        LUCID_VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sampler));

        m_renderPass = createSceneRenderPass(m_device, format);

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &m_view;
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;
        LUCID_VK_CHECK(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr,
                                           &m_framebuffer));
    } catch (...) {
        destroy();
        throw;
    }
}

SceneTarget::~SceneTarget() {
    destroy();
}

void SceneTarget::destroy() noexcept {
    if (m_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(m_device, m_framebuffer, nullptr);
        m_framebuffer = VK_NULL_HANDLE;
    }
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    if (m_view != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_view, nullptr);
        m_view = VK_NULL_HANDLE;
    }
    if (m_image != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_image, nullptr);
        m_image = VK_NULL_HANDLE;
    }
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
    m_device = VK_NULL_HANDLE;
}

} // namespace lucid::vk
