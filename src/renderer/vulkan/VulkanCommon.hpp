#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace lucid::vk {

[[nodiscard]] std::string_view resultToString(VkResult result) noexcept;
[[nodiscard]] bool isSrgbFormat(VkFormat format) noexcept;
[[nodiscard]] std::array<float, 4> srgbToLinear(std::array<float, 4> color) noexcept;

} // namespace lucid::vk

#define LUCID_VK_CHECK(expression)                                                                 \
    do {                                                                                           \
        const VkResult lucidVkResult = (expression);                                               \
        if (lucidVkResult != VK_SUCCESS) {                                                         \
            throw std::runtime_error(std::string("Vulkan call failed: ") + #expression +           \
                                     " -> " + std::string(lucid::vk::resultToString(lucidVkResult))); \
        }                                                                                          \
    } while (false)
