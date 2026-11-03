#pragma once

#include "renderer/vulkan/Buffer.hpp"
#include "renderer/vulkan/Texture.hpp"
#include "renderer/vulkan/VulkanConfig.hpp"
#include "ui/UiState.hpp"
#include "ui/theme/Theme.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace lucid::text {
class FontAtlas;
}

namespace lucid::vk {

class Device;

class UiPass {
public:
    UiPass(const Device& device, VkRenderPass renderPass, VkFormat format,
           VkExtent2D extent, float contentScale, const text::FontAtlas& font);
    ~UiPass();

    UiPass(const UiPass&) = delete;
    UiPass& operator=(const UiPass&) = delete;
    UiPass(UiPass&&) = delete;
    UiPass& operator=(UiPass&&) = delete;

    void updateExtent(VkExtent2D extent) noexcept { m_extent = extent; }
    [[nodiscard]] VkFormat format() const noexcept { return m_format; }

    void draw(VkCommandBuffer commandBuffer, std::uint32_t frameIndex,
              const ui::FrameState& state) const;

private:
    struct TextVertex {
        float x;
        float y;
        float u;
        float v;
        float r;
        float g;
        float b;
        float a;
    };

    struct RectPushConstants {
        float viewport[2];
        float rectMin[2];
        float rectMax[2];
        float radius;
        float borderWidth;
        float fillColor[4];
        float borderColor[4];
        float blur;
        float opacity;
    };

    struct TextPushConstants {
        float viewport[2];
        float blur;
        float opacity;
    };

    struct TextRange {
        std::uint32_t firstVertex;
        std::uint32_t vertexCount;
        VkRect2D scissor;
    };

    void createDescriptorSetLayout();
    void createDescriptorSet();
    void createPipelineLayouts();
    void createPipelines();
    void createVertexBuffers();
    void destroy() noexcept;

    void drawRect(VkCommandBuffer commandBuffer, const RectPushConstants& pushConstants) const;
    void fillRectPushConstants(RectPushConstants& out, float x0, float y0, float x1, float y1,
                               float radius, float borderWidth, float blur, float opacity,
                               const ui::theme::Color& fill,
                               const ui::theme::Color& border) const;
    [[nodiscard]] std::array<float, 4> toGpuColor(const ui::theme::Color& color) const;
    float appendText(std::vector<TextVertex>& vertices, std::string_view text, float x,
                     float baseline, const std::array<float, 4>& color) const;
    float appendText(std::vector<TextVertex>& vertices, std::u32string_view text, float x,
                     float baseline, const std::array<float, 4>& color) const;
    [[nodiscard]] float measureText(std::string_view text) const;
    [[nodiscard]] float measureText(std::u32string_view text) const;

    const Device& m_device;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent{};
    float m_scale = 1.0F;
    const text::FontAtlas& m_font;
    Texture m_fontTexture;

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout m_rectPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_textPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_rectPipeline = VK_NULL_HANDLE;
    VkPipeline m_textPipeline = VK_NULL_HANDLE;
    std::array<Buffer, kFramesInFlight> m_vertexBuffers;

    mutable float m_animatedHighlightTop = 0.0F;
    mutable float m_animatedHighlightBottom = 0.0F;
    mutable float m_animatedHighlightTopVelocity = 0.0F;
    mutable float m_animatedHighlightBottomVelocity = 0.0F;
    mutable float m_animatedCaretX = 0.0F;
    mutable float m_animatedCaretVelocity = 0.0F;
    mutable double m_lastDrawTime = 0.0;
    mutable bool m_animationInitialized = false;
};

} // namespace lucid::vk
