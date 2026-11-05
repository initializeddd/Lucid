#include "renderer/vulkan/ShaderPreviewPass.hpp"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <shellapi.h>
#endif
namespace {
void check(VkResult result, const char* message) { if (result!=VK_SUCCESS) throw std::runtime_error(message); }
struct Vulkan {
    VkInstance instance=VK_NULL_HANDLE; VkDevice device=VK_NULL_HANDLE; VkPhysicalDevice physical=VK_NULL_HANDLE; VkQueue queue=VK_NULL_HANDLE;
    VkImage image=VK_NULL_HANDLE; VkDeviceMemory imageMemory=VK_NULL_HANDLE, bufferMemory=VK_NULL_HANDLE; VkImageView view=VK_NULL_HANDLE;
    VkRenderPass renderPass=VK_NULL_HANDLE; VkFramebuffer framebuffer=VK_NULL_HANDLE; VkBuffer buffer=VK_NULL_HANDLE;
    VkCommandPool pool=VK_NULL_HANDLE; std::uint32_t family=0;
    ~Vulkan() {
        if (device) { vkDeviceWaitIdle(device); if (pool) vkDestroyCommandPool(device,pool,nullptr); if (framebuffer) vkDestroyFramebuffer(device,framebuffer,nullptr); if (renderPass) vkDestroyRenderPass(device,renderPass,nullptr); if (view) vkDestroyImageView(device,view,nullptr); if (image) vkDestroyImage(device,image,nullptr); if (imageMemory) vkFreeMemory(device,imageMemory,nullptr); if (buffer) vkDestroyBuffer(device,buffer,nullptr); if (bufferMemory) vkFreeMemory(device,bufferMemory,nullptr); vkDestroyDevice(device,nullptr); }
        if (instance) vkDestroyInstance(instance,nullptr);
    }
    std::uint32_t memoryType(std::uint32_t mask,VkMemoryPropertyFlags flags) {
        VkPhysicalDeviceMemoryProperties memory{}; vkGetPhysicalDeviceMemoryProperties(physical,&memory);
        for (std::uint32_t i=0;i<memory.memoryTypeCount;++i) if ((mask&(1U<<i)) && (memory.memoryTypes[i].propertyFlags&flags)==flags) return i;
        throw std::runtime_error("Required Vulkan memory type unavailable");
    }
    void init() {
        std::uint32_t count=0; check(vkEnumerateInstanceExtensionProperties(nullptr,&count,nullptr),"Cannot enumerate Vulkan extensions");
        std::vector<VkExtensionProperties> extensions(count); check(vkEnumerateInstanceExtensionProperties(nullptr,&count,extensions.data()),"Cannot enumerate Vulkan extensions");
        std::vector<const char*> enabled;
        const bool portability=std::any_of(extensions.begin(),extensions.end(),[](const auto& e) { return std::strcmp(e.extensionName,VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)==0; });
        if (portability) enabled.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        VkApplicationInfo app{}; app.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO; app.pApplicationName="Lucid shader preview helper"; app.apiVersion=VK_API_VERSION_1_0;
        VkInstanceCreateInfo info{}; info.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; info.pApplicationInfo=&app; info.enabledExtensionCount=static_cast<std::uint32_t>(enabled.size()); info.ppEnabledExtensionNames=enabled.data(); if (portability) info.flags=VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        check(vkCreateInstance(&info,nullptr,&instance),"Cannot create Vulkan instance");
        count=0; check(vkEnumeratePhysicalDevices(instance,&count,nullptr),"Cannot enumerate graphics devices"); if (!count) throw std::runtime_error("No Vulkan graphics device");
        std::vector<VkPhysicalDevice> devices(count); check(vkEnumeratePhysicalDevices(instance,&count,devices.data()),"Cannot enumerate graphics devices");
        for (auto candidate:devices) { std::uint32_t n=0; vkGetPhysicalDeviceQueueFamilyProperties(candidate,&n,nullptr); std::vector<VkQueueFamilyProperties> families(n); vkGetPhysicalDeviceQueueFamilyProperties(candidate,&n,families.data()); for (std::uint32_t i=0;i<n;++i) if (families[i].queueCount && (families[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)) { physical=candidate; family=i; break; } if (physical) break; }
        if (!physical) throw std::runtime_error("No graphics queue");
        count=0; check(vkEnumerateDeviceExtensionProperties(physical,nullptr,&count,nullptr),"Cannot enumerate device extensions"); std::vector<VkExtensionProperties> deviceExtensions(count); check(vkEnumerateDeviceExtensionProperties(physical,nullptr,&count,deviceExtensions.data()),"Cannot enumerate device extensions");
        std::vector<const char*> deviceEnabled; for (const auto& ext:deviceExtensions) if (std::strcmp(ext.extensionName,"VK_KHR_portability_subset")==0) deviceEnabled.push_back("VK_KHR_portability_subset");
        float priority=1; VkDeviceQueueCreateInfo queueInfo{}; queueInfo.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; queueInfo.queueFamilyIndex=family; queueInfo.queueCount=1; queueInfo.pQueuePriorities=&priority;
        VkDeviceCreateInfo deviceInfo{}; deviceInfo.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; deviceInfo.queueCreateInfoCount=1; deviceInfo.pQueueCreateInfos=&queueInfo; deviceInfo.enabledExtensionCount=static_cast<std::uint32_t>(deviceEnabled.size()); deviceInfo.ppEnabledExtensionNames=deviceEnabled.data();
        check(vkCreateDevice(physical,&deviceInfo,nullptr,&device),"Cannot create Vulkan device"); vkGetDeviceQueue(device,family,0,&queue);
    }
    std::vector<unsigned char> render(std::string_view shader,std::uint32_t width,std::uint32_t height) {
        const VkDeviceSize byteCount=VkDeviceSize(width)*height*4;
        VkImageCreateInfo imageInfo{}; imageInfo.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO; imageInfo.imageType=VK_IMAGE_TYPE_2D; imageInfo.format=VK_FORMAT_R8G8B8A8_UNORM; imageInfo.extent={width,height,1}; imageInfo.mipLevels=1; imageInfo.arrayLayers=1; imageInfo.samples=VK_SAMPLE_COUNT_1_BIT; imageInfo.tiling=VK_IMAGE_TILING_OPTIMAL; imageInfo.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT; imageInfo.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateImage(device,&imageInfo,nullptr,&image),"Cannot create preview image"); VkMemoryRequirements requirements{}; vkGetImageMemoryRequirements(device,image,&requirements);
        VkMemoryAllocateInfo alloc{}; alloc.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; alloc.allocationSize=requirements.size; alloc.memoryTypeIndex=memoryType(requirements.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(vkAllocateMemory(device,&alloc,nullptr,&imageMemory),"Cannot allocate image memory"); check(vkBindImageMemory(device,image,imageMemory,0),"Cannot bind image memory");
        VkImageViewCreateInfo viewInfo{}; viewInfo.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO; viewInfo.image=image; viewInfo.viewType=VK_IMAGE_VIEW_TYPE_2D; viewInfo.format=imageInfo.format; viewInfo.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}; check(vkCreateImageView(device,&viewInfo,nullptr,&view),"Cannot create image view");
        VkAttachmentDescription attachment{}; attachment.format=imageInfo.format; attachment.samples=VK_SAMPLE_COUNT_1_BIT; attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE; attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE; attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE; attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; attachment.finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        VkAttachmentReference color{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}; VkSubpassDescription subpass{}; subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS; subpass.colorAttachmentCount=1; subpass.pColorAttachments=&color;
        VkSubpassDependency dependency{}; dependency.srcSubpass=0; dependency.dstSubpass=VK_SUBPASS_EXTERNAL; dependency.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; dependency.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT; dependency.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; dependency.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
        VkRenderPassCreateInfo passInfo{}; passInfo.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO; passInfo.attachmentCount=1; passInfo.pAttachments=&attachment; passInfo.subpassCount=1; passInfo.pSubpasses=&subpass; passInfo.dependencyCount=1; passInfo.pDependencies=&dependency; check(vkCreateRenderPass(device,&passInfo,nullptr,&renderPass),"Cannot create render pass");
        VkFramebufferCreateInfo fb{}; fb.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO; fb.renderPass=renderPass; fb.attachmentCount=1; fb.pAttachments=&view; fb.width=width; fb.height=height; fb.layers=1; check(vkCreateFramebuffer(device,&fb,nullptr,&framebuffer),"Cannot create framebuffer");
        VkBufferCreateInfo bufferInfo{}; bufferInfo.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO; bufferInfo.size=byteCount; bufferInfo.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT; bufferInfo.sharingMode=VK_SHARING_MODE_EXCLUSIVE; check(vkCreateBuffer(device,&bufferInfo,nullptr,&buffer),"Cannot create readback buffer");
        vkGetBufferMemoryRequirements(device,buffer,&requirements); alloc.allocationSize=requirements.size; alloc.memoryTypeIndex=memoryType(requirements.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT); check(vkAllocateMemory(device,&alloc,nullptr,&bufferMemory),"Cannot allocate readback memory"); check(vkBindBufferMemory(device,buffer,bufferMemory,0),"Cannot bind readback memory");
        VkCommandPoolCreateInfo poolInfo{}; poolInfo.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; poolInfo.queueFamilyIndex=family; check(vkCreateCommandPool(device,&poolInfo,nullptr,&pool),"Cannot create command pool");
        VkCommandBufferAllocateInfo cmdInfo{}; cmdInfo.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; cmdInfo.commandPool=pool; cmdInfo.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cmdInfo.commandBufferCount=1; VkCommandBuffer command=VK_NULL_HANDLE; check(vkAllocateCommandBuffers(device,&cmdInfo,&command),"Cannot allocate command buffer");
        lucid::vk::ShaderPreviewPass preview(device,renderPass,{width,height},shader);
        VkCommandBufferBeginInfo begin{}; begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; check(vkBeginCommandBuffer(command,&begin),"Cannot begin preview");
        VkClearValue clear{}; VkRenderPassBeginInfo passBegin{}; passBegin.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO; passBegin.renderPass=renderPass; passBegin.framebuffer=framebuffer; passBegin.renderArea.extent={width,height}; passBegin.clearValueCount=1; passBegin.pClearValues=&clear; vkCmdBeginRenderPass(command,&passBegin,VK_SUBPASS_CONTENTS_INLINE); preview.draw(command); vkCmdEndRenderPass(command);
        VkBufferImageCopy region{}; region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; region.imageExtent={width,height,1}; vkCmdCopyImageToBuffer(command,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer,1,&region); check(vkEndCommandBuffer(command),"Cannot finish preview commands");
        VkSubmitInfo submit{}; submit.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO; submit.commandBufferCount=1; submit.pCommandBuffers=&command; check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Cannot submit preview"); check(vkQueueWaitIdle(queue),"Preview GPU work failed");
        void* mapped=nullptr; check(vkMapMemory(device,bufferMemory,0,byteCount,0,&mapped),"Cannot map preview pixels"); std::vector<unsigned char> pixels(static_cast<std::size_t>(byteCount)); std::memcpy(pixels.data(),mapped,pixels.size()); vkUnmapMemory(device,bufferMemory); return pixels;
    }
};
}
// Internal helper: caller must stage trusted SPIR-V in a private directory and
// supervise this process with a hard timeout. GPU execution is not sandboxed.
int main(int argc,char** argv) {
    try {
        if (argc!=4) throw std::runtime_error("Usage: shader-preview <fragment.spv> <width> <height>");
        char* end=nullptr; const auto width=std::strtoul(argv[2],&end,10); if (!end || *end || width==0 || width>1024) throw std::runtime_error("Invalid width");
        const auto height=std::strtoul(argv[3],&end,10); if (!end || *end || height==0 || height>1024) throw std::runtime_error("Invalid height");
        std::filesystem::path shaderPath;
#if defined(_WIN32)
        int wideCount=0; auto wide=CommandLineToArgvW(GetCommandLineW(),&wideCount);
        if (!wide || wideCount!=4) { if (wide) LocalFree(wide); throw std::runtime_error("Invalid command line"); }
        shaderPath=std::filesystem::path(wide[1]); LocalFree(wide);
#else
        shaderPath=std::filesystem::path(argv[1]);
#endif
        std::ifstream file(shaderPath,std::ios::binary|std::ios::ate); if (!file) throw std::runtime_error("Cannot open fragment artifact"); const auto size=file.tellg(); if (size<20 || size>1024*1024) throw std::runtime_error("Invalid fragment artifact size");
        std::string shader(static_cast<std::size_t>(size),'\0'); file.seekg(0); if (!file.read(shader.data(),size)) throw std::runtime_error("Cannot read fragment artifact");
        Vulkan vk; vk.init(); const auto pixels=vk.render(shader,static_cast<std::uint32_t>(width),static_cast<std::uint32_t>(height));
#if defined(_WIN32)
        if (_setmode(_fileno(stdout), _O_BINARY) == -1) throw std::runtime_error("Cannot configure binary output");
#endif
        std::cout << "P6\n" << width << ' ' << height << "\n255\n";
        for (std::size_t i=0;i<pixels.size();i+=4) std::cout.write(reinterpret_cast<const char*>(pixels.data()+i),3);
        if (!std::cout) throw std::runtime_error("Cannot write preview pixels");
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
