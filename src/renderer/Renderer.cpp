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
#include "renderer/vulkan/Background3DTarget.hpp"
#include "renderer/vulkan/Background3DPipeline.hpp"
#include "renderer/vulkan/Background3DScene.hpp"
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
    std::array<std::unique_ptr<vk::Background3DScene>, 10> backgroundScenes;
    std::array<std::unique_ptr<vk::Background3DTarget>, vk::kFramesInFlight> backgroundTargets;
    std::array<std::unique_ptr<vk::Background3DPipeline>, vk::kFramesInFlight> backgroundPipelines;
    std::array<std::unique_ptr<vk::BlurPass>, vk::kFramesInFlight> backgroundDirectPasses;
    std::array<std::unique_ptr<vk::BlurPass>, vk::kFramesInFlight> backgroundScenePasses;
    std::array<std::unique_ptr<vk::SceneTarget>, vk::kFramesInFlight> backgroundBlurTargets;
    std::array<std::unique_ptr<vk::BlurPass>, vk::kFramesInFlight> backgroundHorizontalPasses;
    std::array<std::unique_ptr<vk::BlurPass>, vk::kFramesInFlight> backgroundBlurDirectPasses;
    std::array<std::unique_ptr<vk::BlurPass>, vk::kFramesInFlight> backgroundBlurScenePasses;

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
        constexpr std::array<const char*, 10> sceneNames{"city-transit", "lunar-horizon", "mountain-parallax", "", "ocean-lighthouse", "", "orbital-station", "forest-fireflies", "", ""};
        for (std::size_t index = 0; index < sceneNames.size(); ++index) {
            if (sceneNames[index][0] == '\0') continue;
            try { backgroundScenes[index] = std::make_unique<vk::Background3DScene>(device, sceneNames[index]); }
            catch (const std::exception& error) { log::warn(std::string("3D background unavailable: ") + error.what()); }
        }
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
            backgroundDirectPasses[index].reset(); backgroundScenePasses[index].reset();
            backgroundHorizontalPasses[index].reset(); backgroundBlurDirectPasses[index].reset(); backgroundBlurScenePasses[index].reset(); backgroundBlurTargets[index].reset();
            backgroundPipelines[index].reset(); backgroundTargets[index].reset();
        }
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
            if (std::any_of(backgroundScenes.begin(), backgroundScenes.end(), [](const auto& scene) { return static_cast<bool>(scene); })) {
                try {
                    // Keep pixel-art pixels similar in logical size on both
                    // standard-density and Retina/high-DPI displays.
                    const auto pixelScale = static_cast<std::uint32_t>(
                        std::clamp(std::lround(window.contentScale() * 2.0F), 2L, 4L));
                    const VkExtent2D backgroundExtent{std::max(1U, extent.width / pixelScale), std::max(1U, extent.height / pixelScale)};
                    backgroundTargets[index] = std::make_unique<vk::Background3DTarget>(device, swapchain.format(), backgroundExtent);
                    backgroundPipelines[index] = std::make_unique<vk::Background3DPipeline>(device, backgroundTargets[index]->renderPass());
                    backgroundDirectPasses[index] = std::make_unique<vk::BlurPass>(device, swapchain.renderPass(), swapchain.format(), extent,
                        backgroundTargets[index]->colorView(), backgroundTargets[index]->sampler(), vk::BlurAxis::Horizontal);
                    backgroundScenePasses[index] = std::make_unique<vk::BlurPass>(device, sceneTargets[index]->renderPass(), swapchain.format(), extent,
                        backgroundTargets[index]->colorView(), backgroundTargets[index]->sampler(), vk::BlurAxis::Horizontal);
                    try {
                        backgroundBlurTargets[index] = std::make_unique<vk::SceneTarget>(device, swapchain.format(), extent);
                        backgroundHorizontalPasses[index] = std::make_unique<vk::BlurPass>(device, backgroundBlurTargets[index]->renderPass(), swapchain.format(), extent,
                            backgroundTargets[index]->colorView(), backgroundTargets[index]->linearSampler(), vk::BlurAxis::Horizontal);
                        backgroundBlurDirectPasses[index] = std::make_unique<vk::BlurPass>(device, swapchain.renderPass(), swapchain.format(), extent,
                            backgroundBlurTargets[index]->view(), backgroundBlurTargets[index]->sampler(), vk::BlurAxis::Vertical);
                        backgroundBlurScenePasses[index] = std::make_unique<vk::BlurPass>(device, sceneTargets[index]->renderPass(), swapchain.format(), extent,
                            backgroundBlurTargets[index]->view(), backgroundBlurTargets[index]->sampler(), vk::BlurAxis::Vertical);
                    } catch (const std::exception& error) {
                        log::warn(std::string("3D background blur unavailable: ") + error.what());
                        backgroundHorizontalPasses[index].reset(); backgroundBlurDirectPasses[index].reset(); backgroundBlurScenePasses[index].reset(); backgroundBlurTargets[index].reset();
                    }
                } catch (const std::exception& error) {
                    log::warn(std::string("3D background renderer unavailable: ") + error.what());
                    backgroundDirectPasses[index].reset(); backgroundScenePasses[index].reset();
                    backgroundHorizontalPasses[index].reset(); backgroundBlurDirectPasses[index].reset(); backgroundBlurScenePasses[index].reset(); backgroundBlurTargets[index].reset();
                    backgroundPipelines[index].reset(); backgroundTargets[index].reset();
                }
            }
        }
        // A partial allocation must not alternate between rendered and empty
        // backgrounds as frames rotate through the in-flight targets.
        const bool completeBackgroundTargets = std::all_of(backgroundTargets.begin(), backgroundTargets.end(),
            [](const auto& target) { return static_cast<bool>(target); });
        if (!completeBackgroundTargets) {
            for (std::size_t index = 0; index < vk::kFramesInFlight; ++index) {
                backgroundDirectPasses[index].reset(); backgroundScenePasses[index].reset();
                backgroundHorizontalPasses[index].reset(); backgroundBlurDirectPasses[index].reset(); backgroundBlurScenePasses[index].reset(); backgroundBlurTargets[index].reset();
                backgroundPipelines[index].reset(); backgroundTargets[index].reset();
            }
        }
        // If only some frames obtained blur resources, use the same unblurred
        // fallback for every frame rather than flickering between the two.
        const bool completeBackgroundBlur = std::all_of(backgroundBlurTargets.begin(), backgroundBlurTargets.end(),
            [](const auto& target) { return static_cast<bool>(target); });
        if (!completeBackgroundBlur) {
            for (std::size_t index = 0; index < vk::kFramesInFlight; ++index) {
                backgroundHorizontalPasses[index].reset(); backgroundBlurDirectPasses[index].reset(); backgroundBlurScenePasses[index].reset(); backgroundBlurTargets[index].reset();
            }
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
        const auto sceneIndex = state.backgroundScene;
        const bool renderBackground3D = state.backgroundMotion && sceneIndex >= 0 &&
            static_cast<std::size_t>(sceneIndex) < backgroundScenes.size() && backgroundScenes[static_cast<std::size_t>(sceneIndex)] &&
            backgroundTargets[currentFrame] && backgroundPipelines[currentFrame] &&
            backgroundDirectPasses[currentFrame] && backgroundScenePasses[currentFrame];
        const float backgroundBlurRadius = 16.0F * contentScale * std::clamp(state.backgroundDim, 0.0F, 1.0F);
        const bool blurBackground3D = renderBackground3D && backgroundBlurRadius > 0.01F &&
            backgroundBlurTargets[currentFrame] && backgroundHorizontalPasses[currentFrame] &&
            backgroundBlurDirectPasses[currentFrame] && backgroundBlurScenePasses[currentFrame];
        // All supported choices use 3D assets. If a device or asset fails to
        // initialize, leave the background clear instead of showing an
        // unrelated legacy 2D placeholder for the selected scene.
        if (state.backgroundScene >= 0 && static_cast<std::size_t>(state.backgroundScene) < backgroundScenes.size())
            sceneState.backgroundMotion = false;
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

        if (renderBackground3D) {
            std::array<VkClearValue, 2> sceneClear{};
            sceneClear[0] = clearValue;
            sceneClear[1].depthStencil = {1.0F, 0};
            VkRenderPassBeginInfo backgroundInfo{}; backgroundInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            backgroundInfo.renderPass = backgroundTargets[currentFrame]->renderPass();
            backgroundInfo.framebuffer = backgroundTargets[currentFrame]->framebuffer();
            backgroundInfo.renderArea.extent = backgroundTargets[currentFrame]->extent();
            backgroundInfo.clearValueCount = static_cast<std::uint32_t>(sceneClear.size()); backgroundInfo.pClearValues = sceneClear.data();
            vkCmdBeginRenderPass(commandBuffer, &backgroundInfo, VK_SUBPASS_CONTENTS_INLINE);
            std::array<float,4> primary{state.settingsPrimary.r, state.settingsPrimary.g, state.settingsPrimary.b, 1.0F};
            if (vk::isSrgbFormat(swapchain.format())) primary = vk::srgbToLinear(primary);
            backgroundScenes[static_cast<std::size_t>(sceneIndex)]->draw(commandBuffer, *backgroundPipelines[currentFrame], backgroundTargets[currentFrame]->extent(), state.animationTime, state.backgroundDim, clear, {primary[0], primary[1], primary[2]}, state.backgroundNaturalColors);
            vkCmdEndRenderPass(commandBuffer);
            shaderReadBarrier(commandBuffer, backgroundTargets[currentFrame]->colorImage());
        }
        if (blurBackground3D) {
            VkRenderPassBeginInfo blurInfo{};
            blurInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            blurInfo.renderPass = backgroundBlurTargets[currentFrame]->renderPass();
            blurInfo.framebuffer = backgroundBlurTargets[currentFrame]->framebuffer();
            blurInfo.renderArea.extent = swapchain.extent();
            blurInfo.clearValueCount = 1; blurInfo.pClearValues = &clearValue;
            vkCmdBeginRenderPass(commandBuffer, &blurInfo, VK_SUBPASS_CONTENTS_INLINE);
            backgroundHorizontalPasses[currentFrame]->draw(commandBuffer, backgroundBlurRadius, 0.0F);
            vkCmdEndRenderPass(commandBuffer);
            shaderReadBarrier(commandBuffer, backgroundBlurTargets[currentFrame]->image());
        }

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
            if (blurBackground3D) backgroundBlurDirectPasses[currentFrame]->draw(commandBuffer, backgroundBlurRadius, 0.0F);
            else if (renderBackground3D) backgroundDirectPasses[currentFrame]->draw(commandBuffer, 0.0F, 0.0F);
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
            if (blurBackground3D) backgroundBlurScenePasses[currentFrame]->draw(commandBuffer, backgroundBlurRadius, 0.0F);
            else if (renderBackground3D) backgroundScenePasses[currentFrame]->draw(commandBuffer, 0.0F, 0.0F);
            sceneUiPasses[currentFrame]->draw(commandBuffer, currentFrame, sceneState);
            vkCmdEndRenderPass(commandBuffer);

            shaderReadBarrier(commandBuffer, sceneTargets[currentFrame]->image());

            const float commandAmount =
                std::clamp(state.opacity, 0.0F, 1.0F);
            const float blurRadius = 18.0F * contentScale * commandAmount;

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
