#include "renderer/vulkan/Texture.hpp"
#include <cstdint>
#include <limits>
int main() {
    using lucid::vk::validTextureByteSize;
    constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
    constexpr auto bytes = std::numeric_limits<VkDeviceSize>::max();
    static_assert(validTextureByteSize(1024, 1024, 1024U * 1024U * 4U, true));
    static_assert(!validTextureByteSize(1024, 1024, 1024U * 1024U * 4U - 1, true));
    static_assert(!validTextureByteSize(0, 1, 1, false));
    static_assert(validTextureByteSize(maximum, maximum, bytes, false));
    static_assert(!validTextureByteSize(maximum, maximum, bytes, true));
    return 0;
}
