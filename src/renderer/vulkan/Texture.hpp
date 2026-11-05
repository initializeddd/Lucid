#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <limits>

namespace lucid::vk {

class Device;

// Check the byte count before multiplying image dimensions; a 32-bit width
// and height can overflow VkDeviceSize when expanded to four channels.
[[nodiscard]] constexpr bool validTextureByteSize(std::uint32_t width, std::uint32_t height,
                                                   VkDeviceSize size, bool rgba) noexcept {
    if (!width || !height) return false;
    const VkDeviceSize pixels = static_cast<VkDeviceSize>(width) * height;
    const VkDeviceSize channels = rgba ? 4 : 1;
    return pixels <= std::numeric_limits<VkDeviceSize>::max() / channels && size >= pixels * channels;
}

class Texture {
public:
    Texture(const Device& device, std::uint32_t width, std::uint32_t height,
            const std::uint8_t* pixels, VkDeviceSize size, bool rgba = false);
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
