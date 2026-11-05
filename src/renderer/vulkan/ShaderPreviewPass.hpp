#pragma once
#include <vulkan/vulkan.h>
#include <string_view>
namespace lucid::vk {
// Records a real full-screen draw into an existing one-color, single-sample
// offscreen render pass. Trusted fragment SPIR-V must expose main and be
// compatible with a descriptor-free pipeline and the bundled fullscreen vertex
// shader (location 0 vec2 UV). This class is not a validator or sandbox.
// The caller owns the device, render pass, framebuffer, render-pass begin/end,
// synchronization and GPU lifetime; keep this object alive until all submitted
// commands finish. Run untrusted or user-supplied GPU work only in a separately
// supervised helper: this synchronous API provides no GPU timeout guarantee.
class ShaderPreviewPass {
public:
    ShaderPreviewPass(VkDevice device, VkRenderPass renderPass, VkExtent2D extent, std::string_view fragmentSpirv);
    ~ShaderPreviewPass();
    ShaderPreviewPass(const ShaderPreviewPass&) = delete;
    ShaderPreviewPass& operator=(const ShaderPreviewPass&) = delete;
    void draw(VkCommandBuffer commandBuffer) const noexcept;
private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
};
}
