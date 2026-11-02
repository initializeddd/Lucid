#include "renderer/vulkan/BlurPass.hpp"

#include "renderer/vulkan/Device.hpp"
#include "renderer/vulkan/VulkanCommon.hpp"

#include "shaders/blur.frag.h"
#include "shaders/fullscreen.vert.h"

#include <stdexcept>

namespace lucid::vk {
namespace {

[[nodiscard]] VkShaderModule createShaderModule(const Device& device,
                                                const std::uint32_t* code,
                                                std::size_t size) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = size;
    createInfo.pCode = code;

    VkShaderModule module = VK_NULL_HANDLE;
    LUCID_VK_CHECK(vkCreateShaderModule(device.handle(), &createInfo, nullptr, &module));
    return module;
}

void destroyShaderModule(VkDevice device, VkShaderModule module) noexcept {
    if (module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, module, nullptr);
    }
}

} // namespace

BlurPass::BlurPass(const Device& device, VkRenderPass renderPass, VkFormat format,
                   VkExtent2D extent, VkImageView sceneView, VkSampler sceneSampler,
                   BlurAxis axis)
    : m_device(device), m_renderPass(renderPass), m_extent(extent), m_axis(axis) {
    (void)format;
    try {
        createDescriptorSet(sceneView, sceneSampler);
        createPipelines();
    } catch (...) {
        destroy();
        throw;
    }
}

BlurPass::~BlurPass() {
    destroy();
}

void BlurPass::createDescriptorSet(VkImageView sceneView, VkSampler sceneSampler) {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    LUCID_VK_CHECK(vkCreateDescriptorSetLayout(m_device.handle(), &layoutInfo, nullptr,
                                               &m_descriptorSetLayout));

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;
    LUCID_VK_CHECK(vkCreateDescriptorPool(m_device.handle(), &poolInfo, nullptr,
                                          &m_descriptorPool));

    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = m_descriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &m_descriptorSetLayout;
    LUCID_VK_CHECK(vkAllocateDescriptorSets(m_device.handle(), &allocateInfo,
                                            &m_descriptorSet));

    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = sceneSampler;
    imageInfo.imageView = sceneView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(m_device.handle(), 1, &write, 0, nullptr);
}

void BlurPass::createPipelines() {
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_descriptorSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    LUCID_VK_CHECK(vkCreatePipelineLayout(m_device.handle(), &layoutInfo, nullptr,
                                          &m_pipelineLayout));

    const VkShaderModule vertexShader =
        createShaderModule(m_device, fullscreen_vert_spv,
                           sizeof(fullscreen_vert_spv));
    const VkShaderModule fragmentShader =
        createShaderModule(m_device, blur_frag_spv, sizeof(blur_frag_spv));

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertexShader;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragmentShader;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{};
    viewport.width = static_cast<float>(m_extent.width);
    viewport.height = static_cast<float>(m_extent.height);
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    VkRect2D scissor{};
    scissor.extent = m_extent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0F;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState attachment{};
    attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT |
                                VK_COLOR_COMPONENT_A_BIT;
    attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &attachment;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;

    LUCID_VK_CHECK(vkCreateGraphicsPipelines(m_device.handle(), VK_NULL_HANDLE, 1,
                                             &pipelineInfo, nullptr, &m_pipeline));

    destroyShaderModule(m_device.handle(), vertexShader);
    destroyShaderModule(m_device.handle(), fragmentShader);
}

void BlurPass::draw(VkCommandBuffer commandBuffer, float radius, float dim) const {
    PushConstants push{};
    push.viewport[0] = static_cast<float>(m_extent.width);
    push.viewport[1] = static_cast<float>(m_extent.height);
    push.radius = radius;
    push.dim = dim;
    push.horizontal = m_axis == BlurAxis::Horizontal ? 1.0F : 0.0F;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    vkCmdPushConstants(commandBuffer, m_pipelineLayout,
                       static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT |
                                                       VK_SHADER_STAGE_FRAGMENT_BIT),
                       0, sizeof(push), &push);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

void BlurPass::destroy() noexcept {
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device.handle(), m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device.handle(), m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device.handle(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device.handle(), m_descriptorSetLayout,
                                     nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
}

} // namespace lucid::vk
