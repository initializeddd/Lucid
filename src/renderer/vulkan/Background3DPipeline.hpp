#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
namespace lucid::vk {
class Device;
// Meshes use scene::MeshVertex and uint32 indices. The caller owns buffers and
// records the render pass with a cleared color and depth attachment.
class Background3DPipeline {
public:
    Background3DPipeline(const Device& device, VkRenderPass renderPass);
    ~Background3DPipeline();
    Background3DPipeline(const Background3DPipeline&) = delete;
    Background3DPipeline& operator=(const Background3DPipeline&) = delete;
    void draw(VkCommandBuffer command, VkBuffer vertices, VkBuffer indices, std::uint32_t count,
              const std::array<float, 16>& mvp, const std::array<float, 4>& color,
              const std::array<float, 4>& animation, const std::array<float, 4>& background,
              const std::array<float, 4>& material) const;
private:
    void destroy() noexcept;
    VkDevice m_device = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipeline m_transparentPipeline = VK_NULL_HANDLE;
};
}
