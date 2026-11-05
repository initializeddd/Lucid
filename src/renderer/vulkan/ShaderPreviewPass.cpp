#include "renderer/vulkan/ShaderPreviewPass.hpp"
#include "modules/SpirvInspection.hpp"
#include "modules/ShaderPreviewInterface.hpp"
#include "shaders/fullscreen.vert.h"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
namespace lucid::vk {
namespace {
void check(VkResult result, const char* what) { if (result != VK_SUCCESS) throw std::runtime_error(what); }
VkShaderModule makeShader(VkDevice device, const std::uint32_t* code, std::size_t size) {
    VkShaderModuleCreateInfo info{}; info.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; info.codeSize=size; info.pCode=code;
    VkShaderModule shader=VK_NULL_HANDLE; check(vkCreateShaderModule(device,&info,nullptr,&shader),"Cannot create preview shader"); return shader;
}
}
ShaderPreviewPass::ShaderPreviewPass(VkDevice device, VkRenderPass renderPass, VkExtent2D extent, std::string_view bytes) : device_(device) {
    if (!device || !renderPass || !extent.width || !extent.height || extent.width>2048 || extent.height>2048 || bytes.size()>1024U*1024U) throw std::invalid_argument("Invalid preview target or shader size");
    modules::SpirvInspection metadata; std::string error;
    if (!modules::inspectSpirv(bytes,metadata,error)) throw std::invalid_argument("Invalid preview SPIR-V: "+error);
    if (!modules::validatePreviewFragmentInterface(bytes,error)) throw std::invalid_argument(error);
    bool fragment=false; for (const auto& entry:metadata.entryPoints) if (entry.name=="main" && entry.executionModel==4) fragment=true;
    if (!fragment) throw std::invalid_argument("Preview requires a fragment entry point named main");
    // Vulkan expects aligned native words; byte-swapped SPIR-V is normalized.
    const bool swapped=static_cast<unsigned char>(bytes[0])==0x07;
    std::vector<std::uint32_t> code(bytes.size()/4);
    for (std::size_t i=0;i<code.size();++i) { const auto* p=reinterpret_cast<const unsigned char*>(bytes.data()+4*i); code[i]=swapped ? (std::uint32_t(p[0])<<24)|(std::uint32_t(p[1])<<16)|(std::uint32_t(p[2])<<8)|p[3] : std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24); }
    VkShaderModule vertex=VK_NULL_HANDLE, frag=VK_NULL_HANDLE;
    try {
        VkPipelineLayoutCreateInfo layout{}; layout.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        check(vkCreatePipelineLayout(device_,&layout,nullptr,&layout_),"Cannot create preview layout");
        vertex=makeShader(device_,fullscreen_vert_spv,sizeof(fullscreen_vert_spv)); frag=makeShader(device_,code.data(),bytes.size());
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType=stages[1].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT; stages[0].module=vertex; stages[0].pName="main";
        stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module=frag; stages[1].pName="main";
        VkPipelineVertexInputStateCreateInfo vertexInput{}; vertexInput.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineInputAssemblyStateCreateInfo assembly{}; assembly.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO; assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport viewport{}; viewport.width=static_cast<float>(extent.width); viewport.height=static_cast<float>(extent.height); viewport.maxDepth=1;
        VkRect2D scissor{}; scissor.extent=extent;
        VkPipelineViewportStateCreateInfo viewportState{}; viewportState.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO; viewportState.viewportCount=1; viewportState.pViewports=&viewport; viewportState.scissorCount=1; viewportState.pScissors=&scissor;
        VkPipelineRasterizationStateCreateInfo raster{}; raster.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO; raster.polygonMode=VK_POLYGON_MODE_FILL; raster.cullMode=VK_CULL_MODE_NONE; raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE; raster.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo samples{}; samples.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO; samples.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState attachment{}; attachment.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{}; blend.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO; blend.attachmentCount=1; blend.pAttachments=&attachment;
        VkGraphicsPipelineCreateInfo pipeline{}; pipeline.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO; pipeline.stageCount=2; pipeline.pStages=stages; pipeline.pVertexInputState=&vertexInput; pipeline.pInputAssemblyState=&assembly; pipeline.pViewportState=&viewportState; pipeline.pRasterizationState=&raster; pipeline.pMultisampleState=&samples; pipeline.pColorBlendState=&blend; pipeline.layout=layout_; pipeline.renderPass=renderPass;
        check(vkCreateGraphicsPipelines(device_,VK_NULL_HANDLE,1,&pipeline,nullptr,&pipeline_),"Cannot create fragment preview pipeline");
        vkDestroyShaderModule(device_,frag,nullptr); vkDestroyShaderModule(device_,vertex,nullptr);
    } catch (...) {
        if (frag) vkDestroyShaderModule(device_,frag,nullptr);
        if (vertex) vkDestroyShaderModule(device_,vertex,nullptr);
        if (pipeline_) vkDestroyPipeline(device_,pipeline_,nullptr);
        if (layout_) vkDestroyPipelineLayout(device_,layout_,nullptr);
        throw;
    }
}
ShaderPreviewPass::~ShaderPreviewPass() { if (pipeline_) vkDestroyPipeline(device_,pipeline_,nullptr); if (layout_) vkDestroyPipelineLayout(device_,layout_,nullptr); }
void ShaderPreviewPass::draw(VkCommandBuffer commandBuffer) const noexcept { vkCmdBindPipeline(commandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_); vkCmdDraw(commandBuffer,3,1,0,0); }
}
