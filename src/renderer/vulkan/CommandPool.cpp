#include "renderer/vulkan/CommandPool.hpp"

#include "renderer/vulkan/Device.hpp"
#include "renderer/vulkan/VulkanCommon.hpp"

namespace lucid::vk {

CommandPool::CommandPool(const Device& device) : m_device(device.handle()) {
    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = device.graphicsFamily();

    LUCID_VK_CHECK(vkCreateCommandPool(m_device, &createInfo, nullptr, &m_pool));
}

CommandPool::~CommandPool() {
    if (m_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_pool, nullptr);
    }
}

std::vector<VkCommandBuffer> CommandPool::allocate(std::uint32_t count) const {
    std::vector<VkCommandBuffer> commandBuffers(count, VK_NULL_HANDLE);

    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = m_pool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = count;

    LUCID_VK_CHECK(vkAllocateCommandBuffers(m_device, &allocateInfo, commandBuffers.data()));
    return commandBuffers;
}

void CommandPool::begin(VkCommandBuffer commandBuffer) const {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    LUCID_VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
}

void CommandPool::end(VkCommandBuffer commandBuffer) const {
    LUCID_VK_CHECK(vkEndCommandBuffer(commandBuffer));
}

} // namespace lucid::vk
