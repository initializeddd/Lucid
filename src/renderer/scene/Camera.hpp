#pragma once
#include <array>
namespace lucid::renderer::scene {
using Vector3 = std::array<float, 3>;
struct Camera {
    Vector3 position{0, 0, 5};
    Vector3 target{0, 0, 0};
    Vector3 up{0, 1, 0};
    float verticalFovRadians = 0.785398163F;
    float nearPlane = 0.1F;
    float farPlane = 100.0F;
};
// Column-major Vulkan clip transform; depth is in [0,1]. Returns false for
// invalid cameras instead of passing non-finite matrices to the GPU.
[[nodiscard]] bool viewProjection(const Camera& camera, float aspect, std::array<float, 16>& result);
}
