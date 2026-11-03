#include "renderer/vulkan/Background3DPipeline.hpp"
#include "renderer/vulkan/Device.hpp"
#include "renderer/vulkan/VulkanCommon.hpp"
#include "renderer/scene/Mesh.hpp"
#include "shaders/background3d.vert.h"
#include "shaders/background3d.frag.h"
#include <array>
#include <cstddef>
namespace lucid::vk {
namespace {
VkShaderModule module(VkDevice device, const std::uint32_t* code, std::size_t size) {
    VkShaderModuleCreateInfo info{}; info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; info.codeSize = size; info.pCode = code;
    VkShaderModule result = VK_NULL_HANDLE; LUCID_VK_CHECK(vkCreateShaderModule(device, &info, nullptr, &result)); return result;
}
struct Push { std::array<float,16> mvp; std::array<float,4> color; std::array<float,4> animation; std::array<float,4> background; std::array<float,4> material; };
static_assert(sizeof(Push) <= 128, "Vulkan guarantees at least 128 bytes of push constants");
}
Background3DPipeline::Background3DPipeline(const Device& device, VkRenderPass renderPass) : m_device(device.handle()) {
    VkShaderModule vertex = VK_NULL_HANDLE, fragment = VK_NULL_HANDLE;
    try {
        vertex = module(m_device, background3d_vert_spv, sizeof(background3d_vert_spv));
        fragment = module(m_device, background3d_frag_spv, sizeof(background3d_frag_spv));
        VkPushConstantRange range{}; range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; range.size = sizeof(Push);
        VkPipelineLayoutCreateInfo layout{}; layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; layout.pushConstantRangeCount = 1; layout.pPushConstantRanges = &range;
        LUCID_VK_CHECK(vkCreatePipelineLayout(m_device, &layout, nullptr, &m_layout));
        std::array<VkPipelineShaderStageCreateInfo,2> stages{};
        stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vertex; stages[0].pName = "main";
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fragment; stages[1].pName = "main";
        VkVertexInputBindingDescription binding{0, sizeof(renderer::scene::MeshVertex), VK_VERTEX_INPUT_RATE_VERTEX};
        std::array<VkVertexInputAttributeDescription,2> attributes{{
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<std::uint32_t>(offsetof(renderer::scene::MeshVertex, position))},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<std::uint32_t>(offsetof(renderer::scene::MeshVertex, normal))}}};
        VkPipelineVertexInputStateCreateInfo input{}; input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        input.vertexBindingDescriptionCount = 1; input.pVertexBindingDescriptions = &binding;
        input.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size()); input.pVertexAttributeDescriptions = attributes.data();
        VkPipelineInputAssemblyStateCreateInfo assembly{}; assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO; assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{}; viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO; viewport.viewportCount = 1; viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo raster{}; raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO; raster.polygonMode = VK_POLYGON_MODE_FILL; raster.cullMode = VK_CULL_MODE_NONE; raster.lineWidth = 1;
        VkPipelineMultisampleStateCreateInfo samples{}; samples.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO; samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depth{}; depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO; depth.depthTestEnable = VK_TRUE; depth.depthWriteEnable = VK_TRUE; depth.depthCompareOp = VK_COMPARE_OP_LESS;
        VkPipelineColorBlendAttachmentState blend{}; blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blending{}; blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO; blending.attachmentCount = 1; blending.pAttachments = &blend;
        const std::array<VkDynamicState,2> dynamic{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicInfo{}; dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO; dynamicInfo.dynamicStateCount = static_cast<std::uint32_t>(dynamic.size()); dynamicInfo.pDynamicStates = dynamic.data();
        VkGraphicsPipelineCreateInfo pipeline{}; pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline.stageCount = static_cast<std::uint32_t>(stages.size()); pipeline.pStages = stages.data(); pipeline.pVertexInputState = &input; pipeline.pInputAssemblyState = &assembly;
        pipeline.pViewportState = &viewport; pipeline.pRasterizationState = &raster; pipeline.pMultisampleState = &samples;
        pipeline.pDepthStencilState = &depth; pipeline.pColorBlendState = &blending; pipeline.pDynamicState = &dynamicInfo;
        pipeline.layout = m_layout; pipeline.renderPass = renderPass; pipeline.subpass = 0;
        LUCID_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline, nullptr, &m_pipeline));
        depth.depthWriteEnable = VK_FALSE;
        blend.blendEnable = VK_TRUE;
        blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend.colorBlendOp = VK_BLEND_OP_ADD; blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; blend.alphaBlendOp = VK_BLEND_OP_ADD;
        LUCID_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline, nullptr, &m_transparentPipeline));
    } catch (...) { if (vertex) vkDestroyShaderModule(m_device, vertex, nullptr); if (fragment) vkDestroyShaderModule(m_device, fragment, nullptr); destroy(); throw; }
    vkDestroyShaderModule(m_device, vertex, nullptr); vkDestroyShaderModule(m_device, fragment, nullptr);
}
Background3DPipeline::~Background3DPipeline() { destroy(); }
void Background3DPipeline::destroy() noexcept { if (m_transparentPipeline) vkDestroyPipeline(m_device, m_transparentPipeline, nullptr); if (m_pipeline) vkDestroyPipeline(m_device, m_pipeline, nullptr); if (m_layout) vkDestroyPipelineLayout(m_device, m_layout, nullptr); m_transparentPipeline = VK_NULL_HANDLE; m_pipeline = VK_NULL_HANDLE; m_layout = VK_NULL_HANDLE; }
void Background3DPipeline::draw(VkCommandBuffer command, VkBuffer vertices, VkBuffer indices, std::uint32_t count, const std::array<float,16>& mvp, const std::array<float,4>& color, const std::array<float,4>& animation, const std::array<float,4>& background, const std::array<float,4>& material) const {
    if (!count || !vertices || !indices) return;
    const Push push{mvp,color,animation,background,material}; const VkDeviceSize offset = 0;
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, color[3] < 1.0F ? m_transparentPipeline : m_pipeline);
    vkCmdBindVertexBuffers(command, 0, 1, &vertices, &offset); vkCmdBindIndexBuffer(command, indices, 0, VK_INDEX_TYPE_UINT32);
    vkCmdPushConstants(command, m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
    vkCmdDrawIndexed(command, count, 1, 0, 0, 0);
}
}
