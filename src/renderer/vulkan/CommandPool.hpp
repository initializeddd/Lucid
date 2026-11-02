#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace lucid::vk {

class Device;

class CommandPool {
public:
    explicit CommandPool(const Device& device);
    ~CommandPool();

    CommandPool(const CommandPool&) = delete;
    CommandPool& operator=(const CommandPool&) = delete;
    CommandPool(CommandPool&&) = delete;
    CommandPool& operator=(CommandPool&&) = delete;

    [[nodiscard]] std::vector<VkCommandBuffer> allocate(std::uint32_t count) const;

    void begin(VkCommandBuffer commandBuffer) const;
    void end(VkCommandBuffer commandBuffer) const;

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkCommandPool m_pool = VK_NULL_HANDLE;
};

} // namespace lucid::vk
