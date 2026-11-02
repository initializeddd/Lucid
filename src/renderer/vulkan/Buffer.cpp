#include "renderer/vulkan/Buffer.hpp"

#include "renderer/vulkan/Device.hpp"
#include "renderer/vulkan/VulkanCommon.hpp"

#include <cstring>
#include <stdexcept>

namespace lucid::vk {

Buffer::~Buffer() {
    destroy();
}

void Buffer::create(const Device& device, VkDeviceSize size, VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags properties, bool persistentlyMapped) {
    destroy();

    m_device = device.handle();
    m_size = size;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    LUCID_VK_CHECK(vkCreateBuffer(m_device, &bufferInfo, nullptr, &m_buffer));

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(m_device, m_buffer, &requirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex =
        device.findMemoryType(requirements.memoryTypeBits, properties);
    LUCID_VK_CHECK(vkAllocateMemory(m_device, &allocateInfo, nullptr, &m_memory));
    LUCID_VK_CHECK(vkBindBufferMemory(m_device, m_buffer, m_memory, 0));

    if (persistentlyMapped) {
        LUCID_VK_CHECK(vkMapMemory(m_device, m_memory, 0, size, 0, &m_mapped));
    }
}

void Buffer::destroy() noexcept {
    if (m_mapped != nullptr) {
        vkUnmapMemory(m_device, m_memory);
        m_mapped = nullptr;
    }

    if (m_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
    }

    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }

    m_device = VK_NULL_HANDLE;
    m_size = 0;
}

void Buffer::upload(const void* data, VkDeviceSize size) const {
    if (size > m_size) {
        throw std::runtime_error("Buffer upload exceeds the buffer size");
    }
    if (size == 0) {
        return;
    }

    if (m_mapped != nullptr) {
        std::memcpy(m_mapped, data, static_cast<std::size_t>(size));
        return;
    }

    void* mapped = nullptr;
    LUCID_VK_CHECK(vkMapMemory(m_device, m_memory, 0, size, 0, &mapped));
    std::memcpy(mapped, data, static_cast<std::size_t>(size));
    vkUnmapMemory(m_device, m_memory);
}

} // namespace lucid::vk
