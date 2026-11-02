#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>

namespace lucid::vk {

class Device;

class Buffer {
public:
    Buffer() = default;
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&&) = delete;
    Buffer& operator=(Buffer&&) = delete;

    void create(const Device& device, VkDeviceSize size, VkBufferUsageFlags usage,
                VkMemoryPropertyFlags properties, bool persistentlyMapped);
    void destroy() noexcept;

    void upload(const void* data, VkDeviceSize size) const;

    [[nodiscard]] VkBuffer handle() const noexcept { return m_buffer; }
    [[nodiscard]] VkDeviceSize size() const noexcept { return m_size; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkDeviceSize m_size = 0;
    void* m_mapped = nullptr;
};

} // namespace lucid::vk
