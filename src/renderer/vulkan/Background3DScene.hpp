#pragma once
#include "renderer/vulkan/Buffer.hpp"
#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>
namespace lucid::vk {
class Device;
class Background3DPipeline;
// Owns GPU copies of the city mesh parts independently of the swapchain.
class Background3DScene {
public:
    Background3DScene(const Device& device, std::string_view sceneName);
    void draw(VkCommandBuffer command, const Background3DPipeline& pipeline,
              VkExtent2D extent, float time, float dim, const std::array<float,4>& background,
              const std::array<float,3>& primary, bool naturalColors) const;
private:
    struct Part {
        std::unique_ptr<Buffer> vertices;
        std::unique_ptr<Buffer> indices;
        std::uint32_t indexCount = 0;
        std::array<float,4> color{};
        std::array<float,4> naturalColor{};
        float emission = 0.0F;
        float surface = 0.0F;
        enum class Motion { None, Train, Satellite, Clouds, Ocean, Beacon, Rain, Station, Planet, PlanetClouds, Tent, Fireflies, Tracer, RetroGrid, RetroSun, LunarEarth, Vegetation, Stars, Aircraft, Flame, Birds, Ripples, Meteor, Compass, Sparks } motion = Motion::None;
    };
    std::vector<Part> m_parts;
    bool m_lunar = false;
    bool m_mountains = false;
    bool m_lighthouse = false;
    bool m_rainyStreet = false;
    bool m_orbit = false;
    bool m_forest = false;
    bool m_drafting = false;
    bool m_retro = false;
};
}
