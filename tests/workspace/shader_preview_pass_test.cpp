#include "renderer/vulkan/ShaderPreviewPass.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
static int modulesCreated=0,modulesDestroyed=0,layoutsDestroyed=0,pipelinesDestroyed=0,draws=0; static bool failPipeline=false;
extern "C" {
VKAPI_ATTR VkResult VKAPI_CALL vkCreatePipelineLayout(VkDevice,const VkPipelineLayoutCreateInfo*,const VkAllocationCallbacks*,VkPipelineLayout* out) { *out=(VkPipelineLayout)1; return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL vkDestroyPipelineLayout(VkDevice,VkPipelineLayout,const VkAllocationCallbacks*) { ++layoutsDestroyed; }
VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(VkDevice,const VkShaderModuleCreateInfo* info,const VkAllocationCallbacks*,VkShaderModule* out) { if (!info || !info->pCode || !info->codeSize) return VK_ERROR_UNKNOWN; *out=(VkShaderModule)(uintptr_t)(++modulesCreated); return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL vkDestroyShaderModule(VkDevice,VkShaderModule,const VkAllocationCallbacks*) { ++modulesDestroyed; }
VKAPI_ATTR VkResult VKAPI_CALL vkCreateGraphicsPipelines(VkDevice,VkPipelineCache,uint32_t count,const VkGraphicsPipelineCreateInfo* info,const VkAllocationCallbacks*,VkPipeline* out) { if (failPipeline || count!=1 || !info || info->stageCount!=2 || !info->pColorBlendState || info->pColorBlendState->attachmentCount!=1) return VK_ERROR_UNKNOWN; *out=(VkPipeline)1; return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL vkDestroyPipeline(VkDevice,VkPipeline,const VkAllocationCallbacks*) { ++pipelinesDestroyed; }
VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer,VkPipelineBindPoint,VkPipeline) {}
VKAPI_ATTR void VKAPI_CALL vkCmdDraw(VkCommandBuffer,uint32_t vertexCount,uint32_t instances,uint32_t,uint32_t) { if (vertexCount==3 && instances==1) ++draws; }
}
static void put(std::string& out,std::uint32_t v) { for (unsigned i=0;i<4;++i) out.push_back(static_cast<char>(v>>(8*i))); }
int main() {
    std::string fragment; for (auto w : {0x07230203U,0x00010600U,0U,8U,0U,0x00020011U,1U,0x0003000eU,0U,1U,0x0006000fU,4U,1U,0x6e69616dU,0U,2U,0x00030010U,1U,7U,0x00040047U,2U,30U,0U,0x00030016U,3U,32U,0x00040017U,4U,3U,4U,0x00040020U,5U,3U,4U,0x0004003bU,5U,2U,3U}) put(fragment,w);
    auto device=(VkDevice)1; auto renderPass=(VkRenderPass)1;
    { lucid::vk::ShaderPreviewPass pass(device,renderPass,{32,32},fragment); pass.draw((VkCommandBuffer)1); }
    if (modulesCreated!=2 || modulesDestroyed!=2 || layoutsDestroyed!=1 || pipelinesDestroyed!=1 || draws!=1) return 1;
    failPipeline=true; try { lucid::vk::ShaderPreviewPass pass(device,renderPass,{32,32},fragment); return 2; } catch (const std::runtime_error&) {}
    if (modulesDestroyed!=4 || layoutsDestroyed!=2 || pipelinesDestroyed!=1) return 3;
    try { lucid::vk::ShaderPreviewPass pass(device,renderPass,{2049,32},fragment); return 4; } catch (const std::invalid_argument&) {}
    fragment[44]=0; try { lucid::vk::ShaderPreviewPass pass(device,renderPass,{32,32},fragment); return 5; } catch (const std::invalid_argument&) {}
    return 0;
}
