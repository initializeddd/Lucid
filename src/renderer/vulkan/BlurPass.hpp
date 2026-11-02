#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace lucid::vk {

class Device;

enum class BlurAxis {
    Horizontal,
    Vertical,
};

class BlurPass {
public:
    BlurPass(const Device& device, VkRenderPass renderPass, VkFormat format,
             VkExtent2D extent, VkImageView sceneView, VkSampler sceneSampler,
             BlurAxis axis);
    ~BlurPass();

    BlurPass(const BlurPass&) = delete;
    BlurPass& operator=(const BlurPass&) = delete;
    BlurPass(BlurPass&&) = delete;
    BlurPass& operator=(BlurPass&&) = delete;

    void draw(VkCommandBuffer commandBuffer, float radius, float dim) const;

private:
    struct PushConstants {
        float viewport[2];
        float radius;
        float dim;
        float horizontal;
    };

    void createDescriptorSet(VkImageView sceneView, VkSampler sceneSampler);
    void createPipelines();
    void destroy() noexcept;

    const Device& m_device;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkExtent2D m_extent{};
    BlurAxis m_axis = BlurAxis::Horizontal;

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

} // namespace lucid::vk
