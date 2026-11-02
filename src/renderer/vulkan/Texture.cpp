#include "renderer/vulkan/Texture.hpp"

#include "renderer/vulkan/Buffer.hpp"
#include "renderer/vulkan/Device.hpp"
#include "renderer/vulkan/VulkanCommon.hpp"

#include <stdexcept>

namespace lucid::vk {
namespace {

void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                           VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error("Unsupported image layout transition");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);
}

template <typename Function>
void withOneTimeCommands(const Device& device, Function&& function) {
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = device.graphicsFamily();
    LUCID_VK_CHECK(vkCreateCommandPool(device.handle(), &poolInfo, nullptr, &commandPool));

    try {
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;
        LUCID_VK_CHECK(
            vkAllocateCommandBuffers(device.handle(), &allocateInfo, &commandBuffer));

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        LUCID_VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        function(commandBuffer);

        LUCID_VK_CHECK(vkEndCommandBuffer(commandBuffer));

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        LUCID_VK_CHECK(
            vkQueueSubmit(device.graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE));
        LUCID_VK_CHECK(vkQueueWaitIdle(device.graphicsQueue()));
    } catch (...) {
        vkDestroyCommandPool(device.handle(), commandPool, nullptr);
        throw;
    }

    vkDestroyCommandPool(device.handle(), commandPool, nullptr);
}

} // namespace

Texture::Texture(const Device& device, std::uint32_t width, std::uint32_t height,
                 const std::uint8_t* pixels, VkDeviceSize size)
    : m_device(device.handle()) {
    if (width == 0 || height == 0 || pixels == nullptr ||
        size < static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height)) {
        throw std::runtime_error("Invalid texture data");
    }

    try {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {width, height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8_UNORM;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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
        LUCID_VK_CHECK(vkAllocateMemory(m_device, &allocateInfo, nullptr, &m_memory));
        LUCID_VK_CHECK(vkBindImageMemory(m_device, m_image, m_memory, 0));

        Buffer staging;
        staging.create(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       false);
        staging.upload(pixels, size);

        withOneTimeCommands(device, [&](VkCommandBuffer commandBuffer) {
            transitionImageLayout(commandBuffer, m_image, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkBufferImageCopy copyRegion{};
            copyRegion.bufferOffset = 0;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;
            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageOffset = {0, 0, 0};
            copyRegion.imageExtent = {width, height, 1};
            vkCmdCopyBufferToImage(commandBuffer, staging.handle(), m_image,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            transitionImageLayout(commandBuffer, m_image,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        });

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8_UNORM;
        viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
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
        samplerInfo.mipLodBias = 0.0F;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.minLod = 0.0F;
        samplerInfo.maxLod = 0.0F;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        LUCID_VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sampler));
    } catch (...) {
        destroy();
        throw;
    }
}

Texture::~Texture() {
    destroy();
}

void Texture::destroy() noexcept {
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
