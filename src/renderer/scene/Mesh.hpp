#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::renderer::scene {

struct MeshVertex {
    std::array<float, 3> position{};
    std::array<float, 3> normal{};
};

struct Mesh {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
};

// Loads triangle and polygon geometry from Wavefront OBJ. All output is replaced
// only after the entire input is validated. Faces without normals use flat normals.
[[nodiscard]] bool parseObj(std::string_view source, Mesh& output, std::string& error);

} // namespace lucid::renderer::scene
