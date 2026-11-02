#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace lucid::vk {

class Device;

class Texture {
public:
    Texture(const Device& device, std::uint32_t width, std::uint32_t height,
            const std::uint8_t* pixels, VkDeviceSize size);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) = delete;
    Texture& operator=(Texture&&) = delete;

    [[nodiscard]] VkImageView view() const noexcept { return m_view; }
    [[nodiscard]] VkSampler sampler() const noexcept { return m_sampler; }

private:
    void destroy() noexcept;

    VkDevice m_device = VK_NULL_HANDLE;
    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkImageView m_view = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
};

} // namespace lucid::vk
