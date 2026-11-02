#include "renderer/Renderer.hpp"

#include "core/Version.hpp"
#include "logging/Log.hpp"
#include "platform/window/Window.hpp"
#include "renderer/vulkan/CommandPool.hpp"
#include "renderer/vulkan/Device.hpp"
#include "renderer/vulkan/FrameSync.hpp"
#include "renderer/vulkan/BlurPass.hpp"
#include "renderer/vulkan/Instance.hpp"
#include "renderer/vulkan/SceneTarget.hpp"
#include "renderer/vulkan/Surface.hpp"
#include "renderer/vulkan/Swapchain.hpp"
#include "renderer/vulkan/UiPass.hpp"
#include "renderer/vulkan/VulkanCommon.hpp"
#include "renderer/vulkan/VulkanConfig.hpp"
#include "text/font/FontAtlas.hpp"
#include "ui/theme/Theme.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace lucid {
namespace {

void shaderReadBarrier(VkCommandBuffer commandBuffer, VkImage image) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
}

} // namespace

struct Renderer::Impl {
    Window& window;
    vk::Instance instance;
    vk::Surface surface;
    vk::Device device;
    vk::Swapchain swapchain;
    vk::CommandPool commandPool;
    std::array<vk::FrameSync, vk::kFramesInFlight> frames;
    std::vector<VkCommandBuffer> commandBuffers;
    std::array<float, 4> clearColor{};
    std::uint32_t currentFrame = 0;
    float contentScale = 1.0F;
    float uiZoom = 1.0F;
    std::unique_ptr<text::FontAtlas> font;
    std::array<std::unique_ptr<vk::SceneTarget>, vk::kFramesInFlight> sceneTargets;
    std::array<std::unique_ptr<vk::SceneTarget>, vk::kFramesInFlight> blurTargets;
    std::array<std::unique_ptr<vk::UiPass>, vk::kFramesInFlight> sceneUiPasses;
    std::array<std::unique_ptr<vk::BlurPass>, vk::kFramesInFlight> horizontalBlurPasses;
    std::array<std::unique_ptr<vk::BlurPass>, vk::kFramesInFlight> verticalBlurPasses;
    std::unique_ptr<vk::UiPass> overlayUiPass;
    std::unique_ptr<vk::UiPass> directUiPass;

    Impl(Window& windowRef, const RendererConfig& config)
        : window(windowRef),
          instance(kAppName, kVersion, config.enableValidation,
                   Window::requiredVulkanInstanceExtensions()),
          surface(instance, window),
          device(instance, surface),
          swapchain(device, surface, window),
          commandPool(device),
          clearColor(config.clearColor) {
        for (vk::FrameSync& frame : frames) {
            frame.init(device);
        }
        commandBuffers = commandPool.allocate(vk::kFramesInFlight);

        contentScale = window.contentScale() * uiZoom;
        recreateFont();
        createUiResources();
        log::info("Renderer initialized. Presenting a dark surface.");
    }

    void recreateFont() {
        const int fontPixelHeight = std::max(
            1, static_cast<int>(std::lround(ui::theme::kFontPixelSize * contentScale)));
        font = std::make_unique<text::FontAtlas>(fontPixelHeight);
    }

    void updateContentScale(float nextScale) {
        if (std::abs(nextScale - contentScale) < 0.001F) {
            return;
        }
        device.waitIdle();
        contentScale = nextScale;
        recreateFont();
        createUiResources();
        log::info("Content scale changed to " + std::to_string(contentScale) + ".");
    }

    void createUiResources() {
        const VkExtent2D extent = swapchain.extent();
        for (std::size_t index = 0; index < vk::kFramesInFlight; ++index) {
            sceneTargets[index] =
                std::make_unique<vk::SceneTarget>(device, swapchain.format(), extent);
            blurTargets[index] =
                std::make_unique<vk::SceneTarget>(device, swapchain.format(), extent);
            sceneUiPasses[index] = std::make_unique<vk::UiPass>(
                device, sceneTargets[index]->renderPass(),
                sceneTargets[index]->format(), extent, contentScale, *font);
            horizontalBlurPasses[index] = std::make_unique<vk::BlurPass>(
                device, blurTargets[index]->renderPass(), blurTargets[index]->format(),
                extent, sceneTargets[index]->view(), sceneTargets[index]->sampler(),
                vk::BlurAxis::Horizontal);
            verticalBlurPasses[index] = std::make_unique<vk::BlurPass>(
                device, swapchain.compositeRenderPass(), swapchain.format(), extent,
                blurTargets[index]->view(), blurTargets[index]->sampler(),
                vk::BlurAxis::Vertical);
        }
        overlayUiPass = std::make_unique<vk::UiPass>(
            device, swapchain.overlayRenderPass(), swapchain.format(), extent,
            contentScale, *font);
        directUiPass = std::make_unique<vk::UiPass>(
            device, swapchain.renderPass(), swapchain.format(), extent,
            contentScale, *font);
    }

    ~Impl() {
        if (device.handle() != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device.handle());
        }
    }

    void recreateSwapchain() {
        int width = 0;
        int height = 0;
        window.framebufferSize(width, height);
        if (width <= 0 || height <= 0) {
            return;
        }

        device.waitIdle();
        swapchain.recreate(window);
        const float nextScale = window.contentScale() * uiZoom;
        if (std::abs(nextScale - contentScale) >= 0.001F) {
            contentScale = nextScale;
            recreateFont();
            log::info("Content scale changed to " + std::to_string(contentScale) +
                      " while recreating the swapchain.");
        }
        createUiResources();
    }

    void drawFrame(const ui::FrameState& state) {
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        window.framebufferSize(framebufferWidth, framebufferHeight);
        if (framebufferWidth <= 0 || framebufferHeight <= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            return;
        }

        const VkExtent2D extent = swapchain.extent();
        if (static_cast<std::uint32_t>(framebufferWidth) != extent.width ||
            static_cast<std::uint32_t>(framebufferHeight) != extent.height) {
            recreateSwapchain();
        } else {
            const float nextScale = window.contentScale() * uiZoom;
            if (std::abs(nextScale - contentScale) >= 0.001F) {
                updateContentScale(nextScale);
            }
        }

        vk::FrameSync& frame = frames[currentFrame];
        frame.wait();

        std::uint32_t imageIndex = 0;
        const VkResult acquireResult =
            vkAcquireNextImageKHR(device.handle(), swapchain.handle(), UINT64_MAX,
                                  frame.imageAvailable(), VK_NULL_HANDLE, &imageIndex);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            return;
        }
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("Failed to acquire a swapchain image: " +
                                     std::string(vk::resultToString(acquireResult)));
        }

        frame.reset();

        std::array<float, 4> clear = clearColor;
        if (vk::isSrgbFormat(swapchain.format())) {
            clear = vk::srgbToLinear(clear);
        }

        const std::vector<VkFramebuffer>& framebuffers = swapchain.framebuffers();
        if (imageIndex >= framebuffers.size()) {
            throw std::runtime_error("Swapchain image index is out of range");
        }

        const VkCommandBuffer commandBuffer = commandBuffers[currentFrame];
        commandPool.begin(commandBuffer);

        VkClearValue clearValue{};
        clearValue.color.float32[0] = clear[0];
        clearValue.color.float32[1] = clear[1];
        clearValue.color.float32[2] = clear[2];
        clearValue.color.float32[3] = clear[3];

        ui::FrameState sceneState = state;
        sceneState.commandVisible = false;
        if (state.commandVisible) {
            sceneState.showWindowTitle = false;
        }

        ui::FrameState overlayState = state;
        overlayState.hasProject = false;
        overlayState.hasDocument = false;
        overlayState.showWelcome = false;
        overlayState.diagnosticsVisible = false;
        overlayState.visibleLines.clear();
        overlayState.visibleTokens.clear();
        overlayState.statusText = {};
        overlayState.commandVisible = true;
        overlayState.backgroundMotion = false;

        if (!state.commandVisible) {
            VkRenderPassBeginInfo directInfo{};
            directInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            directInfo.renderPass = swapchain.renderPass();
            directInfo.framebuffer = framebuffers[imageIndex];
            directInfo.renderArea.offset = {0, 0};
            directInfo.renderArea.extent = swapchain.extent();
            directInfo.clearValueCount = 1;
            directInfo.pClearValues = &clearValue;

            vkCmdBeginRenderPass(commandBuffer, &directInfo,
                                 VK_SUBPASS_CONTENTS_INLINE);
            directUiPass->draw(commandBuffer, currentFrame, sceneState);
            vkCmdEndRenderPass(commandBuffer);
        } else {
            VkRenderPassBeginInfo sceneInfo{};
            sceneInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            sceneInfo.renderPass = sceneTargets[currentFrame]->renderPass();
            sceneInfo.framebuffer = sceneTargets[currentFrame]->framebuffer();
            sceneInfo.renderArea.offset = {0, 0};
            sceneInfo.renderArea.extent = swapchain.extent();
            sceneInfo.clearValueCount = 1;
            sceneInfo.pClearValues = &clearValue;

            vkCmdBeginRenderPass(commandBuffer, &sceneInfo,
                                 VK_SUBPASS_CONTENTS_INLINE);
            sceneUiPasses[currentFrame]->draw(commandBuffer, currentFrame, sceneState);
            vkCmdEndRenderPass(commandBuffer);

            shaderReadBarrier(commandBuffer, sceneTargets[currentFrame]->image());

            const float commandAmount =
                std::clamp(state.opacity, 0.0F, 1.0F);
            const float blurRadius = 14.0F * contentScale * commandAmount;

            VkRenderPassBeginInfo horizontalInfo{};
            horizontalInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            horizontalInfo.renderPass = blurTargets[currentFrame]->renderPass();
            horizontalInfo.framebuffer = blurTargets[currentFrame]->framebuffer();
            horizontalInfo.renderArea.offset = {0, 0};
            horizontalInfo.renderArea.extent = swapchain.extent();
            horizontalInfo.clearValueCount = 1;
            horizontalInfo.pClearValues = &clearValue;

            vkCmdBeginRenderPass(commandBuffer, &horizontalInfo,
                                 VK_SUBPASS_CONTENTS_INLINE);
            horizontalBlurPasses[currentFrame]->draw(commandBuffer, blurRadius, 0.0F);
            vkCmdEndRenderPass(commandBuffer);

            shaderReadBarrier(commandBuffer, blurTargets[currentFrame]->image());

            VkRenderPassBeginInfo compositeInfo{};
            compositeInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            compositeInfo.renderPass = swapchain.compositeRenderPass();
            compositeInfo.framebuffer = swapchain.compositeFramebuffers()[imageIndex];
            compositeInfo.renderArea.offset = {0, 0};
            compositeInfo.renderArea.extent = swapchain.extent();
            compositeInfo.clearValueCount = 1;
            compositeInfo.pClearValues = &clearValue;

            vkCmdBeginRenderPass(commandBuffer, &compositeInfo,
                                 VK_SUBPASS_CONTENTS_INLINE);
            verticalBlurPasses[currentFrame]->draw(commandBuffer, blurRadius,
                                                   0.26F * commandAmount);
            vkCmdEndRenderPass(commandBuffer);

            VkRenderPassBeginInfo overlayInfo{};
            overlayInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            overlayInfo.renderPass = swapchain.overlayRenderPass();
            overlayInfo.framebuffer = swapchain.overlayFramebuffers()[imageIndex];
            overlayInfo.renderArea.offset = {0, 0};
            overlayInfo.renderArea.extent = swapchain.extent();
            overlayInfo.clearValueCount = 0;
            overlayInfo.pClearValues = nullptr;

            vkCmdBeginRenderPass(commandBuffer, &overlayInfo,
                                 VK_SUBPASS_CONTENTS_INLINE);
            overlayUiPass->draw(commandBuffer, currentFrame, overlayState);
            vkCmdEndRenderPass(commandBuffer);
        }

        commandPool.end(commandBuffer);

        const VkSemaphore waitSemaphores[] = {frame.imageAvailable()};
        const VkPipelineStageFlags waitStages[] = {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        const VkSemaphore renderFinished = swapchain.renderFinished(imageIndex);
        const VkSemaphore signalSemaphores[] = {renderFinished};

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        LUCID_VK_CHECK(vkQueueSubmit(device.graphicsQueue(), 1, &submitInfo, frame.inFlight()));

        const VkSwapchainKHR swapchains[] = {swapchain.handle()};
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;

        const VkResult presentResult = vkQueuePresentKHR(device.presentQueue(), &presentInfo);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
            recreateSwapchain();
        } else if (presentResult != VK_SUCCESS) {
            throw std::runtime_error("Failed to present a swapchain image: " +
                                     std::string(vk::resultToString(presentResult)));
        }

        currentFrame = (currentFrame + 1) % vk::kFramesInFlight;
    }
};

Renderer::Renderer(Window& window, const RendererConfig& config)
    : m_impl(std::make_unique<Impl>(window, config)) {}

Renderer::~Renderer() = default;

void Renderer::drawFrame(const ui::FrameState& state) {
    m_impl->drawFrame(state);
}

void Renderer::setClearColor(const std::array<float, 4>& color) {
    m_impl->clearColor = color;
}

void Renderer::setUiZoom(float zoom) {
    const float clamped = std::clamp(zoom, 0.5F, 3.0F);
    if (std::abs(m_impl->uiZoom - clamped) < 0.001F) {
        return;
    }

    m_impl->device.waitIdle();
    m_impl->uiZoom = clamped;
    m_impl->contentScale = m_impl->window.contentScale() * clamped;
    m_impl->recreateFont();
    m_impl->createUiResources();
}

void Renderer::waitIdle() {
    m_impl->device.waitIdle();
}

float Renderer::logicalCharacterAdvance() const {
    const float scale = m_impl->contentScale > 0.0F ? m_impl->contentScale : 1.0F;
    return m_impl->font->glyph(U' ').advance / scale;
}

} // namespace lucid
