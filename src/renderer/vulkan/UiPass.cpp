#include "renderer/vulkan/UiPass.hpp"

#include "core/Version.hpp"
#include "editor/syntax/Syntax.hpp"
#include "ui/Overlay.hpp"
#include "ui/Welcome.hpp"
#include "ui/VisualMaskSelection.hpp"
#include "renderer/vulkan/Device.hpp"
#include "renderer/vulkan/VulkanCommon.hpp"
#include "text/font/FontAtlas.hpp"
#include "text/Utf8.hpp"
#include "text/Wrap.hpp"

#include <cstdint>

#include "shaders/ui_rect.vert.h"
#include "shaders/ui_rect.frag.h"
#include "shaders/ui_text.vert.h"
#include "shaders/ui_text.frag.h"
#include "shaders/ui_image.frag.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::vk {
namespace {

constexpr std::uint32_t kMaxTextVertices = 262144;
constexpr std::size_t kMaxLineChars = 512;

#if defined(__APPLE__)
constexpr std::u32string_view kCommandGlyph = U"\u2318";
constexpr std::u32string_view kShiftGlyph = U"\u21E7";
constexpr std::string_view kCommandFallback = "Cmd";
#else
constexpr std::u32string_view kCommandGlyph = U"\u2303 Ctrl";
constexpr std::u32string_view kShiftGlyph = U"Shift";
constexpr std::string_view kCommandFallback = "Ctrl";
#endif

struct WelcomeShortcut {
    bool command = false;
    bool shift = false;
    std::string_view key;
    std::string_view label;
    bool alt = false;
};

struct WelcomeRectSpec {
    float x0 = 0.0F;
    float y0 = 0.0F;
    float x1 = 0.0F;
    float y1 = 0.0F;
    float radius = 0.0F;
    float borderWidth = 0.0F;
    float blur = 0.0F;
    float opacity = 1.0F;
    ui::theme::Color fill;
    ui::theme::Color border;
};

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

UiPass::UiPass(const Device& device, VkRenderPass renderPass, VkFormat format,
               VkExtent2D extent, float contentScale, const text::FontAtlas& font)
    : m_device(device),
      m_renderPass(renderPass),
      m_format(format),
      m_extent(extent),
      m_scale(contentScale),
      m_font(font),
      m_fontTexture(device, static_cast<std::uint32_t>(font.width()),
                    static_cast<std::uint32_t>(font.height()), font.pixels().data(),
                    static_cast<VkDeviceSize>(font.pixels().size())) {
    try {
        createDescriptorSetLayout();
        createDescriptorSet();
        createPipelineLayouts();
        createPipelines();
        createVertexBuffers();
    } catch (...) {
        destroy();
        throw;
    }
}

UiPass::~UiPass() {
    destroy();
}

void UiPass::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.bindingCount = 1;
    createInfo.pBindings = &binding;
    LUCID_VK_CHECK(
        vkCreateDescriptorSetLayout(m_device.handle(), &createInfo, nullptr,
                                    &m_descriptorSetLayout));
}

void UiPass::createDescriptorSet() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 4;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 4;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    LUCID_VK_CHECK(
        vkCreateDescriptorPool(m_device.handle(), &poolInfo, nullptr, &m_descriptorPool));

    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = m_descriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &m_descriptorSetLayout;
    LUCID_VK_CHECK(
        vkAllocateDescriptorSets(m_device.handle(), &allocateInfo, &m_descriptorSet));

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = m_fontTexture.view();
    imageInfo.sampler = m_fontTexture.sampler();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(m_device.handle(), 1, &write, 0, nullptr);

    std::array<VkDescriptorSetLayout, 3> layouts{}; layouts.fill(m_descriptorSetLayout);
    allocateInfo.descriptorSetCount = static_cast<std::uint32_t>(layouts.size());
    allocateInfo.pSetLayouts = layouts.data();
    LUCID_VK_CHECK(vkAllocateDescriptorSets(m_device.handle(), &allocateInfo, m_previewSets.data()));
}

void UiPass::prepareVisualPreview(const ui::VisualReviewPreview* preview) {
    if (!preview || !preview->generation || preview->generation == m_previewGeneration) return;
    const std::array<const modules::VisualImage*, 3> images{&preview->baseline, &preview->candidate, &preview->difference};
    for (const auto* image : images) if (!image->width || !image->height || image->width > 1024 || image->height > 1024 || image->rgba.size() != image->width * image->height * 4) return;
    m_device.waitIdle();
    std::array<std::unique_ptr<Texture>, 3> textures;
    for (std::size_t i = 0; i < images.size(); ++i) {
        const auto& image = *images[i];
        textures[i] = std::make_unique<Texture>(m_device, static_cast<std::uint32_t>(image.width), static_cast<std::uint32_t>(image.height), image.rgba.data(), image.rgba.size(), true);
    }
    for (std::size_t i = 0; i < textures.size(); ++i) {
        VkDescriptorImageInfo info{}; info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; info.imageView = textures[i]->view(); info.sampler = textures[i]->sampler();
        VkWriteDescriptorSet write{}; write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; write.dstSet = m_previewSets[i]; write.dstBinding = 0; write.descriptorCount = 1; write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; write.pImageInfo = &info;
        vkUpdateDescriptorSets(m_device.handle(), 1, &write, 0, nullptr);
    }
    m_previewTextures = std::move(textures); m_previewGeneration = preview->generation;
}

void UiPass::createPipelineLayouts() {
    VkPushConstantRange rectRange{};
    rectRange.stageFlags =
        static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT |
                                        VK_SHADER_STAGE_FRAGMENT_BIT);
    rectRange.offset = 0;
    rectRange.size = sizeof(RectPushConstants);

    VkPipelineLayoutCreateInfo rectInfo{};
    rectInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    rectInfo.pushConstantRangeCount = 1;
    rectInfo.pPushConstantRanges = &rectRange;
    LUCID_VK_CHECK(
        vkCreatePipelineLayout(m_device.handle(), &rectInfo, nullptr,
                               &m_rectPipelineLayout));

    VkPushConstantRange textRange{};
    textRange.stageFlags =
        static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT |
                                        VK_SHADER_STAGE_FRAGMENT_BIT);
    textRange.offset = 0;
    textRange.size = sizeof(TextPushConstants);

    VkPipelineLayoutCreateInfo textInfo{};
    textInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    textInfo.setLayoutCount = 1;
    textInfo.pSetLayouts = &m_descriptorSetLayout;
    textInfo.pushConstantRangeCount = 1;
    textInfo.pPushConstantRanges = &textRange;
    LUCID_VK_CHECK(
        vkCreatePipelineLayout(m_device.handle(), &textInfo, nullptr,
                               &m_textPipelineLayout));

}

void UiPass::createPipelines() {
    VkShaderModule rectVert = VK_NULL_HANDLE;
    VkShaderModule rectFrag = VK_NULL_HANDLE;
    VkShaderModule textVert = VK_NULL_HANDLE;
    VkShaderModule textFrag = VK_NULL_HANDLE;
    VkShaderModule imageFrag = VK_NULL_HANDLE;

    try {
        rectVert = createShaderModule(m_device, ui_rect_vert_spv, sizeof(ui_rect_vert_spv));
        rectFrag = createShaderModule(m_device, ui_rect_frag_spv, sizeof(ui_rect_frag_spv));
        textVert = createShaderModule(m_device, ui_text_vert_spv, sizeof(ui_text_vert_spv));
        textFrag = createShaderModule(m_device, ui_text_frag_spv, sizeof(ui_text_frag_spv));
        imageFrag = createShaderModule(m_device, ui_image_frag_spv, sizeof(ui_image_frag_spv));

        VkPipelineShaderStageCreateInfo rectStages[2]{};
        rectStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        rectStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        rectStages[0].module = rectVert;
        rectStages[0].pName = "main";
        rectStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        rectStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        rectStages[1].module = rectFrag;
        rectStages[1].pName = "main";

        VkPipelineVertexInputStateCreateInfo rectVertexInput{};
        rectVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterization{};
        rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization.depthClampEnable = VK_FALSE;
        rasterization.rasterizerDiscardEnable = VK_FALSE;
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode = VK_CULL_MODE_NONE;
        rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization.depthBiasEnable = VK_FALSE;
        rasterization.lineWidth = 1.0F;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.minSampleShading = 1.0F;
        multisampling.alphaToCoverageEnable = VK_FALSE;
        multisampling.alphaToOneEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.blendEnable = VK_TRUE;
        blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        blendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &blendAttachment;

        const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                                VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 2;
        dynamicState.pDynamicStates = dynamicStates;

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = rectStages;
        pipelineInfo.pVertexInputState = &rectVertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterization;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_rectPipelineLayout;
        pipelineInfo.renderPass = m_renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;
        LUCID_VK_CHECK(vkCreateGraphicsPipelines(m_device.handle(), VK_NULL_HANDLE, 1,
                                                 &pipelineInfo, nullptr, &m_rectPipeline));

        VkPipelineShaderStageCreateInfo textStages[2]{};
        textStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        textStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        textStages[0].module = textVert;
        textStages[0].pName = "main";
        textStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        textStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        textStages[1].module = textFrag;
        textStages[1].pName = "main";

        const VkVertexInputBindingDescription binding{
            0, sizeof(TextVertex), VK_VERTEX_INPUT_RATE_VERTEX};
        const VkVertexInputAttributeDescription attributes[3] = {
            {0, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<std::uint32_t>(offsetof(TextVertex, x))},
            {1, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<std::uint32_t>(offsetof(TextVertex, u))},
            {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
             static_cast<std::uint32_t>(offsetof(TextVertex, r))},
        };

        VkPipelineVertexInputStateCreateInfo textVertexInput{};
        textVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        textVertexInput.vertexBindingDescriptionCount = 1;
        textVertexInput.pVertexBindingDescriptions = &binding;
        textVertexInput.vertexAttributeDescriptionCount = 3;
        textVertexInput.pVertexAttributeDescriptions = attributes;

        pipelineInfo.pStages = textStages;
        pipelineInfo.pVertexInputState = &textVertexInput;
        pipelineInfo.layout = m_textPipelineLayout;
        LUCID_VK_CHECK(vkCreateGraphicsPipelines(m_device.handle(), VK_NULL_HANDLE, 1,
                                                 &pipelineInfo, nullptr, &m_textPipeline));
        textStages[1].module = imageFrag;
        LUCID_VK_CHECK(vkCreateGraphicsPipelines(m_device.handle(), VK_NULL_HANDLE, 1,
                                                 &pipelineInfo, nullptr, &m_imagePipeline));

    } catch (...) {
        if (m_rectPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_device.handle(), m_rectPipeline, nullptr);
            m_rectPipeline = VK_NULL_HANDLE;
        }
        if (m_textPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_device.handle(), m_textPipeline, nullptr);
            m_textPipeline = VK_NULL_HANDLE;
        }
        if (m_imagePipeline != VK_NULL_HANDLE) { vkDestroyPipeline(m_device.handle(), m_imagePipeline, nullptr); m_imagePipeline = VK_NULL_HANDLE; }
        destroyShaderModule(m_device.handle(), rectVert);
        destroyShaderModule(m_device.handle(), rectFrag);
        destroyShaderModule(m_device.handle(), textVert);
        destroyShaderModule(m_device.handle(), textFrag);
        destroyShaderModule(m_device.handle(), imageFrag);
        throw;
    }

    destroyShaderModule(m_device.handle(), rectVert);
    destroyShaderModule(m_device.handle(), rectFrag);
    destroyShaderModule(m_device.handle(), textVert);
    destroyShaderModule(m_device.handle(), textFrag);
    destroyShaderModule(m_device.handle(), imageFrag);
}

void UiPass::createVertexBuffers() {
    const VkDeviceSize bufferSize =
        static_cast<VkDeviceSize>(kMaxTextVertices) * sizeof(TextVertex);
    for (Buffer& buffer : m_vertexBuffers) {
        buffer.create(m_device, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      true);
    }
}

void UiPass::destroy() noexcept {
    if (m_imagePipeline != VK_NULL_HANDLE) { vkDestroyPipeline(m_device.handle(), m_imagePipeline, nullptr); m_imagePipeline = VK_NULL_HANDLE; }
    for (Buffer& buffer : m_vertexBuffers) {
        buffer.destroy();
    }

    if (m_textPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device.handle(), m_textPipeline, nullptr);
        m_textPipeline = VK_NULL_HANDLE;
    }

    if (m_rectPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device.handle(), m_rectPipeline, nullptr);
        m_rectPipeline = VK_NULL_HANDLE;
    }

    if (m_textPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device.handle(), m_textPipelineLayout, nullptr);
        m_textPipelineLayout = VK_NULL_HANDLE;
    }

    if (m_rectPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device.handle(), m_rectPipelineLayout, nullptr);
        m_rectPipelineLayout = VK_NULL_HANDLE;
    }

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device.handle(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
        m_descriptorSet = VK_NULL_HANDLE;
    }

    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device.handle(), m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
}

void UiPass::draw(VkCommandBuffer commandBuffer, std::uint32_t frameIndex,
                  const ui::FrameState& state) const {
    const float viewportWidth = static_cast<float>(m_extent.width);
    const float viewportHeight = static_cast<float>(m_extent.height);
    if (viewportWidth <= 0.0F || viewportHeight <= 0.0F) {
        return;
    }

    const float contentScale = m_scale;
    const float margin = ui::theme::kSearchBarMargin * contentScale;
    const float lineHeight = ui::theme::kEditorLineHeight * contentScale;
    const float suggestionLineHeight = ui::theme::kSuggestionLineHeight * contentScale;
    const float fontCenter = (m_font.ascent() + m_font.descent()) * 0.5F;
    const float caretHeight = (m_font.ascent() - m_font.descent()) * 0.9F;
    const double rawDelta =
        m_lastDrawTime > 0.0 ? state.timeSeconds - m_lastDrawTime : 1.0;
    const float speed = std::clamp(state.animationSpeed, 0.25F, 4.0F);
    const float animationDelta =
        static_cast<float>(std::clamp(rawDelta, 0.0, 0.05)) * speed;
    const auto springStep = [](float& value, float& velocity, float target,
                               float delta) {
        constexpr float omega = 28.0F;
        constexpr float damping = 0.92F;
        constexpr float maxStep = 1.0F / 240.0F;
        float remaining = delta;
        while (remaining > 0.0F) {
            const float step = std::min(remaining, maxStep);
            const float acceleration =
                omega * omega * (target - value) -
                2.0F * damping * omega * velocity;
            velocity += acceleration * step;
            value += velocity * step;
            remaining -= step;
        }
    };
    m_lastDrawTime = state.timeSeconds;

    if (!state.hasDocument) {
        m_animationInitialized = false;
    }

    VkViewport viewport{};
    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = viewportWidth;
    viewport.height = viewportHeight;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    VkRect2D fullScissor{};
    fullScissor.offset = {0, 0};
    fullScissor.extent = m_extent;

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &fullScissor);

    std::vector<TextVertex> vertices;
    vertices.reserve(16384);
    std::vector<WelcomeRectSpec> welcomeRects;
    welcomeRects.reserve(16);
    std::vector<WelcomeRectSpec> suggestionIcons;
    suggestionIcons.reserve(32);
    std::vector<std::array<float, 4>> selectionRects;
    selectionRects.reserve(16);
    std::vector<WelcomeRectSpec> editorCaretRects;
    editorCaretRects.reserve(8);
    std::vector<WelcomeRectSpec> diagnosticRects;
    diagnosticRects.reserve(24);
    std::vector<WelcomeRectSpec> overlayRects, visualMaskRects;
    std::vector<WelcomeRectSpec> diagramRects;
    std::vector<TextVertex> diagramVertices;
    VkRect2D diagramScissor = fullScissor;
    struct PreviewQuad { float x0, y0, x1, y1; std::size_t image; };
    std::vector<PreviewQuad> previewQuads;
    overlayRects.reserve(48);
    std::vector<WelcomeRectSpec> compileMarkers;
    compileMarkers.reserve(32);
    std::vector<WelcomeRectSpec> terminalRects;
    terminalRects.reserve(8);
    TextRange terminalTextRange{0, 0, fullScissor};
    bool hasTerminalText = false;
    std::vector<WelcomeRectSpec> settingsPanelRects;
    settingsPanelRects.reserve(64);
    TextRange settingsPanelTextRange{0, 0, fullScissor};
    bool hasSettingsPanelText = false;

    const auto canAppend = [&vertices](std::size_t count) {
        return vertices.size() <= static_cast<std::size_t>(kMaxTextVertices) &&
               count <= static_cast<std::size_t>(kMaxTextVertices) - vertices.size();
    };
    const auto makeRange = [&vertices](std::size_t first, VkRect2D scissor) {
        return TextRange{static_cast<std::uint32_t>(first),
                         static_cast<std::uint32_t>(vertices.size() - first), scissor};
    };

    TextRange completionRange{0, 0, fullScissor};
    std::vector<WelcomeRectSpec> completionRects;
    bool hasCompletionText = false;
    TextRange editorRange{0, 0, fullScissor};
    bool hasEditor = false;
    bool hasHighlight = false;
    float highlightTop = 0.0F;
    float highlightBottom = 0.0F;

    const float editorTop = ui::theme::kEditorTop * contentScale;
    const float editorBottom = ui::theme::kEditorBottom * contentScale;
    const float codeLeft = ui::theme::kEditorCodeLeft * contentScale;
    const float gutterRight = ui::theme::kEditorGutterRight * contentScale;

    std::int32_t editorScissorY = static_cast<std::int32_t>(std::floor(editorTop));
    std::int32_t editorScissorHeight =
        static_cast<std::int32_t>(std::ceil(viewportHeight - editorTop - editorBottom));
    const std::int32_t maxEditorScissorHeight =
        static_cast<std::int32_t>(m_extent.height) - editorScissorY;
    editorScissorHeight = std::min(editorScissorHeight, maxEditorScissorHeight);

    std::size_t drawnLines = 0;
    if (state.hasDocument && !state.visibleLines.empty() && editorScissorY >= 0 &&
        editorScissorHeight > 0) {
        VkRect2D editorScissor{};
        editorScissor.offset = {0, editorScissorY};
        editorScissor.extent = {m_extent.width,
                                static_cast<std::uint32_t>(editorScissorHeight)};

        const std::size_t editorFirst = vertices.size();
        const std::array<float, 4> numberColor = toGpuColor(ui::theme::kLineNumber);
        const std::array<float, 4> currentNumberColor = toGpuColor(ui::theme::kAccent);
        const std::array<float, 4> errorColor = toGpuColor(ui::theme::kError);
        const std::array<float, 4> plainColor = toGpuColor(ui::theme::kText);
        const std::array<std::array<float, 4>, 15> syntaxColors = {
            plainColor,
            toGpuColor(ui::theme::kSyntaxKeyword),
            toGpuColor(ui::theme::kSyntaxType),
            toGpuColor(ui::theme::kSyntaxString),
            toGpuColor(ui::theme::kSyntaxNumber),
            toGpuColor(ui::theme::kSyntaxComment),
            toGpuColor(ui::theme::kSyntaxPreprocessor),
            toGpuColor(ui::theme::kSyntaxFunction),
            toGpuColor(ui::theme::kSyntaxPunctuation),
            toGpuColor(ui::theme::kSyntaxFunction),
            toGpuColor(ui::theme::kSyntaxCompileComment),
            toGpuColor(ui::theme::kSyntaxCompileDirective),
            toGpuColor(ui::theme::kSyntaxCompileFunction),
            toGpuColor(ui::theme::kSyntaxCompileCommand),
            toGpuColor(ui::theme::kSyntaxCompileVariable),
        };

        for (std::size_t index = 0; index < state.visibleLines.size(); ++index) {
            const std::string number =
                std::to_string(state.firstLine + static_cast<int>(index) + 1);
            const std::u32string_view line = state.visibleLines[index];
            const std::size_t lineChars = std::min(line.size(), kMaxLineChars);
            const std::size_t estimated = (number.size() + lineChars) * 6U + 8U;
            if (!canAppend(estimated)) {
                break;
            }

            const float rowTop = editorTop + static_cast<float>(index) * lineHeight;
            const float baseline = rowTop + lineHeight * 0.5F + fontCenter;
            const float numberWidth = measureText(number);
            const int absoluteLine = state.firstLine + static_cast<int>(index);
            if (index < state.coverageLines.size() && state.coverageLines[index] >= 0) {
                const auto coverageColor = state.coverageLines[index] ? ui::theme::kSyntaxString : ui::theme::kError;
                compileMarkers.push_back(WelcomeRectSpec{0.0F, rowTop + lineHeight * 0.18F, 2.0F * contentScale,
                    rowTop + lineHeight * 0.82F, 0.0F, 0.0F, 0.0F, 0.9F, coverageColor, coverageColor});
            }
            const bool currentLine = absoluteLine == state.cursorRow;
            const bool diagnosticLine =
                state.diagnosticsVisible && absoluteLine == state.diagnosticLine;
            if (diagnosticLine) {
                diagnosticRects.push_back(WelcomeRectSpec{
                    0.0F, rowTop, viewportWidth, rowTop + lineHeight, 0.0F, 0.0F,
                    0.0F, 0.08F, ui::theme::kError, ui::theme::kError});
            }
            const bool breakpoint = std::find(state.debugBreakpointLines.begin(), state.debugBreakpointLines.end(), absoluteLine) != state.debugBreakpointLines.end();
            if (breakpoint) appendText(vertices, "B", 4.0F * contentScale, baseline, toGpuColor(ui::theme::kError));
            if (absoluteLine == state.debugStoppedLine) appendText(vertices, ">", 16.0F * contentScale, baseline, toGpuColor(ui::theme::kText));
            if (state.showLineNumbers) {
                appendText(vertices, number, gutterRight - numberWidth, baseline,
                           diagnosticLine ? errorColor
                                          : (currentLine ? currentNumberColor
                                                         : numberColor));
            }

            const bool compileLine =
                std::find(state.compileFunctionLines.begin(),
                          state.compileFunctionLines.end(), absoluteLine) !=
                state.compileFunctionLines.end();
            if (compileLine) {
                const float markerWidth = 3.0F * contentScale;
                const float markerHeight = lineHeight * 0.52F;
                compileMarkers.push_back(WelcomeRectSpec{
                    gutterRight + 4.0F * contentScale,
                    rowTop + (lineHeight - markerHeight) * 0.5F,
                    gutterRight + 4.0F * contentScale + markerWidth,
                    rowTop + (lineHeight + markerHeight) * 0.5F, 1.0F * contentScale,
                    0.0F, 0.0F, 1.0F, ui::theme::kSyntaxCompileFunction,
                    ui::theme::kSyntaxCompileFunction});
            }

            if (state.hasSelection && absoluteLine >= state.selectionStartLine &&
                absoluteLine <= state.selectionEndLine) {
                const std::size_t selectionStart =
                    absoluteLine == state.selectionStartLine
                        ? std::min(static_cast<std::size_t>(std::max(
                                       state.selectionStartColumn, 0)),
                                   line.size())
                        : 0;
                const std::size_t selectionEnd =
                    absoluteLine == state.selectionEndLine
                        ? std::min(static_cast<std::size_t>(std::max(
                                       state.selectionEndColumn, 0)),
                                   line.size())
                        : line.size();
                if (selectionEnd > selectionStart ||
                    state.selectionEndLine > absoluteLine) {
                    const float selectionX0 =
                        codeLeft + measureText(line.substr(0, selectionStart));
                    float selectionX1 =
                        codeLeft + measureText(line.substr(0, selectionEnd));
                    if (state.selectionEndLine > absoluteLine) {
                        selectionX1 += lineHeight * 0.32F;
                    }
                    const float selectionY0 = rowTop + lineHeight * 0.08F;
                    const float selectionY1 = rowTop + lineHeight * 0.92F;
                    selectionRects.push_back(
                        {selectionX0, selectionY0, selectionX1, selectionY1});
                }
            }

            if (diagnosticLine) {
                const std::size_t start = std::min(
                    static_cast<std::size_t>(std::max(state.diagnosticColumn - 1, 0)),
                    line.size());
                const std::size_t length = std::max(1, state.diagnosticLength);
                const std::size_t end = std::min(start + length, line.size());
                const float x0 = codeLeft + measureText(line.substr(0, start));
                float x1 = codeLeft + measureText(line.substr(0, end));
                if (x1 <= x0) {
                    x1 = x0 + 8.0F * contentScale;
                }
                const float underlineY = rowTop + lineHeight - 2.0F * contentScale;
                diagnosticRects.push_back(WelcomeRectSpec{
                    x0, underlineY, x1, underlineY + 1.5F * contentScale, 0.0F,
                    0.0F, 0.0F, 0.9F, ui::theme::kError, ui::theme::kError});
            }

            const std::span<const editor::TokenSpan> tokens =
                index < state.visibleTokens.size() ? state.visibleTokens[index]
                                                    : std::span<const editor::TokenSpan>{};
            if (tokens.empty()) {
                appendText(vertices, line.substr(0, lineChars), codeLeft, baseline,
                           plainColor);
            } else {
                float pen = codeLeft;
                std::size_t consumed = 0;
                for (const editor::TokenSpan& token : tokens) {
                    if (consumed >= lineChars) {
                        break;
                    }
                    const std::size_t start =
                        std::min(static_cast<std::size_t>(token.start), lineChars);
                    const std::size_t length =
                        std::min(static_cast<std::size_t>(token.length), lineChars - start);
                    if (start > consumed) {
                        pen = appendText(vertices, line.substr(consumed, start - consumed),
                                         pen, baseline, plainColor);
                    }
                    if (length > 0) {
                        const std::size_t colorIndex = std::min(
                            static_cast<std::size_t>(token.role),
                            syntaxColors.size() - 1);
                        pen = appendText(vertices, line.substr(start, length), pen, baseline,
                                         syntaxColors[colorIndex]);
                    }
                    consumed = start + length;
                }
                if (consumed < lineChars) {
                    appendText(vertices, line.substr(consumed, lineChars - consumed), pen,
                               baseline, plainColor);
                }
            }
            if (absoluteLine == state.debugStoppedLine && !state.debugInline.empty() && canAppend(state.debugInline.size() * 6U + 6U)) {
                const float inlineX = codeLeft + measureText(line.substr(0, lineChars)) + 24.0F * contentScale;
                if (inlineX < viewportWidth - 30.0F * contentScale)
                    appendText(vertices, state.debugInline, inlineX, baseline, toGpuColor(ui::theme::kTextMuted));
            }
            ++drawnLines;
        }

        if (vertices.size() > editorFirst) {
            editorRange = makeRange(editorFirst, editorScissor);
            hasEditor = true;
        }
    }

    if (state.completionVisible && state.cursorRow >= 0 &&
        static_cast<std::size_t>(state.cursorRow) < state.visibleLines.size()) {
        const auto line = state.visibleLines[static_cast<std::size_t>(state.cursorRow)];
        const auto col = std::min(line.size(), static_cast<std::size_t>(std::max(0, state.cursorColumn)));
        const float width = std::min(280.0F * contentScale, viewportWidth);
        const auto layout = ui::completionListLayout(editorTop, viewportHeight - editorBottom,
            editorTop + static_cast<float>(state.cursorRow) * lineHeight, lineHeight,
            8.0F * contentScale, state.completions.size(),
            static_cast<std::size_t>(std::max(0, state.selectedCompletion)));
        const float height = layout.height;
        const float x = std::clamp(codeLeft + measureText(line.substr(0, col)), 0.0F,
                                   std::max(0.0F, viewportWidth - width));
        const float y = layout.y;
        completionRects.push_back({x, y, x + width, y + height, 5.0F * contentScale,
                                   contentScale, 0.0F, 1.0F, ui::theme::kSurface, ui::theme::kBorder});
        const std::size_t first = vertices.size();
        const float labelWidth = std::max(0.0F, width - 16.0F * contentScale);
        const auto fitLabel = [this, labelWidth](std::string_view label) {
            const float tabAdvance = m_font.glyph(U' ').advance * 4.0F;
            const auto advance = [this, tabAdvance](char32_t c) {
                return c == U'\t' ? tabAdvance : m_font.glyph(c).advance;
            };
            const auto fit = text::fitUtf8Prefix(label, labelWidth, advance);
            if (fit == label.size()) return std::string(label);
            const std::string_view dots = "...";
            const auto dotsFit = text::fitUtf8Prefix(dots, labelWidth, advance);
            const auto suffix = dots.substr(0, dotsFit);
            const auto prefix = text::fitUtf8Prefix(label, std::max(0.0F, labelWidth - measureText(suffix)), advance);
            return std::string(label.substr(0, prefix)) + std::string(suffix);
        };
        if (state.completions.empty()) {
            appendText(vertices, fitLabel("No matching functions"), x + 8.0F * contentScale,
                       y + 4.0F * contentScale + lineHeight * 0.5F + fontCenter,
                       toGpuColor(ui::theme::kTextMuted));
        }
        for (std::size_t i = layout.first; i < std::min(state.completions.size(), layout.first + layout.count); ++i) {
            const float row = y + 4.0F * contentScale + static_cast<float>(i - layout.first) * lineHeight;
            if (static_cast<int>(i) == state.selectedCompletion)
                completionRects.push_back({x + 2.0F * contentScale, row, x + width - 2.0F * contentScale,
                    row + lineHeight, 3.0F * contentScale, 0.0F, 0.0F, 1.0F,
                    ui::theme::kSelection, ui::theme::kSelection});
            appendText(vertices, fitLabel(state.completions[i]), x + 8.0F * contentScale,
                       row + lineHeight * 0.5F + fontCenter, toGpuColor(ui::theme::kText));
        }
        const float cueWidth = std::min(28.0F * contentScale, width * 0.4F);
        const float cueX = x + (width - cueWidth) * 0.5F;
        const float cueHeight = std::max(1.0F, contentScale);
        if (layout.first > 0)
            completionRects.push_back({cueX, y + contentScale, cueX + cueWidth, y + contentScale + cueHeight,
                cueHeight * 0.5F, 0.0F, 0.0F, 0.55F, ui::theme::kTextMuted, ui::theme::kTextMuted});
        if (layout.first + layout.count < state.completions.size())
            completionRects.push_back({cueX, y + height - contentScale - cueHeight, cueX + cueWidth, y + height - contentScale,
                cueHeight * 0.5F, 0.0F, 0.0F, 0.55F, ui::theme::kTextMuted, ui::theme::kTextMuted});
        if (vertices.size() > first) { completionRange = makeRange(first, fullScissor); hasCompletionText = true; }
    }

    if (state.hasDocument && state.cursorRow >= 0 &&
        static_cast<std::size_t>(state.cursorRow) < drawnLines) {
        highlightTop = editorTop + static_cast<float>(state.cursorRow) * lineHeight;
        highlightBottom = highlightTop + lineHeight;
        hasHighlight = state.highlightCurrentLine;

        const bool snap = !m_animationInitialized ||
                          std::abs(highlightTop - m_animatedHighlightTop) >
                              lineHeight * 5.0F;
        if (snap) {
            m_animatedHighlightTop = highlightTop;
            m_animatedHighlightBottom = highlightBottom;
            m_animatedHighlightTopVelocity = 0.0F;
            m_animatedHighlightBottomVelocity = 0.0F;
        } else {
            springStep(m_animatedHighlightTop, m_animatedHighlightTopVelocity,
                       highlightTop, animationDelta);
            springStep(m_animatedHighlightBottom, m_animatedHighlightBottomVelocity,
                       highlightBottom, animationDelta);
        }

        const std::u32string_view line =
            state.visibleLines[static_cast<std::size_t>(state.cursorRow)];
        const std::size_t column = std::min(
            static_cast<std::size_t>(std::max(state.cursorColumn, 0)), line.size());
        const float caretTargetX = codeLeft + measureText(line.substr(0, column));

        if (!m_animationInitialized ||
            std::abs(caretTargetX - m_animatedCaretX) > viewportWidth * 0.45F) {
            m_animatedCaretX = caretTargetX;
            m_animatedCaretVelocity = 0.0F;
        } else {
            springStep(m_animatedCaretX, m_animatedCaretVelocity, caretTargetX,
                       animationDelta);
        }

        if (!state.cursorBlink ||
            std::fmod(state.timeSeconds * speed, 1.06) < 0.53) {
            const std::u32string_view caretGlyph =
                column < line.size() ? line.substr(column, 1)
                                     : std::u32string_view(U" ");
            const float cellWidth = std::max(1.0F, measureText(caretGlyph));
            const float cellTop = m_animatedHighlightTop + lineHeight * 0.08F;
            const float cellBottom = m_animatedHighlightTop + lineHeight * 0.92F;

            const auto pushCaretRect = [&](float x0, float y0, float x1, float y1,
                                           float radius,
                                           const ui::theme::Color& fill) {
                editorCaretRects.push_back(
                    WelcomeRectSpec{x0, y0, x1, y1, radius, 0.0F, 0.0F, 1.0F,
                                    fill, fill});
            };

            switch (state.cursorStyle) {
            case editor::CursorStyle::Line: {
                const float caretTop =
                    m_animatedHighlightTop + (lineHeight - caretHeight) * 0.5F;
                pushCaretRect(m_animatedCaretX, caretTop,
                              m_animatedCaretX +
                                  std::max(1.0F, 1.5F * contentScale),
                              caretTop + caretHeight,
                              std::max(0.5F, 0.75F * contentScale),
                              ui::theme::kCaret);
                break;
            }
            case editor::CursorStyle::Block: {
                pushCaretRect(m_animatedCaretX, cellTop,
                              m_animatedCaretX + cellWidth, cellBottom,
                              std::min(cellWidth * 0.5F, 1.5F * contentScale),
                              ui::theme::kCaret);
                break;
            }
            case editor::CursorStyle::Underscore: {
                const float thickness = std::max(1.5F, 2.0F * contentScale);
                pushCaretRect(m_animatedCaretX, cellBottom - thickness,
                              m_animatedCaretX + cellWidth, cellBottom,
                              thickness * 0.5F, ui::theme::kCaret);
                break;
            }
            case editor::CursorStyle::Kitty: {
                const float lineWidth = std::max(1.0F, 1.5F * contentScale);
                const float caretTop =
                    m_animatedHighlightTop + (lineHeight - caretHeight) * 0.5F;
                pushCaretRect(m_animatedCaretX, caretTop,
                              m_animatedCaretX + lineWidth,
                              caretTop + caretHeight,
                              lineWidth * 0.5F, ui::theme::kCaret);

                const float earInset = std::max(0.5F, 0.5F * contentScale);
                const float earWidth = std::max(1.5F, 2.4F * contentScale);
                const float earHeight = std::max(2.0F, 3.2F * contentScale);
                const float earX0 = m_animatedCaretX + lineWidth - earInset;
                const float earX1 = earX0 + earWidth;
                const float upperEarY = caretTop + std::max(1.0F, 1.4F * contentScale);
                const float lowerEarY =
                    upperEarY + earHeight + std::max(1.0F, 1.2F * contentScale);
                pushCaretRect(earX0, upperEarY, earX1,
                              upperEarY + earHeight, earWidth * 0.45F,
                              ui::theme::kCaret);
                pushCaretRect(earX0, lowerEarY,
                              earX0 + earWidth * 0.82F,
                              lowerEarY + earHeight * 0.82F,
                              earWidth * 0.36F, ui::theme::kCaret);
                break;
            }
            }
        }

        m_animationInitialized = true;
    }

    TextRange windowTitleRange{0, 0, fullScissor};
    bool hasWindowTitle = false;
    if (state.showWindowTitle && !state.windowTitle.empty()) {
        std::string title(state.windowTitle);
        const float maxWidth =
            std::max(80.0F * contentScale, viewportWidth - 160.0F * contentScale);
        bool truncated = false;
        while (!title.empty() && measureText(title) > maxWidth) {
            title.resize(text::lastUtf8Start(title));
            truncated = true;
        }
        if (truncated) {
            title += "...";
        }
        const float titleWidth = measureText(title);
        const std::array<float, 4> titleColor = toGpuColor(ui::theme::kTextMuted);
        const std::size_t titleFirst = vertices.size();
        appendText(vertices, title, (viewportWidth - titleWidth) * 0.5F,
                   18.0F * contentScale + fontCenter, titleColor);
        if (vertices.size() > titleFirst) {
            windowTitleRange = makeRange(titleFirst, fullScissor);
            hasWindowTitle = true;
        }
    }

    TextRange ribbonRange{0, 0, fullScissor};
    bool hasRibbon = false;
    bool hasDirtyDot = false;
    float dirtyDotX0 = 0.0F;
    float dirtyDotX1 = 0.0F;
    float dirtyDotY0 = 0.0F;
    float dirtyDotY1 = 0.0F;

    if (state.hasProject && !state.commandVisible && !state.showWindowTitle) {
        const std::size_t ribbonFirst = vertices.size();
        const float ribbonTop = margin;
        const float baseline = ribbonTop + lineHeight * 0.5F + fontCenter;
        const std::array<float, 4> muted = toGpuColor(ui::theme::kTextMuted);
        const std::array<float, 4> text = toGpuColor(ui::theme::kText);
        float pen = margin;

        pen = appendText(vertices, state.projectName, pen, baseline, muted);
        if (state.hasDocument && !state.filePath.empty()) {
            pen = appendText(vertices, " / ", pen, baseline, muted);
            const std::size_t slash = state.filePath.find_last_of('/');
            if (slash != std::string_view::npos) {
                pen = appendText(vertices, state.filePath.substr(0, slash + 1), pen, baseline,
                                 muted);
            }
            const std::string_view filename =
                slash == std::string_view::npos ? state.filePath
                                                : state.filePath.substr(slash + 1);
            pen = appendText(vertices, filename, pen, baseline, text);

            if (state.dirty) {
                const float centerY = ribbonTop + lineHeight * 0.5F;
                hasDirtyDot = true;
                dirtyDotX0 = pen + 6.0F * contentScale;
                dirtyDotX1 = dirtyDotX0 + 4.0F * contentScale;
                dirtyDotY0 = centerY - 2.0F * contentScale;
                dirtyDotY1 = centerY + 2.0F * contentScale;
            }
        }

        if (vertices.size() > ribbonFirst) {
            ribbonRange = makeRange(ribbonFirst, fullScissor);
            hasRibbon = true;
        }
    }

    TextRange statusRange{0, 0, fullScissor};
    bool hasStatus = false;
    {
        const std::size_t statusFirst = vertices.size();
        const float statusCenter = viewportHeight - editorBottom * 0.5F;
        const float baseline = statusCenter + fontCenter;
        const std::array<float, 4> statusColor = toGpuColor(ui::theme::kLineNumber);

        if (!state.statusText.empty()) {
            const std::string statusLabel = state.statusIsError ? "Error: " + std::string(state.statusText) : std::string(state.statusText);
            appendText(vertices, statusLabel, margin, baseline,
                       toGpuColor(state.statusIsError ? ui::theme::kError : ui::theme::kTextMuted));
        }

        float rightEdge = viewportWidth - margin;
        const float positionReveal = std::clamp(state.statusPositionReveal, 0.0F, 1.0F);
        if (!state.statusPosition.empty() && positionReveal > 0.0F) {
            const float positionWidth = measureText(state.statusPosition);
            auto positionColor = statusColor;
            const float fade = std::clamp((positionReveal - 0.15F) / 0.85F, 0.0F, 1.0F);
            positionColor[3] *= fade * fade * (3.0F - 2.0F * fade);
            appendText(vertices, state.statusPosition, rightEdge - positionWidth,
                       baseline + (1.0F - positionReveal) * 4.0F * contentScale,
                       positionColor);
            rightEdge -= (positionWidth + 18.0F * contentScale) * positionReveal;
        }

        const std::string version = "v" + std::string(kVersionString);
        const float versionWidth = measureText(version);
        appendText(vertices, version, rightEdge - versionWidth, baseline, statusColor);

        if (vertices.size() > statusFirst) {
            statusRange = makeRange(statusFirst, fullScissor);
            hasStatus = true;
        }
    }

    TextRange overlayTextRange{0, 0, fullScissor};
    bool hasOverlayText = false;
    {
        const std::size_t overlayTextFirst = vertices.size();
        const float logicalWidth = viewportWidth / contentScale;
        const float logicalHeight = viewportHeight / contentScale;
        const auto scaled = [contentScale](const ui::Rect& source) {
            return ui::Rect{source.x0 * contentScale, source.y0 * contentScale,
                            source.x1 * contentScale, source.y1 * contentScale};
        };
        const auto addRect = [&overlayRects](const ui::Rect& rect, float radius,
                                             float borderWidth, float opacity,
                                             const ui::theme::Color& fill,
                                             const ui::theme::Color& border) {
            overlayRects.push_back(WelcomeRectSpec{
                rect.x0, rect.y0, rect.x1, rect.y1, radius, borderWidth, 0.0F,
                opacity, fill, border});
        };
        const std::array<float, 4> mutedText = toGpuColor(ui::theme::kTextMuted);
        const std::array<float, 4> normalText = toGpuColor(ui::theme::kText);
        const std::array<float, 4> errorText = toGpuColor(ui::theme::kError);

        if (state.visualPreview) {
            const float inset = 24.0F * contentScale;
            const float x0 = inset, y0 = inset, x1 = viewportWidth - inset, y1 = viewportHeight - inset;
            if (x1 > x0 + 120.0F * contentScale && y1 > y0 + 120.0F * contentScale) {
                addRect({x0, y0, x1, y1}, 8.0F * contentScale, contentScale, 0.99F, ui::theme::kSurface, ui::theme::kBorder);
                appendText(vertices, state.shaderPreviewMode ? "Shader preview · GPU-rendered snapshot · Escape to close" : state.webPreviewMode ? "Web preview · snapshot · Escape to close" : state.visualMaskMode ? "Drag candidate · Enter reviews mask · Escape cancels" : "Visual review · preview · Escape to close", x0 + 16.0F * contentScale, y0 + 30.0F * contentScale, normalText);
                const std::array<const modules::VisualImage*, 3> images{&state.visualPreview->baseline, &state.visualPreview->candidate, &state.visualPreview->difference};
                const std::array<const char*, 3> labels{"Baseline", "Candidate", "Difference"};
                const float gap = 12.0F * contentScale, top = y0 + 62.0F * contentScale;
                const float cellWidth = state.webPreviewMode ? x1 - x0 - 2.0F * gap : (x1 - x0 - 4.0F * gap) / 3.0F;
                const float cellHeight = y1 - top - gap;
                for (std::size_t n = 0; n < (state.webPreviewMode ? 1U : images.size()); ++n) {
                    const auto& image = *images[state.webPreviewMode ? 1 : n];
                    if (!image.width || !image.height || image.rgba.size() != image.width * image.height * 4 || cellWidth <= 0 || cellHeight <= 0) continue;
                    const float left = x0 + gap + n * (cellWidth + gap);
                    if (!state.webPreviewMode) appendText(vertices, labels[n], left, top - 10.0F * contentScale, mutedText);
                    const float scale = std::min(cellWidth / image.width, cellHeight / image.height);
                    const float startX = left + (cellWidth - image.width * scale) * 0.5F;
                    const float startY = top + (cellHeight - image.height * scale) * 0.5F;
                    previewQuads.push_back({startX, startY, startX + image.width * scale, startY + image.height * scale, state.webPreviewMode ? 1 : n});
                    if(state.visualMaskMode && state.visualMaskSelected && n==1 && state.visualMaskSourceWidth && state.visualMaskSourceHeight) {
                        const auto fitted=ui::visualReviewPaneRect(logicalWidth,logicalHeight,1,image.width,image.height); ui::VisualPreviewRect overlay;
                        if(ui::visualMaskOverlay(fitted,state.visualMaskSourceWidth,state.visualMaskSourceHeight,{0,0,state.visualMaskSourceWidth,state.visualMaskSourceHeight},state.visualMask,overlay)) {
                            const float ax=static_cast<float>(overlay.x)*contentScale, ay=static_cast<float>(overlay.y)*contentScale, bx=static_cast<float>(overlay.x+overlay.width)*contentScale, by=static_cast<float>(overlay.y+overlay.height)*contentScale, stroke=std::max(2.0F,2.0F*contentScale);
                            const auto edge=[&](float l,float t,float r,float b) { visualMaskRects.push_back(WelcomeRectSpec{l,t,r,b,0,0,0,1,ui::theme::kPrimary,ui::theme::kPrimary}); };
                            edge(ax,ay,bx,ay+stroke);edge(ax,by-stroke,bx,by);edge(ax,ay,ax+stroke,by);edge(bx-stroke,ay,bx,by);
                        }
                    }

                }
            }
        }

        if (state.diagramPreviewVisible && state.diagramWidth > 0.0 && state.diagramHeight > 0.0) {
            const float inset = 24.0F * contentScale;
            const float x0 = inset, y0 = inset, x1 = viewportWidth - inset, y1 = viewportHeight - inset;
            if (x1 > x0 + std::max(80.0F, 32.0F * contentScale + 1.0F) &&
                y1 > y0 + std::max(100.0F, 64.0F * contentScale + 1.0F)) {
                addRect({x0, y0, x1, y1}, 8.0F * contentScale, 1.0F * contentScale, 0.98F, ui::theme::kSurface, ui::theme::kBorder);
                appendText(vertices, "Diagram preview", x0 + 18.0F * contentScale, y0 + 30.0F * contentScale, normalText);
                const std::string hint = "Drag/arrows pan · scroll/+/- zoom · 0 fit · Esc close";
                appendText(vertices, hint, x1 - 18.0F * contentScale - measureText(hint), y0 + 30.0F * contentScale, mutedText);
                const float left = x0 + 16.0F * contentScale, top = y0 + 48.0F * contentScale;
                const float width = x1 - left - 16.0F * contentScale, height = y1 - top - 16.0F * contentScale;
                const auto clipLeft=static_cast<std::int32_t>(std::clamp(left,0.0F,viewportWidth)); const auto clipTop=static_cast<std::int32_t>(std::clamp(top,0.0F,viewportHeight));
                const auto clipRight=static_cast<std::int32_t>(std::clamp(left+width,0.0F,viewportWidth)); const auto clipBottom=static_cast<std::int32_t>(std::clamp(top+height,0.0F,viewportHeight));
                diagramScissor.offset={clipLeft,clipTop}; diagramScissor.extent={static_cast<std::uint32_t>(std::max(0,clipRight-clipLeft)),static_cast<std::uint32_t>(std::max(0,clipBottom-clipTop))};
                const auto zoom=std::isfinite(state.diagramZoom) ? std::clamp(state.diagramZoom,1.0,8.0) : 1.0;
                const auto panX=std::isfinite(state.diagramPanX) ? std::clamp(state.diagramPanX,-0.95,0.95) : 0.0, panY=std::isfinite(state.diagramPanY) ? std::clamp(state.diagramPanY,-0.95,0.95) : 0.0;
                const float factor = std::min(width / static_cast<float>(state.diagramWidth), height / static_cast<float>(state.diagramHeight)) * static_cast<float>(zoom);
                const float originX = left + (width - static_cast<float>(state.diagramWidth) * factor) * 0.5F + static_cast<float>(panX)*width;
                const float originY = top + (height - static_cast<float>(state.diagramHeight) * factor) * 0.5F + static_cast<float>(panY)*height;
                const auto diagramRect=[&](const ui::Rect& rect,float radius,float borderWidth,float opacity,const ui::theme::Color& fill,const ui::theme::Color& border) { diagramRects.push_back(WelcomeRectSpec{rect.x0,rect.y0,rect.x1,rect.y1,radius,borderWidth,0.0F,opacity,fill,border}); };
                const auto px = [originX, factor](double value) { return originX + static_cast<float>(value) * factor; };
                const auto py = [originY, factor](double value) { return originY + static_cast<float>(value) * factor; };
                const float stroke = std::max(1.0F, 1.5F * contentScale);
                for (const auto& edge : state.diagramEdges) {
                    for (std::size_t i = 1; i < edge.points.size(); ++i) {
                        const auto a = edge.points[i - 1], b = edge.points[i];
                        const float ax = px(a.x), ay = py(a.y), bx = px(b.x), by = py(b.y);
                        diagramRect({std::min(ax, bx) - stroke * 0.5F, std::min(ay, by) - stroke * 0.5F, std::max(ax, bx) + stroke * 0.5F, std::max(ay, by) + stroke * 0.5F}, 0.0F, 0.0F, 0.85F, ui::theme::kTextMuted, ui::theme::kTextMuted);
                    }
                    if (edge.points.size() >= 2) {
                        const auto drawArrow = [&](const auto& a, const auto& b) {
                            const bool horizontal = std::abs(b.x - a.x) > std::abs(b.y - a.y);
                            const char* arrow = horizontal ? (b.x > a.x ? ">" : "<") : (b.y > a.y ? "v" : "^");
                            const float dx = horizontal ? (b.x > a.x ? -8.0F : 8.0F) * contentScale : 0.0F;
                            const float dy = horizontal ? 0.0F : (b.y > a.y ? -8.0F : 8.0F) * contentScale;
                            appendText(diagramVertices, arrow, px(b.x) + dx - measureText(arrow) * 0.5F, py(b.y) + dy + fontCenter, normalText);
                        };
                        drawArrow(edge.points[edge.points.size()-2],edge.points.back());
                        if(edge.bidirectional) drawArrow(edge.points[1],edge.points[0]);
                    }
                    if (!edge.label.empty() && edge.points.size() >= 2) {
                        std::size_t longest=1; double length=-1;
                        for (std::size_t i=1;i<edge.points.size();++i) { const auto dx=edge.points[i].x-edge.points[i-1].x, dy=edge.points[i].y-edge.points[i-1].y; const auto candidate=std::hypot(dx,dy); if (candidate>length) { length=candidate; longest=i; } }
                        const auto a=edge.points[longest-1], b=edge.points[longest]; const float x=px((a.x+b.x)*0.5), y=py((a.y+b.y)*0.5);
                        std::string label=edge.label; const float maxWidth=120.0F*contentScale;
                        while (!label.empty() && measureText(label)>maxWidth) label.resize(text::lastUtf8Start(label));
                        if (!label.empty()) { const float labelWidth=measureText(label); diagramRect({x-labelWidth*0.5F-4.0F*contentScale,y-lineHeight*0.5F,x+labelWidth*0.5F+4.0F*contentScale,y+lineHeight*0.5F},2.0F*contentScale,0.0F,0.98F,ui::theme::kSurface,ui::theme::kSurface); appendText(diagramVertices,label,x-labelWidth*0.5F,y+fontCenter,normalText); }
                    }
                }
                for (const auto& node : state.diagramNodes) {
                    const float nx0 = px(node.x), ny0 = py(node.y), nx1 = px(node.x + node.width), ny1 = py(node.y + node.height);
                    float radius = 4.0F * contentScale;
                    if (node.shape == modules::DiagramNodeShape::Rounded) radius = std::min((ny1 - ny0) * 0.2F, (nx1 - nx0) * 0.5F);
                    else if (node.shape == modules::DiagramNodeShape::Stadium) radius = std::min(nx1 - nx0, ny1 - ny0) * 0.5F;
                    if (node.shape == modules::DiagramNodeShape::Diamond) radius = -1.0F;
                    diagramRect({nx0, ny0, nx1, ny1}, radius, 1.0F * contentScale, 1.0F, ui::theme::kBackground, ui::theme::kBorder);
                    std::string label = node.label; const float maxWidth = std::max(0.0F, (nx1 - nx0) * (node.shape == modules::DiagramNodeShape::Diamond ? 0.55F : 1.0F) - 12.0F * contentScale);
                    while (!label.empty() && measureText(label) > maxWidth) label.resize(text::lastUtf8Start(label));
                    if (!label.empty() && ny1 - ny0 >= lineHeight) appendText(diagramVertices, label, (nx0 + nx1 - measureText(label)) * 0.5F, (ny0 + ny1) * 0.5F + fontCenter, normalText);
                }
            }
        }

        if (!state.visualPreview && !state.diagramPreviewVisible && !state.commandVisible && state.settingsErrorVisible &&
            !state.settingsErrorText.empty()) {
            const ui::SettingsErrorLayout layout =
                ui::settingsErrorLayout(logicalWidth, logicalHeight);
            const ui::Rect textRect = scaled(layout.text);
            std::string message = "Settings: ";
            message += state.settingsErrorText;
            const float maxWidth = textRect.x1 - textRect.x0;
            while (!message.empty() && measureText(message) > maxWidth) {
                message.resize(text::lastUtf8Start(message));
            }
            if (!message.empty() && message.back() != '.') {
                message += "...";
            }
            appendText(vertices, message,
                       textRect.x1 - measureText(message),
                       (textRect.y0 + textRect.y1) * 0.5F + fontCenter,
                       errorText);
        }

        if (!state.visualPreview && !state.diagramPreviewVisible && state.diagnosticsVisible) {
            const ui::DiagnosticCardLayout layout =
                state.diagnosticAnchorValid
                    ? ui::diagnosticCardLayoutAt(
                          logicalWidth, logicalHeight, state.diagnosticAnchorX,
                          state.diagnosticLineTop, state.diagnosticLineBottom)
                    : ui::diagnosticCardLayout(logicalWidth, logicalHeight);
            const float diagnosticOffsetX =
                state.diagnosticOffsetX * contentScale;
            const float diagnosticOffsetY =
                state.diagnosticOffsetY * contentScale;
            const auto moveRect = [diagnosticOffsetX, diagnosticOffsetY](
                                      ui::Rect rect) {
                return ui::Rect{rect.x0 + diagnosticOffsetX,
                                rect.y0 + diagnosticOffsetY,
                                rect.x1 + diagnosticOffsetX,
                                rect.y1 + diagnosticOffsetY};
            };
            const ui::Rect card = moveRect(scaled(layout.card));
            const ui::Rect ignore = moveRect(scaled(layout.ignoreButton));

            if (layout.anchored && diagnosticOffsetX == 0.0F &&
                diagnosticOffsetY == 0.0F) {
                const ui::Rect connector = moveRect(scaled(layout.connector));
                addRect(connector, 0.0F, 0.0F, 0.55F, ui::theme::kError,
                        ui::theme::kError);
            }
            addRect(card, 6.0F * contentScale, 1.0F * contentScale, 0.97F,
                    ui::theme::kSurface, ui::theme::kBorder);
            addRect(ui::Rect{card.x0, card.y0 + 8.0F * contentScale,
                             card.x0 + 2.0F * contentScale,
                             card.y1 - 8.0F * contentScale},
                    1.0F * contentScale, 0.0F, 1.0F, ui::theme::kError,
                    ui::theme::kError);
            addRect(ignore, 3.0F * contentScale, 1.0F * contentScale,
                    state.diagnosticIgnoreHovered ? 1.0F : 0.0F,
                    state.diagnosticIgnoreHovered ? ui::theme::kSuggestionSelected
                                                  : ui::theme::kSurface,
                    ui::theme::kBorder);

            const float textLeft = card.x0 + 16.0F * contentScale;
            std::string label = state.diagnosticLabel.empty()
                                    ? std::string("SYNTAX ERROR")
                                    : std::string(state.diagnosticLabel);
            if (!state.diagnosticCode.empty()) {
                label += " [" + std::string(state.diagnosticCode) + "]";
            }
            appendText(vertices, label, textLeft,
                       card.y0 + 23.0F * contentScale + fontCenter, errorText);

            std::string message = "Line " +
                                  std::to_string(state.diagnosticLine + 1) + ":" +
                                  std::to_string(state.diagnosticColumn) + "  ";
            message += state.diagnosticMessage;
            const float maxWidth =
                ignore.x0 - textLeft - 12.0F * contentScale;
            bool truncated = false;
            while (!message.empty() &&
                   measureText(message) > maxWidth) {
                message.resize(text::lastUtf8Start(message));
                truncated = true;
            }
            if (truncated) {
                message += "...";
            }
            appendText(vertices, message, textLeft,
                       card.y0 + 47.0F * contentScale + fontCenter, normalText);
            appendText(vertices, "x",
                       (ignore.x0 + ignore.x1) * 0.5F - measureText("x") * 0.5F,
                       (ignore.y0 + ignore.y1) * 0.5F + fontCenter,
                       state.diagnosticIgnoreHovered ? errorText : mutedText);
        }

        if (vertices.size() > overlayTextFirst) {
            overlayTextRange = makeRange(overlayTextFirst, fullScissor);
            hasOverlayText = true;
        }
    }

    TextRange welcomeRange{0, 0, fullScissor};
    bool hasWelcomeText = false;
#if defined(__APPLE__)
    const bool useCommandGlyph = m_font.glyph(0x2318).hasPixels;
    const bool useShiftGlyph = m_font.glyph(0x21E7).hasPixels;
#else
    const bool useCommandGlyph = m_font.glyph(0x2303).hasPixels;
    const bool useShiftGlyph = false;
#endif
    const float commandTextWidth =
        useCommandGlyph ? measureText(kCommandGlyph) : measureText(kCommandFallback);
#if defined(__APPLE__)
    const float shiftTextWidth =
        useShiftGlyph ? measureText(kShiftGlyph) : measureText("Shift");
#else
    const float shiftTextWidth = measureText("Shift");
#endif

    enum class FileIconKind {
        Generic,
        Code,
        Script,
        Web,
        Style,
        Header,
        Json,
        Markdown,
        Shell,
        Image,
        Config,
        Cmake,
        Git,
        Text,
        Application,
    };

    const auto fileIconKind = [](std::string_view path) {
        while (!path.empty() && (path.back() == '/' || path.back() == '\\')) path.remove_suffix(1);
        const std::size_t slash = path.find_last_of("/\\");
        std::string name(
            path.substr(slash == std::string_view::npos ? 0 : slash + 1));
        for (char& character : name) {
            if (character >= 'A' && character <= 'Z') {
                character = static_cast<char>(character - 'A' + 'a');
            }
        }
        const auto endsWith = [&](std::string_view suffix) {
            return name.size() >= suffix.size() &&
                   std::string_view(name).substr(name.size() - suffix.size()) ==
                       suffix;
        };
        const auto equals = [&](std::string_view value) { return name == value; };

        if (endsWith(".app") || endsWith(".exe") || endsWith(".com") || endsWith(".appimage")) return FileIconKind::Application;

        if (equals("cmakelists.txt") || endsWith(".cmake")) {
            return FileIconKind::Cmake;
        }
        if (equals(".gitignore") || equals(".gitattributes") ||
            equals(".gitmodules") || equals(".gitconfig")) {
            return FileIconKind::Git;
        }
        if (equals(".clang-format") || equals(".clang-tidy") ||
            equals(".editorconfig") || equals(".env") || name.starts_with(".env.")) {
            return FileIconKind::Config;
        }
        if (endsWith(".h") || endsWith(".hh") || endsWith(".hpp") ||
            endsWith(".hxx") || endsWith(".inl") || endsWith(".ipp")) {
            return FileIconKind::Header;
        }
        if (endsWith(".html") || endsWith(".htm") || endsWith(".vue") || endsWith(".svelte")) return FileIconKind::Web;
        if (endsWith(".css") || endsWith(".scss") || endsWith(".sass") || endsWith(".less")) return FileIconKind::Style;
        if (endsWith(".py") || endsWith(".pyw") || endsWith(".js") || endsWith(".jsx") || endsWith(".ts") || endsWith(".tsx") || endsWith(".rb") || endsWith(".lua")) return FileIconKind::Script;
        if (endsWith(".c") || endsWith(".cc") || endsWith(".cpp") ||
            endsWith(".cxx") || endsWith(".m") || endsWith(".mm") ||
            endsWith(".py") || endsWith(".pyw") || endsWith(".js") ||
            endsWith(".jsx") || endsWith(".ts") || endsWith(".tsx") ||
            endsWith(".java") || endsWith(".kt") || endsWith(".kts") ||
            endsWith(".rs") || endsWith(".go") || endsWith(".rb") ||
            endsWith(".php") || endsWith(".cs") || endsWith(".swift") ||
            endsWith(".lua") || endsWith(".sql") || endsWith(".css") ||
            endsWith(".scss") || endsWith(".sass") || endsWith(".less") ||
            endsWith(".html") || endsWith(".htm") || endsWith(".vue") ||
            endsWith(".svelte")) {
            return FileIconKind::Code;
        }
        if (endsWith(".json") || endsWith(".jsonc") || endsWith(".lock")) {
            return FileIconKind::Json;
        }
        if (endsWith(".md") || endsWith(".mdx") || endsWith(".markdown") ||
            endsWith(".rst")) {
            return FileIconKind::Markdown;
        }
        if (endsWith(".sh") || endsWith(".bash") || endsWith(".zsh") ||
            endsWith(".fish") || endsWith(".command") || endsWith(".ps1") ||
            endsWith(".bat") || endsWith(".cmd")) {
            return FileIconKind::Shell;
        }
        if (endsWith(".png") || endsWith(".jpg") || endsWith(".jpeg") ||
            endsWith(".gif") || endsWith(".webp") || endsWith(".bmp") ||
            endsWith(".tif") || endsWith(".tiff") || endsWith(".ico") ||
            endsWith(".svg") || endsWith(".heic") || endsWith(".avif")) {
            return FileIconKind::Image;
        }
        if (endsWith(".yaml") || endsWith(".yml") || endsWith(".toml") ||
            endsWith(".ini") || endsWith(".conf") || endsWith(".cfg") ||
            endsWith(".properties") || endsWith(".plist") ||
            endsWith(".xml")) {
            return FileIconKind::Config;
        }
        if (endsWith(".txt") || endsWith(".log") || endsWith(".csv") ||
            endsWith(".tsv")) {
            return FileIconKind::Text;
        }
        return FileIconKind::Generic;
    };
    const auto fileIconLabel = [](std::string_view path) -> std::string_view {
        const auto slash = path.find_last_of("/\\");
        const auto dot = path.find_last_of('.');
        if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) return {};
        std::string ext(path.substr(dot));
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".c") return "C";
        if (ext == ".cpp" || ext == ".cc" || ext == ".cxx") return "C++";
        if (ext == ".py" || ext == ".pyw") return "PY";
        if (ext == ".js" || ext == ".jsx") return "JS";
        if (ext == ".ts" || ext == ".tsx") return "TS";
        if (ext == ".rs") return "RS";
        if (ext == ".go") return "GO";
        if (ext == ".java") return "JA";
        if (ext == ".kt" || ext == ".kts") return "KT";
        if (ext == ".swift") return "SW";
        if (ext == ".cs") return "C#";
        if (ext == ".php") return "PHP";
        if (ext == ".rb") return "RB";
        if (ext == ".lua") return "LUA";
        if (ext == ".sql") return "SQL";
        return {};
    };

    const ui::theme::Color noFill{0.0F, 0.0F, 0.0F, 0.0F};
    const auto addIconRect = [&](std::vector<WelcomeRectSpec>& target, float x0,
                                 float y0, float x1, float y1, float radius,
                                 float borderWidth, float opacity,
                                 const ui::theme::Color& fill,
                                 const ui::theme::Color& border) {
        target.push_back(WelcomeRectSpec{x0, y0, x1, y1, radius, borderWidth, 0.0F,
                                         opacity, fill, border});
    };
    const auto addFileTypeIcon = [&](std::vector<WelcomeRectSpec>& target, float x,
                                     float y, float width, float height,
                                     float opacity, const ui::theme::Color& fallback,
                                     FileIconKind kind, std::string_view label) {
        ui::theme::Color color = fallback;
        switch (kind) {
        case FileIconKind::Code: case FileIconKind::Header: case FileIconKind::Application: color = ui::theme::kAccent; break;
        case FileIconKind::Script: case FileIconKind::Json: color = ui::theme::kSyntaxType; break;
        case FileIconKind::Web: case FileIconKind::Git: color = ui::theme::kSyntaxNumber; break;
        case FileIconKind::Style: case FileIconKind::Image: color = ui::theme::kSyntaxKeyword; break;
        case FileIconKind::Shell: color = ui::theme::kSyntaxString; break;
        case FileIconKind::Cmake: color = ui::theme::kSyntaxPreprocessor; break;
        case FileIconKind::Config: case FileIconKind::Markdown: case FileIconKind::Text:
        case FileIconKind::Generic: break;
        }
        ui::theme::Color tile = color;
        tile.a *= 0.12F;
        addIconRect(target, x, y, x + width, y + height, height * 0.22F,
                    0.0F, opacity, tile, tile);
        if (!label.empty()) {
            const float labelWidth = measureText(label);
            const float scale = std::min(0.78F, width * 0.82F / std::max(labelWidth, 1.0F));
            const float centerX = x + width * 0.5F, centerY = y + height * 0.5F;
            const auto first = vertices.size();
            auto ink = toGpuColor(color); ink[3] *= opacity;
            appendText(vertices, label, centerX - labelWidth * 0.5F, centerY + fontCenter, ink);
            for (std::size_t i = first; i < vertices.size(); ++i) {
                vertices[i].x = centerX + (vertices[i].x - centerX) * scale;
                vertices[i].y = centerY + (vertices[i].y - centerY) * scale;
            }
            return;
        }
        const float thickness =
            std::max(1.0F * contentScale, height * 0.055F);
        const auto hline = [&](float left, float top, float right) {
            addIconRect(target, x + width * left, y + height * top,
                        x + width * right, y + height * top + thickness, 0.0F,
                        0.0F, opacity, color, color);
        };
        const auto vline = [&](float left, float top, float bottom) {
            addIconRect(target, x + width * left, y + height * top,
                        x + width * left + thickness, y + height * bottom, 0.0F,
                        0.0F, opacity, color, color);
        };
        const auto block = [&](float left, float top, float right,
                               float bottom) {
            addIconRect(target, x + width * left, y + height * top,
                        x + width * right, y + height * bottom, 0.0F, 0.0F,
                        opacity, color, color);
        };
        const auto dotAt = [&](float centerX, float centerY, float diameter) {
            addIconRect(target, x + width * centerX - diameter * 0.5F,
                        y + height * centerY - diameter * 0.5F,
                        x + width * centerX + diameter * 0.5F,
                        y + height * centerY + diameter * 0.5F, diameter * 0.5F,
                        0.0F, opacity, color, color);
        };

        switch (kind) {
        case FileIconKind::Application:
            hline(0.22F, 0.27F, 0.78F); hline(0.22F, 0.70F, 0.78F);
            vline(0.22F, 0.27F, 0.70F); vline(0.72F, 0.27F, 0.70F);
            hline(0.22F, 0.41F, 0.78F); dotAt(0.36F, 0.34F, thickness * 1.4F);
            break;
        case FileIconKind::Generic:
            hline(0.28F, 0.32F, 0.70F);
            hline(0.28F, 0.49F, 0.70F);
            hline(0.28F, 0.66F, 0.54F);
            break;
        case FileIconKind::Code:
            vline(0.24F, 0.30F, 0.70F);
            hline(0.24F, 0.30F, 0.34F);
            hline(0.24F, 0.70F, 0.34F);
            vline(0.74F, 0.30F, 0.70F);
            hline(0.64F, 0.30F, 0.74F);
            hline(0.64F, 0.70F, 0.74F);
            break;
        case FileIconKind::Script:
            hline(0.30F, 0.28F, 0.70F);
            vline(0.26F, 0.28F, 0.49F);
            hline(0.30F, 0.46F, 0.68F);
            vline(0.68F, 0.46F, 0.71F);
            hline(0.26F, 0.68F, 0.71F);
            break;
        case FileIconKind::Web:
            hline(0.25F, 0.30F, 0.75F);
            hline(0.25F, 0.66F, 0.75F);
            vline(0.25F, 0.30F, 0.69F);
            vline(0.72F, 0.30F, 0.69F);
            hline(0.25F, 0.40F, 0.75F);
            dotAt(0.38F, 0.34F, std::max(1.2F * contentScale, height * 0.08F));
            break;
        case FileIconKind::Style:
            hline(0.25F, 0.32F, 0.73F);
            hline(0.25F, 0.50F, 0.62F);
            hline(0.25F, 0.68F, 0.73F);
            dotAt(0.62F, 0.50F, std::max(2.2F * contentScale, height * 0.15F));
            break;
        case FileIconKind::Header:
            vline(0.36F, 0.28F, 0.72F);
            vline(0.62F, 0.28F, 0.72F);
            hline(0.26F, 0.39F, 0.72F);
            hline(0.26F, 0.59F, 0.72F);
            break;
        case FileIconKind::Json:
            vline(0.32F, 0.30F, 0.70F);
            hline(0.32F, 0.30F, 0.42F);
            hline(0.32F, 0.70F, 0.42F);
            hline(0.24F, 0.47F, 0.32F);
            vline(0.68F, 0.30F, 0.70F);
            hline(0.58F, 0.30F, 0.68F);
            hline(0.58F, 0.70F, 0.68F);
            hline(0.68F, 0.47F, 0.76F);
            break;
        case FileIconKind::Markdown:
            vline(0.47F, 0.27F, 0.54F);
            hline(0.36F, 0.54F, 0.64F);
            hline(0.41F, 0.62F, 0.59F);
            hline(0.46F, 0.70F, 0.54F);
            break;
        case FileIconKind::Shell:
            hline(0.25F, 0.33F, 0.42F);
            hline(0.34F, 0.43F, 0.51F);
            hline(0.25F, 0.53F, 0.42F);
            hline(0.52F, 0.67F, 0.76F);
            break;
        case FileIconKind::Image:
            dotAt(0.64F, 0.34F,
                  std::max(2.4F * contentScale, height * 0.16F));
            hline(0.22F, 0.70F, 0.78F);
            hline(0.28F, 0.60F, 0.50F);
            hline(0.36F, 0.50F, 0.46F);
            hline(0.48F, 0.63F, 0.72F);
            break;
        case FileIconKind::Config: {
            hline(0.24F, 0.38F, 0.76F);
            hline(0.24F, 0.60F, 0.76F);
            const float knob =
                std::max(2.6F * contentScale, height * 0.18F);
            const float knobOffset = (thickness * 0.5F) / height;
            dotAt(0.40F, 0.38F + knobOffset, knob);
            dotAt(0.62F, 0.60F + knobOffset, knob);
            break;
        }
        case FileIconKind::Cmake:
            block(0.24F, 0.34F, 0.46F, 0.44F);
            block(0.50F, 0.34F, 0.74F, 0.44F);
            block(0.24F, 0.50F, 0.36F, 0.60F);
            block(0.40F, 0.50F, 0.62F, 0.60F);
            block(0.66F, 0.50F, 0.76F, 0.60F);
            break;
        case FileIconKind::Git: {
            const float dot =
                std::max(2.4F * contentScale, height * 0.16F);
            vline(0.32F, 0.30F, 0.70F);
            hline(0.32F, 0.48F, 0.66F);
            dotAt(0.32F, 0.30F, dot);
            dotAt(0.32F, 0.70F, dot);
            dotAt(0.66F, 0.48F, dot);
            break;
        }
        case FileIconKind::Text:
            hline(0.24F, 0.27F, 0.72F);
            hline(0.24F, 0.40F, 0.66F);
            hline(0.24F, 0.53F, 0.70F);
            hline(0.24F, 0.66F, 0.50F);
            break;
        }
    };
    const auto addFolderIcon = [&](std::vector<WelcomeRectSpec>& target, float x,
                                   float y, float width, float height, float opacity,
                                   const ui::theme::Color& color,
                                   const ui::theme::Color& badgeColor) {
        const float tabWidth = width * 0.46F;
        const float tabHeight = height * 0.28F;
        const float borderWidth =
            std::max(1.0F * contentScale, height * 0.05F);
        const float radius = std::max(1.0F * contentScale, height * 0.13F);
        ui::theme::Color bodyFill = color;
        bodyFill.a *= 0.65F;
        addIconRect(target, x, y + tabHeight, x + width, y + height, radius,
                    borderWidth, opacity, bodyFill, color);
        addIconRect(target, x + width * 0.06F, y,
                    x + width * 0.06F + tabWidth, y + tabHeight + borderWidth,
                    radius * 0.75F, 0.0F, opacity, color, color);
        if (badgeColor.a > 0.0F) {
            const float badge = std::max(3.0F * contentScale, height * 0.22F);
            const float badgeX = x + width - badge - width * 0.14F;
            const float badgeY = y + height - badge - height * 0.14F;
            addIconRect(target, badgeX, badgeY, badgeX + badge, badgeY + badge,
                        badge * 0.24F, 0.0F, opacity, badgeColor, badgeColor);
        }
    };
    const auto addProjectIcon = [&](std::vector<WelcomeRectSpec>& target, float x,
                                    float y, float width, float height, float opacity,
                                    const ui::theme::Color& color,
                                    const ui::theme::Color& badgeColor) {
        const float stroke = std::max(1.0F * contentScale, height * 0.055F);
        const float radius = std::max(1.0F * contentScale, height * 0.10F);
        ui::theme::Color fill = color;
        fill.a *= 0.07F;
        addIconRect(target, x + width * 0.08F, y + height * 0.12F,
                    x + width * 0.92F, y + height * 0.92F,
                    radius, stroke, opacity, fill, color);
        ui::theme::Color rail = badgeColor;
        rail.a *= 0.9F;
        addIconRect(target, x + width * 0.23F, y + height * 0.27F,
                    x + width * 0.29F, y + height * 0.75F,
                    stroke * 0.5F, 0.0F, opacity, rail, rail);
        ui::theme::Color text = color;
        text.a *= 0.7F;
        addIconRect(target, x + width * 0.40F, y + height * 0.34F,
                    x + width * 0.76F, y + height * 0.34F + stroke,
                    stroke * 0.5F, 0.0F, opacity, text, text);
        addIconRect(target, x + width * 0.40F, y + height * 0.57F,
                    x + width * 0.65F, y + height * 0.57F + stroke,
                    stroke * 0.5F, 0.0F, opacity, text, text);
    };
    const auto addPlusBadge = [&](std::vector<WelcomeRectSpec>& target, float x,
                                  float y, float size, float opacity,
                                  const ui::theme::Color& color) {
        const float thickness = std::max(1.0F * contentScale, size * 0.16F);
        addIconRect(target, x, y + (size - thickness) * 0.5F, x + size,
                    y + (size + thickness) * 0.5F, thickness * 0.5F, 0.0F,
                    opacity, color, color);
        addIconRect(target, x + (size - thickness) * 0.5F, y,
                    x + (size + thickness) * 0.5F, y + size, thickness * 0.5F,
                    0.0F, opacity, color, color);
    };

    const auto drawWelcomeShortcuts = [&](float top, float centerX,
                                          int columns, float left,
                                          bool alignLeft,
                                          const std::array<float, 4>& textColor) {
        constexpr std::array<WelcomeShortcut, 27> shortcuts{{
            {true, false, "K", "Open file"},
            {true, false, "E", "Switch file"},
            {true, false, ",", "Settings"},
            {true, false, "F", "Find"},
            {true, false, "S", "Save"},
            {true, false, "R", "Run"},
            {true, true, "F", "Search project"},
            {false, false, "F12", "Definition"},
            {true, false, "F12", "Declaration"},
            {false, true, "F12", "References"},
            {false, false, "F2", "Rename symbol"},
            {true, true, "I", "Language status"},
            {false, true, "S", "Signature help", true},
            {false, true, "O", "Project symbols", true},
            {false, false, "F4", "File symbols"},
            {true, true, "Space", "Complete"},
            {true, true, "A", "Ask AI"},
            {true, true, "L", "Clean code"},
            {true, true, "T", "Tasks"},
            {true, true, "G", "Git review"},
            {true, true, "M", "Modules"},
            {true, true, "D", "Debugger"},
            {true, true, "B", "Breakpoint"},
            {true, true, "Y", "Recovery"},
            {true, true, "R", "Functions"},
            {true, false, "W", "Close file"},
            {true, true, "U", "Contrast"},
        }};

        const float keyPad = 6.0F * contentScale;
        const float keyGap = 6.0F * contentScale;
        const float labelGap = keyGap;
        const float chipGap = 20.0F * contentScale;
        const float keyHeight = 18.0F * contentScale;
        const float rowHeight = 24.0F * contentScale;
        const float commandKeyWidth = commandTextWidth + keyPad * 2.0F;
        const float shiftKeyWidth = shiftTextWidth + keyPad * 2.0F;
#if defined(__APPLE__)
        constexpr std::string_view altName = "Option";
#else
        constexpr std::string_view altName = "Alt";
#endif
        const auto keyWidth = [&](std::string_view key) {
            return measureText(key) + keyPad * 2.0F;
        };
        const auto chipWidth = [&](const WelcomeShortcut& shortcut) {
            float width = keyWidth(shortcut.key);
            int keys = 1;
            if (shortcut.command) {
                width += commandKeyWidth;
                ++keys;
            }
            if (shortcut.shift) {
                width += shiftKeyWidth;
                ++keys;
            }
            if (shortcut.alt) { width += keyWidth(altName); ++keys; }
            width += keyGap * static_cast<float>(keys - 1);
            return width + labelGap + measureText(shortcut.label);
        };

        const std::array<float, 4> hintColor =
            toGpuColor(ui::theme::kTextMuted);
        std::array<float, 3> columnWidths{};
        for (std::size_t shortcutIndex = 0; shortcutIndex < shortcuts.size();
             ++shortcutIndex) {
            const std::size_t column = shortcutIndex % static_cast<std::size_t>(columns);
            columnWidths[column] =
                std::max(columnWidths[column], chipWidth(shortcuts[shortcutIndex]));
        }

        float tableWidth = 0.0F;
        for (int column = 0; column < columns; ++column) {
            tableWidth += columnWidths[static_cast<std::size_t>(column)];
        }
        tableWidth += chipGap * static_cast<float>(columns - 1);
        const float tableLeft = alignLeft ? left : centerX - tableWidth * 0.5F;

        const int totalRows = (static_cast<int>(shortcuts.size()) + columns - 1) / columns;
        const float dividerTop = top + 2.0F * rowHeight;
        const float lineY = dividerTop + rowHeight * 0.5F;
        const float middle = tableLeft + tableWidth * 0.5F;
        const float gap = 12.0F * contentScale;
        if (middle - gap > tableLeft)
            addIconRect(welcomeRects, tableLeft, lineY, middle - gap, lineY + contentScale,
                        0.0F, 0.0F, 1.0F, ui::theme::kBorder, ui::theme::kBorder);
        if (middle + gap < tableLeft + tableWidth)
            addIconRect(welcomeRects, middle + gap, lineY, tableLeft + tableWidth, lineY + contentScale,
                        0.0F, 0.0F, 1.0F, ui::theme::kBorder, ui::theme::kBorder);
        const float pixel = 1.5F * contentScale;
        const float unfold = std::clamp(state.welcomeShortcutProgress, 0.0F, 1.0F);
        const float angle = unfold * 3.14159265358979323846F;
        for (int step = 0; step < 3; ++step) {
            const float originalY = (1.0F - static_cast<float>(step)) * pixel;
            for (int side : {-1, 1}) {
                const float originalX = static_cast<float>(side * step) * pixel;
                const float dx = originalX * std::cos(angle) - originalY * std::sin(angle);
                const float dy = originalX * std::sin(angle) + originalY * std::cos(angle);
                addIconRect(welcomeRects, middle + dx - pixel * 0.5F, lineY + dy - pixel * 0.5F,
                            middle + dx + pixel * 0.5F, lineY + dy + pixel * 0.5F,
                            0.0F, 0.0F, 1.0F, ui::theme::kTextMuted, ui::theme::kTextMuted);
            }
        }
        const int visibleRows = std::max(0, static_cast<int>((viewportHeight - dividerTop - rowHeight - 20.0F * contentScale) / rowHeight));
        const int scroll = std::clamp(state.welcomeShortcutScroll, 0, std::max(0, totalRows - 2 - visibleRows));
        if (unfold > 0.01F && visibleRows < totalRows - 2) {
            const std::string_view hint = "Scroll or use arrow keys for more shortcuts";
            auto fadingHint = hintColor; fadingHint[3] *= unfold;
            appendText(vertices, hint, (viewportWidth - measureText(hint)) * 0.5F, viewportHeight - 12.0F * contentScale, fadingHint);
        }
        std::size_t index = 0;
        std::size_t rowIndex = 0;
        while (index < shortcuts.size()) {
            const int rowSize = std::min<int>(
                columns, static_cast<int>(shortcuts.size() - index));
            const int row = static_cast<int>(rowIndex);
            if (row >= 2 && (unfold <= 0.001F || row < 2 + scroll || row >= 2 + scroll + visibleRows)) {
                index += static_cast<std::size_t>(rowSize); ++rowIndex; continue;
            }
            const float rowProgress = row < 2 ? 1.0F : std::clamp(unfold * 1.5F - static_cast<float>(row - 2 - scroll) * 0.08F, 0.0F, 1.0F);
            const float rowOpacity = rowProgress * rowProgress * (3.0F - 2.0F * rowProgress);
            const float rowTop = row < 2 ? top + static_cast<float>(row) * rowHeight : dividerTop + rowHeight + static_cast<float>(row - 2 - scroll) * rowHeight - (1.0F - rowOpacity) * 8.0F * contentScale;
            auto rowTextColor = textColor; rowTextColor[3] *= rowOpacity;
            auto rowHintColor = hintColor; rowHintColor[3] *= rowOpacity;
            const float keyTop = rowTop + (rowHeight - keyHeight) * 0.5F;
            const float baseline = keyTop + keyHeight * 0.5F + fontCenter;
            float x = tableLeft;

            const auto addKeycap = [&](float width, bool glyphAvailable,
                                       std::u32string_view glyph,
                                       std::string_view fallback) {
                welcomeRects.push_back(WelcomeRectSpec{
                    x, keyTop, x + width, keyTop + keyHeight,
                    3.0F * contentScale, 1.0F * contentScale, 0.0F, 0.85F * rowOpacity,
                    ui::theme::kSurface, ui::theme::kBorder});
                const float textWidth =
                    glyphAvailable ? measureText(glyph) : measureText(fallback);
                const float textX = x + (width - textWidth) * 0.5F;
                if (glyphAvailable) {
                    appendText(vertices, glyph, textX, baseline, rowTextColor);
                } else {
                    appendText(vertices, fallback, textX, baseline, rowTextColor);
                }
                x += width + keyGap;
            };

            for (int column = 0; column < rowSize; ++column) {
                const WelcomeShortcut& shortcut =
                    shortcuts[index + static_cast<std::size_t>(column)];
                x = tableLeft;
                for (int previous = 0; previous < column; ++previous)
                    x += columnWidths[static_cast<std::size_t>(previous)] + chipGap;
                if (shortcut.shift) {
                    addKeycap(shiftKeyWidth, useShiftGlyph, kShiftGlyph, "Shift");
                }
                if (shortcut.command) {
                    addKeycap(commandKeyWidth, useCommandGlyph, kCommandGlyph,
                              kCommandFallback);
                }
                if (shortcut.alt) addKeycap(keyWidth(altName), false, {}, altName);
                addKeycap(keyWidth(shortcut.key), false, {}, shortcut.key);
                appendText(vertices, shortcut.label, x, baseline, rowHintColor);
            }

            index += static_cast<std::size_t>(rowSize);
            ++rowIndex;
        }
    };

    const std::size_t welcomeRectStart = welcomeRects.size();
    if (state.showWelcome && !state.hasProject) {
        const ui::WelcomeLayout layout =
            ui::welcomeLayout(viewportWidth, viewportHeight, contentScale);
        constexpr float logoMinX = 18.0F;
        constexpr float logoMaxX = 89.7614F;
        constexpr float logoMinY = 15.0F;
        constexpr float logoArtHeight = 70.0F;
        const float logoUnit = layout.logoSize / logoArtHeight;
        const float logoLeft =
            layout.logoLeft + layout.logoSize * 0.5F -
            (logoMinX + logoMaxX) * 0.5F * logoUnit;
        const float logoTop = layout.logoTop - logoMinY * logoUnit;

        if (layout.showDivider) {
            welcomeRects.push_back(WelcomeRectSpec{
                layout.dividerX0, layout.dividerY0, layout.dividerX1,
                layout.dividerY1, 0.0F, 0.0F, 0.0F, 0.55F,
                ui::theme::kBorder, ui::theme::kBorder});
            welcomeRects.push_back(WelcomeRectSpec{
                layout.dividerX0 - 0.5F * contentScale, layout.dividerY0,
                layout.dividerX1 + 0.5F * contentScale,
                layout.dividerY0 + 24.0F * contentScale, 1.0F * contentScale,
                0.0F, 0.0F, 0.9F, ui::theme::kPrimary, ui::theme::kPrimary});
        }

        const auto addLogoRect = [&](float x, float y, float width, float height,
                                     const ui::theme::Color& color) {
            welcomeRects.push_back(WelcomeRectSpec{
                logoLeft + x * logoUnit, logoTop + y * logoUnit,
                logoLeft + (x + width) * logoUnit,
                logoTop + (y + height) * logoUnit, 0.0F, 0.0F, 0.0F, 1.0F, color,
                color});
        };

        addLogoRect(18.0F, 15.0F, 15.0F, 70.0F, ui::theme::kLogoAccent);
        addLogoRect(45.0682F, 15.1818F, 14.7613F, 69.8182F,
                    ui::theme::kLogoForeground);
        addLogoRect(45.0682F, 72.8295F, 44.6932F, 12.1705F,
                    ui::theme::kLogoForeground);

        const bool welcomeHovered = state.welcomeActionHovered;
        const std::array<float, 4> actionColor =
            toGpuColor(welcomeHovered ? ui::theme::kPrimary : ui::theme::kText);
        const ui::theme::Color keyFill =
            welcomeHovered ? ui::theme::kSurface : ui::theme::kBackground;
        const ui::theme::Color keyBorder =
            welcomeHovered ? ui::theme::kPrimary : ui::theme::kBorder;
        const ui::theme::Color actionFill =
            welcomeHovered ? ui::theme::kSuggestionSelected : ui::theme::kSurface;
        const ui::theme::Color actionBorder =
            welcomeHovered ? ui::theme::kPrimary : ui::theme::kBorder;

        const std::size_t welcomeFirst = vertices.size();

        welcomeRects.push_back(WelcomeRectSpec{
            layout.actionX0, layout.actionY0, layout.actionX1, layout.actionY1,
            9.0F * contentScale, 1.0F * contentScale, 0.0F,
            welcomeHovered ? 0.98F : 0.72F, actionFill, actionBorder});

        const float keyPad = 8.0F * contentScale;
        const float keyGap = 6.0F * contentScale;
        const float labelGap = 14.0F * contentScale;
        const float actionPad = 16.0F * contentScale;
        const float keyTop =
            layout.actionY0 +
            (layout.actionY1 - layout.actionY0 - layout.keyHeight) * 0.5F;
        const float keyBaseline = keyTop + layout.keyHeight * 0.5F + fontCenter;
        const float commandWidth = commandTextWidth + keyPad * 2.0F;
        const float letterWidth = measureText("P") + keyPad * 2.0F;
        const std::string_view label = "Open a project";
        const float labelWidth = measureText(label);
        const float totalWidth =
            commandWidth + keyGap + letterWidth + labelGap + labelWidth;
        const float rowStart = layout.actionX0 + actionPad;
        const float arrowWidth = measureText(">");
        const bool showArrow =
            layout.actionX1 - layout.actionX0 >= totalWidth + actionPad * 2.0F +
                                                  arrowWidth + 24.0F * contentScale;

        const auto addKey = [&](float x, float width, std::string_view text) {
            welcomeRects.push_back(WelcomeRectSpec{
                x, keyTop, x + width, keyTop + layout.keyHeight,
                3.0F * contentScale, 1.0F * contentScale, 0.0F, 0.92F,
                keyFill, keyBorder});
            const float textWidth = measureText(text);
            appendText(vertices, text, x + (width - textWidth) * 0.5F, keyBaseline,
                       actionColor);
        };

        welcomeRects.push_back(WelcomeRectSpec{
            rowStart, keyTop, rowStart + commandWidth, keyTop + layout.keyHeight,
            3.0F * contentScale, 1.0F * contentScale, 0.0F, 0.92F, keyFill,
            keyBorder});
        if (useCommandGlyph) {
            appendText(vertices, kCommandGlyph,
                       rowStart + (commandWidth - commandTextWidth) * 0.5F,
                       keyBaseline, actionColor);
        } else {
            appendText(vertices, kCommandFallback,
                       rowStart + (commandWidth - commandTextWidth) * 0.5F,
                       keyBaseline, actionColor);
        }
        addKey(rowStart + commandWidth + keyGap, letterWidth, "P");
        appendText(vertices,
                   label, rowStart + commandWidth + keyGap + letterWidth + labelGap,
                   keyBaseline, actionColor);

        if (showArrow) {
            appendText(vertices, ">", layout.actionX1 - actionPad - arrowWidth,
                       keyBaseline,
                       toGpuColor(welcomeHovered ? ui::theme::kPrimary
                                                 : ui::theme::kTextMuted));
        }

        drawWelcomeShortcuts(layout.hintTop, layout.centerX, layout.hintColumns,
                             layout.hintLeft, layout.hintAlignLeft,
                             toGpuColor(ui::theme::kTextMuted));

        if (vertices.size() > welcomeFirst) {
            welcomeRange = makeRange(welcomeFirst, fullScissor);
            hasWelcomeText = true;
        }
    } else if (state.hasProject && !state.hasDocument && !state.commandVisible) {
        const std::size_t emptyFirst = vertices.size();

        const auto emptyLayout = ui::welcomeLayout(viewportWidth, viewportHeight, contentScale);
        const float blockHeight = lineHeight * 3.0F + 3.0F * 24.0F * contentScale;
        const float centerY = std::max(lineHeight * 2.0F, (viewportHeight - blockHeight) * 0.5F + lineHeight * 2.0F);
        const std::array<float, 4> titleColor =
            toGpuColor(ui::theme::kTextMuted);
        const std::array<float, 4> projectColor = toGpuColor(ui::theme::kText);
        constexpr float projectScale = 1.5F;

        if (!state.projectName.empty()) {
            const float projectCenterY = centerY - lineHeight * 1.45F;
            const float iconWidth = 30.0F * contentScale;
            const float iconHeight = 24.0F * contentScale;
            const float iconGap = 13.0F * contentScale;
            const float maxTextWidth = std::max(
                40.0F * contentScale,
                (viewportWidth - 64.0F * contentScale - iconWidth - iconGap) /
                    projectScale);
            std::string projectName(state.projectName);
            bool truncated = false;
            while (!projectName.empty() &&
                   measureText(projectName) > maxTextWidth) {
                projectName.resize(text::lastUtf8Start(projectName));
                truncated = true;
            }
            if (truncated) {
                projectName += "...";
            }
            const float projectTextWidth =
                measureText(projectName) * projectScale;
            const float groupWidth = iconWidth + iconGap + projectTextWidth;
            const float groupLeft = (viewportWidth - groupWidth) * 0.5F;
            const float iconTop = projectCenterY - iconHeight * 0.5F;

            ui::theme::Color folderColor = ui::theme::kTextMuted;
            folderColor.a = 0.72F;
            ui::theme::Color badgeColor = ui::theme::kPrimary;
            badgeColor.a = 0.95F;
            addProjectIcon(welcomeRects, groupLeft, iconTop, iconWidth, iconHeight,
                           1.0F, folderColor, badgeColor);

            const std::size_t projectTextFirst = vertices.size();
            appendText(vertices, projectName,
                       -measureText(projectName) * 0.5F, 0.0F, projectColor);
            const float projectCenterX =
                groupLeft + iconWidth + iconGap + projectTextWidth * 0.5F;
            for (std::size_t index = projectTextFirst; index < vertices.size();
                 ++index) {
                vertices[index].x =
                    projectCenterX + vertices[index].x * projectScale;
                vertices[index].y = projectCenterY + fontCenter * projectScale +
                                    vertices[index].y * projectScale;
            }
        }

        const std::string title = "No file open";
        const float titleWidth = measureText(title);
        appendText(vertices, title, (viewportWidth - titleWidth) * 0.5F,
                   centerY + fontCenter, titleColor);
        drawWelcomeShortcuts(centerY + lineHeight * 1.15F, viewportWidth * 0.5F,
                             emptyLayout.hintColumns, 0.0F, false,
                             toGpuColor(ui::theme::kTextMuted));
        if (vertices.size() > emptyFirst) {
            welcomeRange = makeRange(emptyFirst, fullScissor);
            hasWelcomeText = true;
        }
    }

    // Animate only the welcome content; editor and overlay controls remain sharp.
    const float startup = std::clamp(state.startupProgress, 0.0F, 1.0F);
    if (hasWelcomeText && startup < 1.0F) {
        const float scale = 0.97F + 0.03F * startup;
        const float offsetY = (1.0F - startup) * 12.0F * contentScale;
        const float centerX = viewportWidth * 0.5F, centerY = viewportHeight * 0.5F;
        for (std::size_t i = welcomeRange.firstVertex; i < welcomeRange.firstVertex + welcomeRange.vertexCount; ++i) {
            vertices[i].x = centerX + (vertices[i].x - centerX) * scale;
            vertices[i].y = centerY + (vertices[i].y - centerY) * scale + offsetY;
        }
        for (std::size_t i = welcomeRectStart; i < welcomeRects.size(); ++i) {
            auto& rect = welcomeRects[i];
            rect.x0 = centerX + (rect.x0 - centerX) * scale;
            rect.x1 = centerX + (rect.x1 - centerX) * scale;
            rect.y0 = centerY + (rect.y0 - centerY) * scale + offsetY;
            rect.y1 = centerY + (rect.y1 - centerY) * scale + offsetY;
            rect.opacity *= startup;
            rect.blur = std::max(rect.blur, (1.0F - startup) * 3.0F * contentScale);
        }
    }

    float barX0 = 0.0F;
    float barY1 = 0.0F;
    float barBlur = 0.0F;
    float barOpacity = std::clamp(state.opacity, 0.0F, 1.0F);

    TextRange commandTextRange{0, 0, fullScissor};
    bool hasCommandText = false;
    TextRange commandStatusRange{0, 0, fullScissor};
    bool hasCommandStatus = false;
    std::array<TextRange, static_cast<std::size_t>(ui::theme::kMaxSuggestions)>
        suggestionRanges{};
    std::array<float, static_cast<std::size_t>(ui::theme::kMaxSuggestions)>
        suggestionOpacities{};
    std::size_t suggestionRangeCount = 0;
    bool hasSuggestionSelection = false;
    float suggestionSelectionX0 = 0.0F;
    float suggestionSelectionY0 = 0.0F;
    float suggestionSelectionX1 = 0.0F;
    float suggestionSelectionY1 = 0.0F;
    float suggestionSelectionRadius = 0.0F;
    float suggestionSelectionOpacity = 0.0F;
    bool hasSuggestionRail = false;
    float suggestionRailX0 = 0.0F;
    float suggestionRailY0 = 0.0F;
    float suggestionRailX1 = 0.0F;
    float suggestionRailY1 = 0.0F;
    bool hasCommandCaret = false;
    bool hasCommandSelection = false;
    float commandSelectionX0 = 0.0F, commandSelectionX1 = 0.0F;
    float commandSelectionY0 = 0.0F, commandSelectionY1 = 0.0F;
    float commandCaretX0 = 0.0F;
    float commandCaretX1 = 0.0F;
    float commandCaretY0 = 0.0F;
    float commandCaretY1 = 0.0F;

    if (state.commandVisible) {
        const float availableWidth = std::max(0.0F, viewportWidth - margin * 2.0F);
        const float barWidth =
            std::min(ui::theme::kSearchBarWidth * contentScale, availableWidth);
        const float barHeight = ui::theme::kSearchBarHeight * contentScale;

        if (barWidth > 0.0F && barHeight > 0.0F) {
            const float baseX0 = margin;
            const float baseY0 = std::max(
                margin,
                ui::commandBarTop(viewportHeight / contentScale) * contentScale);
            const float baseY1 = baseY0 + barHeight;
            const float offsetX = state.offsetX * contentScale;
            const float scaleX = state.scaleX;
            const float scaleY = state.scaleY;
            const float pivotX = baseX0;
            const float pivotY = baseY0 + barHeight * 0.5F;

            const auto transformX = [pivotX, scaleX, offsetX](float x) {
                return pivotX + (x - pivotX) * scaleX + offsetX;
            };
            const auto transformY = [pivotY, scaleY](float y) {
                return pivotY + (y - pivotY) * scaleY;
            };

            barX0 = transformX(baseX0);
            barY1 = transformY(baseY1);
            barBlur = state.blur * contentScale;

            const float padding = ui::theme::kSearchBarPadding * contentScale;
            float textX = baseX0 + padding;
            const float centerY = baseY0 + barHeight * 0.5F;
            const float baseline = centerY + fontCenter;
            const bool empty = state.commandQuery.empty();
            const std::string_view text =
                empty ? (!state.commandPlaceholder.empty() ? state.commandPlaceholder :
                         state.commandKind == ui::PromptKind::Project
                             ? std::string_view("Open or create project...")
                             : state.commandKind == ui::PromptKind::OpenFiles
                                   ? std::string_view("Switch open file...")
                             : state.commandKind == ui::PromptKind::Replace
                                   ? std::string_view("Replace selection with... (empty deletes)")
                             : state.commandKind == ui::PromptKind::Find
                                   ? std::string_view("Find in file...")
                             : state.commandKind == ui::PromptKind::Ai
                                   ? std::string_view("Ask local AI about the selection...")
                             : state.commandKind == ui::PromptKind::Function
                                   ? std::string_view("Search project functions...")
                                   : std::string_view("Open or create file..."))
                      : state.commandQuery;
            const std::array<float, 4> accentColor = toGpuColor(ui::theme::kAccent);
            const std::array<float, 4> mutedColor = toGpuColor(ui::theme::kTextMuted);
            const std::array<float, 4> primaryColor = toGpuColor(ui::theme::kText);
            const std::array<float, 4> textColor =
                empty ? mutedColor : primaryColor;

            const float textRight = baseX0 + std::max(barWidth, std::min(viewportWidth - margin * 2.0F,
                                                                       520.0F * contentScale));
            const auto cursor = std::min(state.commandCursor, state.commandQuery.size());
            const float cursorWidth = measureText(state.commandQuery.substr(0, cursor));
            if (!empty) textX -= std::max(0.0F, textX + cursorWidth + 4.0F * contentScale - textRight);
            const auto selectionStart = std::min(state.commandSelectionStart, state.commandQuery.size());
            const auto selectionEnd = std::min(state.commandSelectionEnd, state.commandQuery.size());
            if (selectionStart < selectionEnd) {
                const float left = std::max(baseX0, textX + measureText(state.commandQuery.substr(0, selectionStart)));
                const float right = std::min(textRight, textX + measureText(state.commandQuery.substr(0, selectionEnd)));
                if (right > left) {
                    hasCommandSelection = true;
                    commandSelectionX0 = transformX(left); commandSelectionX1 = transformX(right);
                    commandSelectionY0 = transformY(centerY - caretHeight * 0.55F);
                    commandSelectionY1 = transformY(centerY + caretHeight * 0.55F);
                }
            }

            const std::size_t textFirst = vertices.size();
            appendText(vertices, text, textX, baseline, textColor);
            for (std::size_t index = textFirst; index < vertices.size(); ++index) {
                vertices[index].x = transformX(vertices[index].x);
                vertices[index].y = transformY(vertices[index].y);
            }

            const auto makeScissorRect = [&](float x0, float y0, float x1, float y1) {
                const int maxX = static_cast<int>(m_extent.width);
                const int maxY = static_cast<int>(m_extent.height);
                const int left = std::clamp(static_cast<int>(std::floor(x0)), 0, maxX);
                const int top = std::clamp(static_cast<int>(std::floor(y0)), 0, maxY);
                const int right = std::clamp(static_cast<int>(std::ceil(x1)), 0, maxX);
                const int bottom = std::clamp(static_cast<int>(std::ceil(y1)), 0, maxY);
                VkRect2D scissor{};
                scissor.offset = {left, top};
                scissor.extent = {static_cast<std::uint32_t>(std::max(0, right - left)),
                                  static_cast<std::uint32_t>(std::max(0, bottom - top))};
                return scissor;
            };

            const float textScissorX1 = transformX(
                baseX0 + std::max(barWidth, std::min(viewportWidth - margin * 2.0F,
                                                     520.0F * contentScale)));
            const VkRect2D textScissor =
                makeScissorRect(transformX(baseX0), transformY(baseY0),
                                textScissorX1, transformY(baseY1));
            if (textScissor.extent.width > 0 && textScissor.extent.height > 0 &&
                vertices.size() > textFirst) {
                commandTextRange = makeRange(textFirst, textScissor);
                hasCommandText = true;
            }

            if (!empty && std::fmod(state.timeSeconds, 1.06) < 0.53) {
                const float caretWidth = std::max(1.0F, 1.5F * contentScale);
                const float caretBaseX = textX + cursorWidth + 2.0F * contentScale;
                const float caretTop = centerY - caretHeight * 0.5F;

                hasCommandCaret = true;
                commandCaretX0 = transformX(caretBaseX);
                commandCaretX1 = transformX(caretBaseX + caretWidth);
                commandCaretY0 = transformY(caretTop);
                commandCaretY1 = transformY(caretTop + caretHeight);
            }

            const float statusLeft = barX0 + 12.0F * contentScale;
            float suggestionTop = barY1 + ui::commandSuggestionGap(viewportHeight / contentScale) * contentScale;
            const float suggestionWidth = std::max(
                barWidth, std::min(viewportWidth - margin * 2.0F, 520.0F * contentScale));
            const float suggestionX0 = barX0;
            const float suggestionX1 = barX0 + suggestionWidth * scaleX;
            const float suggestionInset = ui::theme::kSuggestionInset * contentScale;
            const float pathPadding = ui::theme::kSuggestionPaddingX * contentScale;

            const auto easeOutCubic = [](float value) {
                const float t = 1.0F - std::clamp(value, 0.0F, 1.0F);
                return 1.0F - t * t * t;
            };
            const float revealAge = static_cast<float>(
                std::max(0.0, state.timeSeconds - state.suggestionEpoch)) * speed;
            const auto rowReveal = [revealAge, &easeOutCubic](std::size_t index) {
                const float local =
                    (revealAge - static_cast<float>(index) * 0.016F) / 0.14F;
                return easeOutCubic(local);
            };

            if (state.commandStatusRows > 0) {
                const std::size_t commandStatusFirst = vertices.size();
                const std::string statusLabel = state.commandStatusIsError ? "Error: " + std::string(state.commandStatus) : std::string(state.commandStatus);
                const auto statusLines = text::wrapLines(statusLabel, state.commandStatusColumns);
                for (std::size_t row = 0; row < std::min(state.commandStatusRows, statusLines.size()); ++row) {
                    std::string display = statusLines[row];
                    if (row + 1 == state.commandStatusRows && statusLines.size() > state.commandStatusRows) {
                        const auto dots = std::min<std::size_t>(3, state.commandStatusColumns);
                        const auto prefix = text::wrapLines(display, std::max<std::size_t>(1, state.commandStatusColumns - dots));
                        display = (state.commandStatusColumns > dots && !prefix.empty() ? prefix.front() : "") + std::string(dots, '.');
                    }
                    appendText(vertices, display, statusLeft,
                               suggestionTop + static_cast<float>(row) * suggestionLineHeight + lineHeight * 0.5F + fontCenter,
                               toGpuColor(state.commandStatusIsError ? ui::theme::kError : ui::theme::kTextMuted));
                }
                const VkRect2D statusScissor = makeScissorRect(
                    suggestionX0, suggestionTop, suggestionX1,
                    suggestionTop + static_cast<float>(state.commandStatusRows) * suggestionLineHeight);
                if (statusScissor.extent.width > 0 && statusScissor.extent.height > 0 &&
                    vertices.size() > commandStatusFirst) {
                    commandStatusRange = makeRange(commandStatusFirst, statusScissor);
                    hasCommandStatus = true;
                }
                suggestionTop += static_cast<float>(state.commandStatusRows) * suggestionLineHeight;
            }

            const std::size_t count =
                std::min(state.suggestions.size(),
                         static_cast<std::size_t>(ui::theme::kMaxSuggestions));
            if (count > 0) {
                const VkRect2D suggestionScissor = makeScissorRect(
                    suggestionX0, suggestionTop, suggestionX1,
                    suggestionTop + static_cast<float>(count) * suggestionLineHeight +
                        4.0F * contentScale);

                const float selectionOpacity =
                    std::clamp(state.suggestionSelectionOpacity, 0.0F, 1.0F) * barOpacity;
                if (selectionOpacity > 0.01F && state.selectedSuggestion >= 0 &&
                    static_cast<std::size_t>(state.selectedSuggestion) < count &&
                    state.suggestions[static_cast<std::size_t>(state.selectedSuggestion)].kind != ui::SuggestionKind::ReadOnlyText &&
                    state.suggestions[static_cast<std::size_t>(state.selectedSuggestion)].kind != ui::SuggestionKind::MarkdownHeading &&
                    state.suggestions[static_cast<std::size_t>(state.selectedSuggestion)].kind != ui::SuggestionKind::MarkdownCode &&
                    state.suggestionSelectionRow >= -0.5F) {
                    const float selectedTop =
                        suggestionTop + state.suggestionSelectionRow * suggestionLineHeight;
                    const float rowX0 = std::clamp(suggestionX0 + suggestionInset, 0.0F,
                                                   viewportWidth);
                    const float rowX1 = std::clamp(suggestionX1 - suggestionInset, 0.0F,
                                                   viewportWidth);
                    const float rowY0 = selectedTop + 1.0F * contentScale;
                    const float rowY1 =
                        selectedTop + suggestionLineHeight - 1.0F * contentScale;
                    if (rowX1 > rowX0 && rowY1 > rowY0) {
                        hasSuggestionSelection = true;
                        suggestionSelectionX0 = rowX0;
                        suggestionSelectionY0 = rowY0;
                        suggestionSelectionX1 = rowX1;
                        suggestionSelectionY1 = rowY1;
                        suggestionSelectionRadius =
                            ui::theme::kSuggestionRadius * contentScale;
                        suggestionSelectionOpacity = selectionOpacity;

                        const float railWidth =
                            ui::theme::kSuggestionRailWidth * contentScale;
                        suggestionRailX0 = rowX0 + 3.0F * contentScale;
                        suggestionRailX1 = suggestionRailX0 + railWidth;
                        suggestionRailY0 = rowY0 + 3.0F * contentScale;
                        suggestionRailY1 = rowY1 - 3.0F * contentScale;
                        hasSuggestionRail = suggestionRailY1 > suggestionRailY0;
                    }
                }

                for (std::size_t index = 0; index < count; ++index) {
                    const ui::PathSuggestion& suggestion = state.suggestions[index];
                    const bool selected =
                        static_cast<int>(index) == state.selectedSuggestion;
                    const float reveal = rowReveal(index);
                    const float edgeAbove = index == 0 ? std::clamp(state.suggestionMoreAbove, 0.0F, 1.0F) : 0.0F;
                    const float edgeBelow = index + 1 == count ? std::clamp(state.suggestionMoreBelow, 0.0F, 1.0F) : 0.0F;
                    const float rowAlpha =
                        barOpacity * std::clamp(reveal, 0.0F, 1.0F);
                    const float rowTop = suggestionTop +
                                         static_cast<float>(index) * suggestionLineHeight +
                                         (1.0F - reveal) * 5.0F * contentScale;
                    const float suggestionBaseline =
                        rowTop + suggestionLineHeight * 0.5F + fontCenter;
                    const float pathX = suggestionX0 + suggestionInset + pathPadding;
                    const float iconWidth = 14.0F * contentScale;
                    const float iconHeight = 15.0F * contentScale;
                    const float iconTop =
                        rowTop + suggestionLineHeight * 0.5F - iconHeight * 0.5F;
                    const float iconX = pathX;
                    const float suggestionTextX =
                        iconX + iconWidth + 8.0F * contentScale;
                    const std::size_t rowFirst = vertices.size();
                    const std::size_t iconFirst = suggestionIcons.size();

                    if (suggestion.kind == ui::SuggestionKind::ReadOnlyText || suggestion.kind == ui::SuggestionKind::MarkdownHeading || suggestion.kind == ui::SuggestionKind::MarkdownCode) {
                        appendText(vertices, suggestion.path, pathX, suggestionBaseline, suggestion.kind == ui::SuggestionKind::MarkdownHeading ? accentColor : suggestion.kind == ui::SuggestionKind::MarkdownCode ? mutedColor : primaryColor);
                    } else if (suggestion.kind == ui::SuggestionKind::Function ||
                        suggestion.kind == ui::SuggestionKind::Task) {
                        float pen = pathX;
                        if (suggestion.kind == ui::SuggestionKind::Function)
                            pen = appendText(vertices, "@", pathX, suggestionBaseline,
                                             selected ? accentColor : mutedColor);
                        const float right = suggestionX1 - suggestionInset - pathPadding;
                        const float available = std::max(0.0F, right - pen);
                        const auto fit = [this](std::string_view text, float width) {
                            if (measureText(text) <= width) return std::string(text);
                            constexpr std::string_view suffix = "...";
                            const float suffixWidth = measureText(suffix);
                            if (suffixWidth > width) return std::string{};
                            const auto boundary = [text](std::size_t offset) {
                                while (offset > 0 && offset < text.size() &&
                                       (static_cast<unsigned char>(text[offset]) & 0xc0U) == 0x80U) --offset;
                                return offset;
                            };
                            std::size_t low = 0, high = text.size();
                            while (low < high) {
                                const auto middle = low + (high - low + 1) / 2;
                                if (measureText(text.substr(0, boundary(middle))) + suffixWidth <= width) low = middle;
                                else high = middle - 1;
                            }
                            return std::string(text.substr(0, boundary(low))) + std::string(suffix);
                        };
                        const float gap = 12.0F * contentScale;
                        std::string detail;
                        if (!suggestion.detail.empty() && available > 100.0F * contentScale)
                            detail = fit(suggestion.detail, available * 0.42F);
                        const float detailWidth = measureText(detail);
                        const float labelWidth = std::max(0.0F, available - (detail.empty() ? 0.0F : detailWidth + gap));
                        const std::string label = fit(suggestion.path, labelWidth);
                        appendText(vertices, label, pen, suggestionBaseline,
                                   selected ? accentColor : primaryColor);
                        if (!detail.empty())
                            appendText(vertices, detail, right - detailWidth,
                                       suggestionBaseline, mutedColor);
                    } else if (suggestion.kind == ui::SuggestionKind::CreateFile ||
                        suggestion.kind == ui::SuggestionKind::CreateDirectory) {
                        const bool directory =
                            suggestion.kind == ui::SuggestionKind::CreateDirectory;
                        if (directory) {
                            addFolderIcon(suggestionIcons, iconX, iconTop, iconWidth,
                                          iconHeight, rowAlpha, ui::theme::kAccent,
                                          noFill);
                        } else {
                            addFileTypeIcon(suggestionIcons, iconX, iconTop, iconWidth,
                                            iconHeight, rowAlpha, ui::theme::kAccent,
                                            fileIconKind(suggestion.path), fileIconLabel(suggestion.path));
                        }
                        addPlusBadge(
                            suggestionIcons, iconX + iconWidth * 0.60F,
                            iconTop + iconHeight * 0.52F,
                            8.0F * contentScale, rowAlpha, ui::theme::kAccent);
                        float pen = appendText(vertices, suggestion.path,
                                               suggestionTextX, suggestionBaseline,
                                               accentColor);
                        if (directory && (suggestion.path.empty() ||
                                          suggestion.path.back() != '/')) {
                            appendText(vertices, "/", pen, suggestionBaseline, mutedColor);
                        }
                    } else {
                        const bool project =
                            suggestion.kind == ui::SuggestionKind::Project;
                        const bool directory =
                            suggestion.kind == ui::SuggestionKind::Directory || project;
                        if (project) {
                            addProjectIcon(
                                suggestionIcons, iconX, iconTop, iconWidth,
                                iconHeight, rowAlpha,
                                selected ? ui::theme::kAccent : ui::theme::kTextMuted,
                                ui::theme::kAccent);
                        } else if (directory && fileIconKind(suggestion.path) != FileIconKind::Application) {
                            addFolderIcon(
                                suggestionIcons, iconX, iconTop, iconWidth, iconHeight,
                                rowAlpha,
                                selected ? ui::theme::kAccent : ui::theme::kTextMuted,
                                noFill);
                        } else {
                            addFileTypeIcon(
                                suggestionIcons, iconX, iconTop, iconWidth, iconHeight,
                                rowAlpha,
                                selected ? ui::theme::kAccent : ui::theme::kTextMuted,
                                fileIconKind(suggestion.path), fileIconLabel(suggestion.path));
                        }
                        float pen = suggestionTextX;
                        const std::size_t slash = suggestion.path.find_last_of('/');
                        if (slash != std::string_view::npos) {
                            pen = appendText(vertices, suggestion.path.substr(0, slash + 1),
                                             pen, suggestionBaseline, mutedColor);
                        }
                        const std::string_view filename =
                            slash == std::string_view::npos
                                ? suggestion.path
                                : suggestion.path.substr(slash + 1);
                        pen = appendText(vertices, filename, pen, suggestionBaseline,
                                         selected ? accentColor : primaryColor);
                        if (directory && !suggestion.path.empty() &&
                            suggestion.path.back() != '/') {
                            appendText(vertices, "/", pen, suggestionBaseline, mutedColor);
                        }
                    }

                    // The content itself fades toward an edge with more results. This
                    // works over the animated background without an opaque overlay.
                    for (std::size_t v = rowFirst; v < vertices.size(); ++v) {
                        const float y = std::clamp((vertices[v].y - rowTop) / suggestionLineHeight, 0.0F, 1.0F);
                        vertices[v].a *= 1.0F - 0.58F * (edgeAbove * (1.0F - y) + edgeBelow * y);
                    }
                    for (std::size_t icon = iconFirst; icon < suggestionIcons.size(); ++icon) {
                        suggestionIcons[icon].opacity *= 1.0F - 0.22F * std::max(edgeAbove, edgeBelow);
                    }

                    if (rowAlpha > 0.01F && vertices.size() > rowFirst &&
                        suggestionRangeCount < suggestionRanges.size() &&
                        suggestionScissor.extent.width > 0 &&
                        suggestionScissor.extent.height > 0) {
                        suggestionRanges[suggestionRangeCount] =
                            makeRange(rowFirst, suggestionScissor);
                        suggestionOpacities[suggestionRangeCount] = rowAlpha;
                        ++suggestionRangeCount;
                    }
                }
            }
        }
    }

    if (state.compileTerminalVisible && !state.commandVisible &&
        !state.compileTerminalTitle.empty()) {
        const float logicalWidth = viewportWidth / contentScale;
        const float logicalHeight = viewportHeight / contentScale;
        const bool standalone = state.compileTerminalStandalone;
        ui::CompileTerminalLayout layout;
        if (standalone) {
            const float titleHeightLogical =
                ui::theme::kCompileTerminalTitleHeight;
            const float padding = ui::theme::kCompileTerminalPadding;
            const float inputHeight = 24.0F;
            layout.panel =
                ui::Rect{0.0F, 0.0F, logicalWidth, logicalHeight};
            layout.titleBar =
                ui::Rect{0.0F, 0.0F, logicalWidth, titleHeightLogical};
            layout.output =
                ui::Rect{padding, titleHeightLogical, logicalWidth - padding,
                         logicalHeight - padding - inputHeight};
            layout.input = ui::Rect{padding,
                                    logicalHeight - padding - inputHeight,
                                    logicalWidth - padding,
                                    logicalHeight - padding};
        } else {
            const int rows = std::max(
                1, static_cast<int>(std::floor(
                       (logicalHeight - ui::theme::kEditorTop -
                        ui::theme::kEditorBottom) /
                       std::max(1.0F, ui::theme::kEditorLineHeight))));
            const int row = std::clamp(
                state.compileTerminalLine - state.firstLine, 0, rows - 1);
            const float anchorTop =
                ui::theme::kEditorTop +
                static_cast<float>(row) * ui::theme::kEditorLineHeight;
            layout = ui::compileTerminalLayout(logicalWidth, logicalHeight,
                                               anchorTop);
        }

        const float reveal = std::clamp(state.compileTerminalReveal, 0.0F, 1.25F);
        const float scale = 0.96F + 0.04F * reveal;
        const float offsetY = (1.0F - reveal) * 12.0F * contentScale;

        const auto toScreen = [contentScale](const ui::Rect& source) {
            return ui::Rect{source.x0 * contentScale, source.y0 * contentScale,
                            source.x1 * contentScale, source.y1 * contentScale};
        };

        ui::Rect panel = toScreen(layout.panel);
        ui::Rect trafficLights = toScreen(layout.trafficLights);
        ui::Rect close = toScreen(layout.closeButton);
        ui::Rect output = toScreen(layout.output);
        ui::Rect input = toScreen(layout.input);
        const float centerX = (panel.x0 + panel.x1) * 0.5F;
        const float bottomY = panel.y1;
        const auto mapX = [centerX, scale](float x) {
            return centerX + (x - centerX) * scale;
        };
        const auto mapY = [bottomY, scale, offsetY](float y) {
            return bottomY + (y - bottomY) * scale + offsetY;
        };
        const auto mapRect = [&mapX, &mapY](ui::Rect source) {
            return ui::Rect{mapX(source.x0), mapY(source.y0), mapX(source.x1),
                            mapY(source.y1)};
        };
        panel = mapRect(panel);
        trafficLights = mapRect(trafficLights);
        close = mapRect(close);
        output = mapRect(output);
        input = mapRect(input);
        const float terminalOffsetX = state.compileTerminalOffsetX * contentScale;
        const float terminalOffsetY = state.compileTerminalOffsetY * contentScale;
        const auto offsetRect = [terminalOffsetX, terminalOffsetY](ui::Rect rect) {
            return ui::Rect{rect.x0 + terminalOffsetX, rect.y0 + terminalOffsetY,
                            rect.x1 + terminalOffsetX, rect.y1 + terminalOffsetY};
        };
        panel = offsetRect(panel);
        trafficLights = offsetRect(trafficLights);
        close = offsetRect(close);
        output = offsetRect(output);
        input = offsetRect(input);

        const float opacity = std::clamp(state.compileTerminalOpacity, 0.0F, 1.0F);
        const float padding = ui::theme::kCompileTerminalPadding * contentScale;
        const float titleHeight =
            ui::theme::kCompileTerminalTitleHeight * contentScale;
        const float terminalLineHeight = lineHeight;

        const float panelRadius =
            standalone ? 0.0F
                       : ui::theme::kCompileTerminalRadius * contentScale;
        if (!standalone) {
            const ui::theme::Color shadow{0.0F, 0.0F, 0.0F, 1.0F};
            for (int layer = 3; layer >= 1; --layer) {
                const float expand =
                    static_cast<float>(layer) * 5.0F * contentScale;
                terminalRects.push_back(WelcomeRectSpec{
                    panel.x0 - expand, panel.y0 - expand + 3.0F * contentScale,
                    panel.x1 + expand, panel.y1 + expand + 3.0F * contentScale,
                    panelRadius + expand, 0.0F, 12.0F * contentScale,
                    opacity * 0.08F, shadow, shadow});
            }
        }

        terminalRects.push_back(WelcomeRectSpec{
            panel.x0, panel.y0, panel.x1, panel.y1, panelRadius,
            standalone ? 0.0F : 1.0F * contentScale, 0.0F, opacity,
            ui::theme::kSurface,
            standalone
                ? ui::theme::kSurface
                : (state.compileTerminalFocused ? ui::theme::kPrimary
                                                : ui::theme::kBorder)});
        terminalRects.push_back(WelcomeRectSpec{
            panel.x0 + 1.0F * contentScale, panel.y0 + 1.0F * contentScale,
            panel.x1 - 1.0F * contentScale, panel.y0 + titleHeight,
            std::max(0.0F, panelRadius - 1.0F * contentScale), 0.0F, 0.0F,
            opacity * 0.98F, ui::theme::kSuggestionSurface,
            ui::theme::kSuggestionSurface});
        if (!standalone) {
            terminalRects.push_back(WelcomeRectSpec{
                panel.x0 + panelRadius, panel.y0, panel.x1 - panelRadius,
                panel.y0 + 2.0F * contentScale, 1.0F * contentScale, 0.0F,
                0.0F,
                opacity * (state.compileTerminalFocused ? 0.95F : 0.55F),
                ui::theme::kPrimary, ui::theme::kPrimary});
        }
        terminalRects.push_back(WelcomeRectSpec{
            panel.x0 + panelRadius, panel.y0 + titleHeight,
            panel.x1 - panelRadius,
            panel.y0 + titleHeight + 1.0F * contentScale, 0.0F, 0.0F, 0.0F,
            opacity * 0.85F, ui::theme::kBorder, ui::theme::kBorder});
        terminalRects.push_back(WelcomeRectSpec{
            panel.x0 + padding, input.y0 - 6.0F * contentScale,
            panel.x1 - padding, input.y0 - 5.0F * contentScale,
            0.0F, 0.0F, 0.0F, opacity * 0.7F, ui::theme::kBorder,
            ui::theme::kBorder});

        if (!standalone) {
            const float dot = 10.0F * contentScale;
            const float dotGap = 7.0F * contentScale;
            const float dotRadius = dot * 0.5F;
            const float dotY = (trafficLights.y0 + trafficLights.y1) * 0.5F;
            const std::array<ui::theme::Color, 3> lightColors{
                ui::theme::Color{0.98431F, 0.37255F, 0.34118F, 1.0F},
                ui::theme::Color{0.99608F, 0.73725F, 0.18039F, 1.0F},
                ui::theme::Color{0.15686F, 0.78431F, 0.25098F, 1.0F}};
            for (std::size_t index = 0; index < lightColors.size(); ++index) {
                const float dotX = trafficLights.x0 + dotRadius +
                                   static_cast<float>(index) * (dot + dotGap);
                const float lightOpacity =
                    index == 0U && state.compileTerminalCloseHovered
                        ? opacity
                        : opacity * 0.92F;
                terminalRects.push_back(WelcomeRectSpec{
                    dotX - dotRadius, dotY - dotRadius, dotX + dotRadius,
                    dotY + dotRadius, dotRadius, 0.0F, 0.0F, lightOpacity,
                    lightColors[index], lightColors[index]});
            }
            if (state.compileTerminalCloseHovered) {
                terminalRects.push_back(WelcomeRectSpec{
                    close.x0, close.y0, close.x1, close.y1,
                    4.0F * contentScale, 1.0F * contentScale, 0.0F,
                    opacity * 0.35F, ui::theme::kSurface, ui::theme::kError});
            }
        }

        const std::size_t terminalFirst = vertices.size();
        const std::array<float, 4> titleColor =
            toGpuColor(ui::theme::kSyntaxCompileFunction);
        const std::array<float, 4> mutedText = toGpuColor(ui::theme::kTextMuted);
        const std::array<float, 4> normalText = toGpuColor(ui::theme::kText);
        const std::array<float, 4> accentText = toGpuColor(ui::theme::kAccent);
        const ui::theme::Color statusSwatch =
            state.compileTerminalRunning ? ui::theme::kPrimary
                : state.compileTerminalFailed ? ui::theme::kError : ui::theme::kTextMuted;
        const std::array<float, 4> statusColor = toGpuColor(statusSwatch);

        const std::string_view statusText =
            state.compileTerminalRunning ? "running" : state.compileTerminalFailed ? "failed" : "finished";
        float statusX0 = panel.x1 - padding;
        if (!standalone) {
            const float statusWidth =
                measureText(statusText) + 26.0F * contentScale;
            const float statusX1 = panel.x1 - padding;
            statusX0 = statusX1 - statusWidth;
            const float pillHeight = 18.0F * contentScale;
            const float pillY0 =
                panel.y0 + (titleHeight - pillHeight) * 0.5F;
            const float pillY1 = pillY0 + pillHeight;
            terminalRects.push_back(WelcomeRectSpec{
                statusX0, pillY0, statusX1, pillY1, pillHeight * 0.5F,
                1.0F * contentScale, 0.0F, opacity * 0.9F,
                state.compileTerminalRunning ? ui::theme::kSelection
                                             : ui::theme::kBackground,
                state.compileTerminalRunning ? ui::theme::kPrimary
                                             : ui::theme::kBorder});
            const float statusDotRadius = 2.0F * contentScale;
            const float statusDotX = statusX0 + 9.0F * contentScale;
            const float statusDotY = (pillY0 + pillY1) * 0.5F;
            terminalRects.push_back(WelcomeRectSpec{
                statusDotX - statusDotRadius, statusDotY - statusDotRadius,
                statusDotX + statusDotRadius, statusDotY + statusDotRadius,
                statusDotRadius, 0.0F, 0.0F, opacity * 0.95F, statusSwatch,
                statusSwatch});
            appendText(vertices, statusText,
                       statusDotX + 6.0F * contentScale,
                       (pillY0 + pillY1) * 0.5F + fontCenter, statusColor);
        }

        std::string title(state.compileTerminalTitle);
        const float titleMaxWidth =
            standalone
                ? std::max(80.0F * contentScale,
                           viewportWidth - 160.0F * contentScale)
                : statusX0 - (trafficLights.x1 + 12.0F * contentScale) -
                      10.0F * contentScale;
        while (!title.empty() && measureText(title) > titleMaxWidth) {
            title.resize(text::lastUtf8Start(title));
        }
        const float titleWidth = measureText(title);
        const float titleX =
            standalone ? (viewportWidth - titleWidth) * 0.5F
                       : trafficLights.x1 + 12.0F * contentScale;
        appendText(vertices, title, titleX,
                   panel.y0 + titleHeight * 0.5F + fontCenter, titleColor);

        const std::string_view outputText = state.compileTerminalOutput;
        std::vector<std::string_view> outputLines;
        outputLines.reserve(24);
        std::size_t lineStart = 0;
        while (lineStart < outputText.size()) {
            const std::size_t newline = outputText.find('\n', lineStart);
            if (newline == std::string_view::npos) {
                outputLines.push_back(outputText.substr(lineStart));
                break;
            }
            outputLines.push_back(outputText.substr(lineStart, newline - lineStart));
            lineStart = newline + 1;
        }

        const int maxOutputLines =
            std::max(1, static_cast<int>((output.y1 - output.y0) / terminalLineHeight));
        const int firstOutputLine =
            std::max(0, static_cast<int>(outputLines.size()) - maxOutputLines - state.compileOutputScroll);
        const float outputMaxWidth = output.x1 - output.x0;
        float outputBaseline = output.y0 + terminalLineHeight * 0.5F + fontCenter;
        for (int index = firstOutputLine;
             index < std::min(static_cast<int>(outputLines.size()), firstOutputLine + maxOutputLines); ++index) {
            std::string_view line = outputLines[static_cast<std::size_t>(index)];
            const std::size_t lineOffset = static_cast<std::size_t>(line.data() - outputText.data());
            const float tabAdvance = m_font.glyph(U' ').advance * 4.0F;
            const auto fit = text::fitUtf8Prefix(line, outputMaxWidth, [this, tabAdvance](char32_t character) {
                return character == U'\t' ? tabAdvance : m_font.glyph(character).advance;
            });
            line = line.substr(0, fit);
            const std::size_t selectionStart = std::max(state.compileSelectionStart, lineOffset);
            const std::size_t selectionEnd = std::min(state.compileSelectionEnd, lineOffset + line.size());
            if (selectionStart < selectionEnd) {
                const float x0 = output.x0 + measureText(line.substr(0, selectionStart - lineOffset));
                const float x1 = output.x0 + measureText(line.substr(0, selectionEnd - lineOffset));
                const float rowTop = output.y0 + static_cast<float>(index - firstOutputLine) * terminalLineHeight;
                terminalRects.push_back(WelcomeRectSpec{x0, rowTop, std::min(x1, output.x1),
                    std::min(rowTop + terminalLineHeight, output.y1), 0.0F, 0.0F, 0.0F,
                    opacity, ui::theme::kSelection, ui::theme::kSelection});
            }
            appendText(vertices, line, output.x0, outputBaseline, mutedText);
            outputBaseline += terminalLineHeight;
        }

        if (state.compileTerminalRunning) {
            std::string inputLine = "> ";
            inputLine += state.compileTerminalInput;
            if (inputLine.size() > 160) {
                inputLine = inputLine.substr(inputLine.size() - 160);
            }
            const float inputMaxWidth = input.x1 - input.x0;
            while (inputLine.size() > 2 && measureText(inputLine) > inputMaxWidth) {
                std::size_t end = 2;
                (void)text::nextUtf8(inputLine, end);
                inputLine.erase(2, end - 2);
            }
            const float promptWidth = measureText("> ");
            appendText(vertices, "> ", input.x0,
                       (input.y0 + input.y1) * 0.5F + fontCenter, accentText);
            appendText(vertices, inputLine.substr(2), input.x0 + promptWidth,
                       (input.y0 + input.y1) * 0.5F + fontCenter, normalText);

            if (std::fmod(state.timeSeconds, 1.06) < 0.53) {
                const float caretX =
                    input.x0 + measureText(std::string_view(inputLine));
                const float terminalCaretHeight =
                    (m_font.ascent() - m_font.descent()) * 0.9F;
                const float terminalCaretTop =
                    (input.y0 + input.y1) * 0.5F - terminalCaretHeight * 0.5F;
                terminalRects.push_back(WelcomeRectSpec{
                    caretX, terminalCaretTop,
                    caretX + std::max(1.0F, 1.5F * contentScale),
                    terminalCaretTop + terminalCaretHeight, 0.0F, 0.0F, 0.0F,
                    opacity, ui::theme::kCaret, ui::theme::kCaret});
            }
        } else {
            appendText(vertices, state.compileTerminalFailed ? "A command failed. Review the output above." : "Process finished. Click the red light to close.",
                       input.x0, (input.y0 + input.y1) * 0.5F + fontCenter,
                       mutedText);
        }

        if (vertices.size() > terminalFirst) {
            const int maxX = static_cast<int>(m_extent.width);
            const int maxY = static_cast<int>(m_extent.height);
            const float shadowPadding = 24.0F * contentScale;
            const int left = std::clamp(
                static_cast<int>(std::floor(panel.x0 - shadowPadding)), 0, maxX);
            const int top = std::clamp(
                static_cast<int>(std::floor(panel.y0 - shadowPadding)), 0, maxY);
            const int right = std::clamp(
                static_cast<int>(std::ceil(panel.x1 + shadowPadding)), 0, maxX);
            const int bottom = std::clamp(
                static_cast<int>(std::ceil(panel.y1 + shadowPadding)), 0, maxY);
            VkRect2D scissor{};
            scissor.offset = {left, top};
            scissor.extent = {
                static_cast<std::uint32_t>(std::max(0, right - left)),
                static_cast<std::uint32_t>(std::max(0, bottom - top))};
            if (scissor.extent.width > 0 && scissor.extent.height > 0) {
                terminalTextRange = makeRange(terminalFirst, scissor);
                hasTerminalText = true;
            }
        }
    }

    if (state.settingsPanelVisible) {
        const float logicalWidth = viewportWidth / contentScale;
        const float logicalHeight = viewportHeight / contentScale;
        const ui::SettingsPanelLayout layout =
            ui::settingsPanelLayout(logicalWidth, logicalHeight);
        const auto toScreen = [contentScale](const ui::Rect& source) {
            return ui::Rect{source.x0 * contentScale, source.y0 * contentScale,
                            source.x1 * contentScale, source.y1 * contentScale};
        };
        const auto addPanelRect = [&settingsPanelRects](
                                      const ui::Rect& rect, float radius,
                                      float borderWidth, float opacity,
                                      const ui::theme::Color& fill,
                                      const ui::theme::Color& border) {
            settingsPanelRects.push_back(WelcomeRectSpec{
                rect.x0, rect.y0, rect.x1, rect.y1, radius, borderWidth, 0.0F,
                opacity, fill, border});
        };
        const auto centerBaseline = [fontCenter](const ui::Rect& rect) {
            return (rect.y0 + rect.y1) * 0.5F + fontCenter;
        };
        const auto centeredText = [this, &vertices, fontCenter](std::string_view text,
                                                          const ui::Rect& rect,
                                                          const std::array<float, 4>& color) {
            const float width = measureText(text);
            appendText(vertices, text, (rect.x0 + rect.x1 - width) * 0.5F,
                       (rect.y0 + rect.y1) * 0.5F + fontCenter, color);
        };

        const std::array<float, 4> normalText = toGpuColor(ui::theme::kText);
        const std::array<float, 4> mutedText = toGpuColor(ui::theme::kTextMuted);
        const std::array<float, 4> accentText = toGpuColor(ui::theme::kPrimary);
        const std::array<float, 4> doneText =
            ui::theme::luminance(ui::theme::kPrimary) > 0.55F
                ? std::array<float, 4>{0.05F, 0.05F, 0.07F, 1.0F}
                : std::array<float, 4>{1.0F, 1.0F, 1.0F, 1.0F};

        addPanelRect(ui::Rect{0.0F, 0.0F, viewportWidth, viewportHeight}, 0.0F,
                     0.0F, 1.0F, ui::theme::kBackground, ui::theme::kBackground);

        const ui::Rect title = toScreen(layout.title);
        const std::size_t settingsFirst = vertices.size();
        appendText(vertices, "Settings", title.x0,
                   centerBaseline(title), normalText);

        const auto sectionLabel = [&](std::string_view text, const ui::Rect& source) {
            const ui::Rect rect = toScreen(source);
            appendText(vertices, text, rect.x0, centerBaseline(rect), mutedText);
        };
        sectionLabel("THEME", layout.themeLabel);
        sectionLabel("PRIMARY", layout.primaryLabel);
        sectionLabel("ANIMATION", layout.speedLabel);
        sectionLabel("TEXT & UI SIZE", layout.zoomLabel);
        sectionLabel("EDITOR", layout.editorLabel);
        sectionLabel("CURSOR", layout.cursorLabel);

        static constexpr std::array<std::string_view, 3> kThemeNames{
            "Dark", "Graphite", "Light"};
        static constexpr std::array<std::string_view, 3> kThemeKeys{
            "lucid-dark", "graphite", "lucid-light"};
        const std::array<ui::SettingsControl, 3> themeControls{
            ui::SettingsControl::ThemeLucidDark,
            ui::SettingsControl::ThemeGraphite,
            ui::SettingsControl::ThemeLucidLight};
        for (std::size_t index = 0; index < layout.themeChips.size(); ++index) {
            ui::theme::Palette chipPalette{};
            (void)ui::theme::paletteByName(kThemeKeys[index], chipPalette);
            const bool selected = state.settingsTheme == kThemeKeys[index];
            const bool hovered = state.settingsHovered == themeControls[index];
            const ui::Rect chip = toScreen(layout.themeChips[index]);
            addPanelRect(chip, 5.0F * contentScale, 1.0F * contentScale, 0.98F,
                         chipPalette.background,
                         selected ? chipPalette.primary
                                  : (hovered ? chipPalette.primary
                                             : chipPalette.border));
            const float textLuminance = ui::theme::luminance(chipPalette.background);
            const std::array<float, 4> chipText =
                textLuminance > 0.55F
                    ? std::array<float, 4>{0.08F, 0.08F, 0.10F, 1.0F}
                    : std::array<float, 4>{0.96F, 0.96F, 0.98F, 1.0F};
            centeredText(kThemeNames[index], chip, chipText);
            const float accentWidth = 16.0F * contentScale;
            addPanelRect(
                ui::Rect{(chip.x0 + chip.x1 - accentWidth) * 0.5F,
                         chip.y1 - 8.0F * contentScale,
                         (chip.x0 + chip.x1 + accentWidth) * 0.5F,
                         chip.y1 - 5.0F * contentScale},
                1.5F * contentScale, 0.0F, selected ? 1.0F : 0.82F,
                chipPalette.primary, chipPalette.primary);
        }

        static constexpr std::array<ui::SettingsControl, 8> kPrimaryControls{
            ui::SettingsControl::PrimaryColor0, ui::SettingsControl::PrimaryColor1,
            ui::SettingsControl::PrimaryColor2, ui::SettingsControl::PrimaryColor3,
            ui::SettingsControl::PrimaryColor4, ui::SettingsControl::PrimaryColor5,
            ui::SettingsControl::PrimaryColor6, ui::SettingsControl::PrimaryColor7};
        const auto sameColor = [](const ui::theme::Color& left,
                                  const ui::theme::Color& right) {
            return std::abs(left.r - right.r) < 0.01F &&
                   std::abs(left.g - right.g) < 0.01F &&
                   std::abs(left.b - right.b) < 0.01F;
        };
        for (std::size_t index = 0; index < layout.primarySwatches.size(); ++index) {
            const bool selected =
                sameColor(state.settingsPrimary, ui::theme::kPrimarySwatches[index]);
            const bool hovered = state.settingsHovered == kPrimaryControls[index];
            const ui::Rect swatch = toScreen(layout.primarySwatches[index]);
            addPanelRect(swatch, 4.0F * contentScale,
                         (selected ? 2.0F : 1.0F) * contentScale, 0.98F,
                         ui::theme::kPrimarySwatches[index],
                         selected ? ui::theme::kText
                                  : (hovered ? ui::theme::kPrimary
                                             : ui::theme::kBorder));
        }

        static constexpr std::array<std::string_view, 3> kSpeedLabels{"0.5x", "1x",
                                                                      "2x"};
        static constexpr std::array<float, 3> kSpeedValues{0.5F, 1.0F, 2.0F};
        static constexpr std::array<ui::SettingsControl, 3> kSpeedControls{
            ui::SettingsControl::SpeedHalf, ui::SettingsControl::SpeedNormal,
            ui::SettingsControl::SpeedDouble};
        for (std::size_t index = 0; index < layout.speedButtons.size(); ++index) {
            const bool selected =
                std::abs(state.settingsAnimationSpeed - kSpeedValues[index]) < 0.01F;
            const bool hovered = state.settingsHovered == kSpeedControls[index];
            const ui::Rect button = toScreen(layout.speedButtons[index]);
            addPanelRect(button, 4.0F * contentScale, 1.0F * contentScale, 0.98F,
                         selected ? ui::theme::kSelection : ui::theme::kSurface,
                         selected || hovered ? ui::theme::kPrimary
                                             : ui::theme::kBorder);
            centeredText(kSpeedLabels[index], button,
                         selected ? accentText : mutedText);
        }

        static constexpr std::array<std::string_view, 4> kZoomLabels{
            "75%", "100%", "125%", "150%"};
        static constexpr std::array<float, 4> kZoomValues{0.75F, 1.0F, 1.25F,
                                                          1.5F};
        static constexpr std::array<ui::SettingsControl, 4> kZoomControls{
            ui::SettingsControl::Zoom75, ui::SettingsControl::Zoom100,
            ui::SettingsControl::Zoom125, ui::SettingsControl::Zoom150};
        for (std::size_t index = 0; index < layout.zoomButtons.size(); ++index) {
            const bool selected =
                std::abs(state.settingsZoomFactor - kZoomValues[index]) < 0.01F;
            const bool hovered = state.settingsHovered == kZoomControls[index];
            const ui::Rect button = toScreen(layout.zoomButtons[index]);
            addPanelRect(button, 4.0F * contentScale, 1.0F * contentScale, 0.98F,
                         selected ? ui::theme::kSelection : ui::theme::kSurface,
                         selected || hovered ? ui::theme::kPrimary
                                             : ui::theme::kBorder);
            centeredText(kZoomLabels[index], button,
                         selected ? accentText : mutedText);
        }

        const auto toggle = [&](const ui::Rect& source, std::string_view label,
                                bool enabled, ui::SettingsControl control) {
            const ui::Rect button = toScreen(source);
            const bool hovered = state.settingsHovered == control;
            addPanelRect(button, 4.0F * contentScale, 1.0F * contentScale, 0.98F,
                         hovered ? ui::theme::kSuggestionSelected
                                 : ui::theme::kSurface,
                         hovered ? ui::theme::kPrimary : ui::theme::kBorder);
            const float boxSize = 14.0F * contentScale;
            const float boxX = button.x0 + 10.0F * contentScale;
            const float boxY = (button.y0 + button.y1 - boxSize) * 0.5F;
            addPanelRect(ui::Rect{boxX, boxY, boxX + boxSize, boxY + boxSize},
                         3.0F * contentScale, 1.0F * contentScale, 0.98F,
                         enabled ? ui::theme::kPrimary : ui::theme::kSurface,
                         enabled ? ui::theme::kPrimary : ui::theme::kBorder);
            if (enabled) {
                const float inset = 4.0F * contentScale;
                addPanelRect(ui::Rect{boxX + inset, boxY + inset,
                                      boxX + boxSize - inset,
                                      boxY + boxSize - inset},
                             1.0F * contentScale, 0.0F, 0.98F,
                             ui::theme::kSurface, ui::theme::kSurface);
            }
            appendText(vertices, label, boxX + boxSize + 9.0F * contentScale,
                       centerBaseline(button), normalText);
        };
        toggle(layout.syntaxErrors, "Syntax errors", state.settingsShowSyntaxErrors,
               ui::SettingsControl::ShowSyntaxErrors);
        toggle(layout.lineNumbers, "Line numbers", state.settingsShowLineNumbers,
               ui::SettingsControl::ShowLineNumbers);
        toggle(layout.highlightCurrentLine, "Current line",
               state.settingsHighlightCurrentLine,
               ui::SettingsControl::HighlightCurrentLine);
        toggle(layout.cursorBlink, "Blinking cursor", state.settingsCursorBlink,
               ui::SettingsControl::CursorBlink);

        static constexpr std::array<std::string_view, 4> kCursorLabels{
            "Line", "Block", "Underscore", "Kitty"};
        static constexpr std::array<editor::CursorStyle, 4> kCursorValues{
            editor::CursorStyle::Line, editor::CursorStyle::Block,
            editor::CursorStyle::Underscore, editor::CursorStyle::Kitty};
        static constexpr std::array<ui::SettingsControl, 4> kCursorControls{
            ui::SettingsControl::CursorLine, ui::SettingsControl::CursorBlock,
            ui::SettingsControl::CursorUnderscore, ui::SettingsControl::CursorKitty};
        for (std::size_t index = 0; index < layout.cursorButtons.size(); ++index) {
            const bool selected =
                state.settingsCursorStyle == kCursorValues[index];
            const bool hovered = state.settingsHovered == kCursorControls[index];
            const ui::Rect button = toScreen(layout.cursorButtons[index]);
            addPanelRect(button, 4.0F * contentScale, 1.0F * contentScale, 0.98F,
                         selected ? ui::theme::kSelection : ui::theme::kSurface,
                         selected || hovered ? ui::theme::kPrimary
                                             : ui::theme::kBorder);
            centeredText(kCursorLabels[index], button,
                         selected ? accentText : mutedText);
        }

        if (layout.iconButtons[0].x1 > layout.iconButtons[0].x0) {
            if (layout.iconLabel.x1 > layout.iconLabel.x0) sectionLabel("APP ICON", layout.iconLabel);
            static constexpr std::array<std::string_view, 3> names{"Classic", "Accent", "Outline"};
            static constexpr std::array<std::string_view, 3> keys{"classic", "accent", "outline"};
            static constexpr std::array<ui::SettingsControl, 3> controls{
                ui::SettingsControl::IconClassic, ui::SettingsControl::IconAccent, ui::SettingsControl::IconOutline};
            for (std::size_t i = 0; i < names.size(); ++i) {
                const bool selected = state.settingsIconTheme == keys[i];
                const auto button = toScreen(layout.iconButtons[i]);
                addPanelRect(button, 4.0F * contentScale, 1.0F * contentScale, 0.98F,
                    selected ? ui::theme::kSelection : ui::theme::kSurface,
                    selected || state.settingsHovered == controls[i] ? ui::theme::kPrimary : ui::theme::kBorder);
                centeredText(names[i], button, selected ? accentText : mutedText);
            }
        }

        if (layout.backgroundMotion.x1 > layout.backgroundMotion.x0) {
            toggle(layout.backgroundMotion, "Motion", state.settingsBackgroundMotion,
                   ui::SettingsControl::BackgroundMotion);
        }

        const ui::Rect openSettingsJson = toScreen(layout.openSettingsJson);
        const bool openHovered =
            state.settingsHovered == ui::SettingsControl::OpenSettingsJson;
        addPanelRect(openSettingsJson, 4.0F * contentScale, 1.0F * contentScale,
                     state.settingsFileAvailable ? 0.98F : 0.42F,
                     openHovered ? ui::theme::kSuggestionSelected
                                 : ui::theme::kSurface,
                     openHovered ? ui::theme::kPrimary : ui::theme::kBorder);
        centeredText("Edit settings...", openSettingsJson,
                     state.settingsFileAvailable
                         ? (openHovered ? accentText : normalText)
                         : mutedText);

        const ui::Rect aiSettings = toScreen(layout.aiSettings);
        const bool aiHovered = state.settingsHovered == ui::SettingsControl::AiSettings;
        addPanelRect(aiSettings, 4.0F * contentScale, 1.0F * contentScale, 0.98F,
                     aiHovered ? ui::theme::kSuggestionSelected : ui::theme::kSurface,
                     aiHovered ? ui::theme::kPrimary : ui::theme::kBorder);
        centeredText("AI settings", aiSettings, aiHovered ? accentText : normalText);

        const ui::Rect done = toScreen(layout.done);
        const bool doneHovered = state.settingsHovered == ui::SettingsControl::Done;
        addPanelRect(done, 4.0F * contentScale, 1.0F * contentScale,
                     doneHovered ? 1.0F : 0.92F, ui::theme::kPrimary,
                     ui::theme::kPrimary);
        centeredText("Done", done, doneText);

        if (vertices.size() > settingsFirst) {
            settingsPanelTextRange = makeRange(settingsFirst, fullScissor);
            hasSettingsPanelText = true;
        }
    }

    const auto diagramFirst=vertices.size();
    vertices.insert(vertices.end(),diagramVertices.begin(),diagramVertices.end());
    const TextRange diagramRange{static_cast<std::uint32_t>(diagramFirst),static_cast<std::uint32_t>(diagramVertices.size()),diagramScissor};
    const auto previewFirst = vertices.size();
    for (const auto& q : previewQuads) {
        const TextVertex a{q.x0,q.y0,0,0,1,1,1,1}, b{q.x1,q.y0,1,0,1,1,1,1}, c{q.x1,q.y1,1,1,1,1,1,1}, d{q.x0,q.y1,0,1,1,1,1,1};
        vertices.insert(vertices.end(), {a,b,c,a,c,d});
    }
    const Buffer& vertexBuffer = m_vertexBuffers[frameIndex % kFramesInFlight];
    if (!vertices.empty()) {
        const std::size_t vertexCount =
            std::min(vertices.size(), static_cast<std::size_t>(kMaxTextVertices));
        vertexBuffer.upload(vertices.data(),
                            static_cast<VkDeviceSize>(vertexCount) * sizeof(TextVertex));
    }

    const auto drawTextRange = [&](const TextRange& range, float rangeBlur,
                                   float rangeOpacity) {
        if (range.vertexCount == 0) {
            return;
        }

        vkCmdSetScissor(commandBuffer, 0, 1, &range.scissor);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_textPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_textPipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);

        TextPushConstants textPushConstants{};
        textPushConstants.viewport[0] = viewportWidth;
        textPushConstants.viewport[1] = viewportHeight;
        textPushConstants.blur = std::min(rangeBlur, 8.0F);
        textPushConstants.opacity = rangeOpacity;
        vkCmdPushConstants(commandBuffer, m_textPipelineLayout,
                           static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT |
                                                           VK_SHADER_STAGE_FRAGMENT_BIT),
                           0, sizeof(textPushConstants), &textPushConstants);

        const VkBuffer vertexBuffers[] = {vertexBuffer.handle()};
        const VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdDraw(commandBuffer, range.vertexCount, 1, range.firstVertex, 0);
    };

    if (hasHighlight) {
        RectPushConstants highlightPushConstants{};
        fillRectPushConstants(highlightPushConstants, 0.0F, m_animatedHighlightTop,
                              viewportWidth, m_animatedHighlightBottom, 0.0F, 0.0F, 0.0F,
                              0.05F, ui::theme::kPrimary, ui::theme::kPrimary);
        drawRect(commandBuffer, highlightPushConstants);
    }

    for (const WelcomeRectSpec& spec : diagnosticRects) {
        RectPushConstants diagnosticPushConstants{};
        fillRectPushConstants(diagnosticPushConstants, spec.x0, spec.y0, spec.x1,
                              spec.y1, spec.radius, spec.borderWidth, spec.blur,
                              spec.opacity, spec.fill, spec.border);
        drawRect(commandBuffer, diagnosticPushConstants);
    }

    for (const std::array<float, 4>& selection : selectionRects) {
        RectPushConstants selectionPushConstants{};
        fillRectPushConstants(selectionPushConstants, selection[0], selection[1],
                              selection[2], selection[3], 0.0F, 0.0F, 0.0F, 1.0F,
                              ui::theme::kSelection, ui::theme::kSelection);
        drawRect(commandBuffer, selectionPushConstants);
    }

    if (hasEditor) {
        drawTextRange(editorRange, 0.0F, 1.0F);
    }

    for (const WelcomeRectSpec& spec : compileMarkers) {
        RectPushConstants markerPushConstants{};
        fillRectPushConstants(markerPushConstants, spec.x0, spec.y0, spec.x1,
                              spec.y1, spec.radius, spec.borderWidth, spec.blur,
                              spec.opacity, spec.fill, spec.border);
        drawRect(commandBuffer, markerPushConstants);
    }

    for (const WelcomeRectSpec& spec : editorCaretRects) {
        RectPushConstants caretPushConstants{};
        fillRectPushConstants(caretPushConstants, spec.x0, spec.y0, spec.x1,
                              spec.y1, spec.radius, spec.borderWidth, spec.blur,
                              spec.opacity, spec.fill, spec.border);
        drawRect(commandBuffer, caretPushConstants);
    }

    for (const auto& spec : completionRects) {
        RectPushConstants constants{};
        fillRectPushConstants(constants, spec.x0, spec.y0, spec.x1, spec.y1,
                              spec.radius, spec.borderWidth, spec.blur, spec.opacity,
                              spec.fill, spec.border);
        drawRect(commandBuffer, constants);
    }
    if (hasCompletionText) drawTextRange(completionRange, 0.0F, 1.0F);

    if (hasWindowTitle && !state.commandVisible) {
        drawTextRange(windowTitleRange, 0.0F, 1.0F);
    }

    if (hasRibbon) {
        drawTextRange(ribbonRange, 0.0F, 1.0F);
    }

    if (hasDirtyDot) {
        const float pulse =
            0.58F + 0.42F *
                        (0.5F + 0.5F * std::sin(state.animationTime * 4.2F));
        RectPushConstants dotPushConstants{};
        fillRectPushConstants(dotPushConstants, dirtyDotX0, dirtyDotY0, dirtyDotX1,
                              dirtyDotY1, (dirtyDotX1 - dirtyDotX0) * 0.5F, 0.0F, 0.0F,
                              pulse, ui::theme::kAccent, ui::theme::kAccent);
        drawRect(commandBuffer, dotPushConstants);
    }

    for (const WelcomeRectSpec& spec : welcomeRects) {
        RectPushConstants rectPushConstants{};
        fillRectPushConstants(rectPushConstants, spec.x0, spec.y0, spec.x1, spec.y1,
                              spec.radius, spec.borderWidth, spec.blur, spec.opacity,
                              spec.fill, spec.border);
        drawRect(commandBuffer, rectPushConstants);
    }

    if (hasWelcomeText) {
        drawTextRange(welcomeRange, (1.0F - startup) * 3.0F, startup);
    }

    if (state.commandVisible) {
        RectPushConstants dimPushConstants{};
        const float dimOpacity = 0.58F * barOpacity;
        fillRectPushConstants(dimPushConstants, 0.0F, 0.0F, viewportWidth, viewportHeight,
                              0.0F, 0.0F, 0.0F, dimOpacity,
                              ui::theme::kBackground, ui::theme::kBackground);
        drawRect(commandBuffer, dimPushConstants);

        if (hasWindowTitle) {
            drawTextRange(windowTitleRange, 0.0F, 1.0F);
        }

        if (hasSuggestionSelection) {
            RectPushConstants selectionPushConstants{};
            fillRectPushConstants(selectionPushConstants, suggestionSelectionX0,
                                  suggestionSelectionY0, suggestionSelectionX1,
                                  suggestionSelectionY1,
                                  suggestionSelectionRadius, 0.0F, 0.0F,
                                  suggestionSelectionOpacity,
                                  ui::theme::kSuggestionSelected,
                                  ui::theme::kSuggestionSelected);
            drawRect(commandBuffer, selectionPushConstants);
        }

        if (hasSuggestionRail) {
            RectPushConstants railPushConstants{};
            fillRectPushConstants(railPushConstants, suggestionRailX0, suggestionRailY0,
                                  suggestionRailX1, suggestionRailY1,
                                  ui::theme::kSuggestionRailWidth * contentScale * 0.5F,
                                  0.0F, 0.0F, suggestionSelectionOpacity,
                                  ui::theme::kAccent, ui::theme::kAccent);
            drawRect(commandBuffer, railPushConstants);
        }

        for (const WelcomeRectSpec& spec : suggestionIcons) {
            RectPushConstants iconPushConstants{};
            fillRectPushConstants(iconPushConstants, spec.x0, spec.y0, spec.x1,
                                  spec.y1, spec.radius, spec.borderWidth, spec.blur,
                                  spec.opacity, spec.fill, spec.border);
            drawRect(commandBuffer, iconPushConstants);
        }

        if (hasCommandSelection) {
            RectPushConstants selectionPushConstants{};
            fillRectPushConstants(selectionPushConstants, commandSelectionX0, commandSelectionY0,
                                  commandSelectionX1, commandSelectionY1, 0.0F, 0.0F, barBlur,
                                  barOpacity, ui::theme::kSelection, ui::theme::kSelection);
            drawRect(commandBuffer, selectionPushConstants);
        }
        if (hasCommandText) {
            drawTextRange(commandTextRange, barBlur, barOpacity);
        }

        if (hasCommandCaret) {
            RectPushConstants caretPushConstants{};
            fillRectPushConstants(caretPushConstants, commandCaretX0, commandCaretY0,
                                  commandCaretX1, commandCaretY1,
                                  (commandCaretX1 - commandCaretX0) * 0.5F, 0.0F, barBlur,
                                  barOpacity, ui::theme::kCaret, ui::theme::kCaret);
            drawRect(commandBuffer, caretPushConstants);
        }

        if (hasCommandStatus) {
            drawTextRange(commandStatusRange, barBlur, barOpacity);
        }

        for (std::size_t index = 0; index < suggestionRangeCount; ++index) {
            drawTextRange(suggestionRanges[index], barBlur,
                          suggestionOpacities[index]);
        }
    }

    if (hasStatus) {
        drawTextRange(statusRange, 0.0F, 1.0F);
    }

    for (const WelcomeRectSpec& spec : overlayRects) {
        RectPushConstants rectPushConstants{};
        fillRectPushConstants(rectPushConstants, spec.x0, spec.y0, spec.x1, spec.y1,
                              spec.radius, spec.borderWidth, spec.blur, spec.opacity,
                              spec.fill, spec.border);
        drawRect(commandBuffer, rectPushConstants);
    }

    if (state.visualPreview && state.visualPreview->generation == m_previewGeneration && previewFirst + previewQuads.size() * 6 <= kMaxTextVertices) {
        vkCmdSetScissor(commandBuffer, 0, 1, &fullScissor);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_imagePipeline);
        TextPushConstants push{}; push.viewport[0] = viewportWidth; push.viewport[1] = viewportHeight; push.blur = isSrgbFormat(m_format) ? 1.0F : 0.0F; push.opacity = 1.0F;
        vkCmdPushConstants(commandBuffer, m_textPipelineLayout, static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), 0, sizeof(push), &push);
        const VkBuffer buffers[] = {vertexBuffer.handle()}; const VkDeviceSize offsets[] = {0}; vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
        for (std::size_t i = 0; i < previewQuads.size(); ++i) {
            const auto descriptor = m_previewSets[previewQuads[i].image];
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_textPipelineLayout, 0, 1, &descriptor, 0, nullptr);
            vkCmdDraw(commandBuffer, 6, 1, static_cast<std::uint32_t>(previewFirst + i * 6), 0);
        }
    }

    for (const auto& spec:visualMaskRects) { RectPushConstants push{}; fillRectPushConstants(push,spec.x0,spec.y0,spec.x1,spec.y1,spec.radius,spec.borderWidth,spec.blur,spec.opacity,spec.fill,spec.border); drawRect(commandBuffer,push); }

    if (state.diagramPreviewVisible && diagramScissor.extent.width && diagramScissor.extent.height) {
        vkCmdSetScissor(commandBuffer,0,1,&diagramScissor);
        for (const auto& spec : diagramRects) { RectPushConstants push{}; fillRectPushConstants(push,spec.x0,spec.y0,spec.x1,spec.y1,spec.radius,spec.borderWidth,spec.blur,spec.opacity,spec.fill,spec.border); drawRect(commandBuffer,push); }
        if (diagramFirst + diagramVertices.size() <= kMaxTextVertices) drawTextRange(diagramRange,0.0F,1.0F);
        vkCmdSetScissor(commandBuffer,0,1,&fullScissor);
    }
    if (hasOverlayText) {
        drawTextRange(overlayTextRange, 0.0F, 1.0F);
    }

    for (const WelcomeRectSpec& spec : terminalRects) {
        RectPushConstants terminalPushConstants{};
        fillRectPushConstants(terminalPushConstants, spec.x0, spec.y0, spec.x1,
                              spec.y1, spec.radius, spec.borderWidth, spec.blur,
                              spec.opacity, spec.fill, spec.border);
        drawRect(commandBuffer, terminalPushConstants);
    }

    if (hasTerminalText) {
        drawTextRange(terminalTextRange, 0.0F, state.compileTerminalOpacity);
    }

    for (const WelcomeRectSpec& spec : settingsPanelRects) {
        RectPushConstants settingsPushConstants{};
        fillRectPushConstants(settingsPushConstants, spec.x0, spec.y0, spec.x1,
                              spec.y1, spec.radius, spec.borderWidth, spec.blur,
                              spec.opacity, spec.fill, spec.border);
        drawRect(commandBuffer, settingsPushConstants);
    }

    if (hasSettingsPanelText) {
        drawTextRange(settingsPanelTextRange, 0.0F, 1.0F);
    }
}

void UiPass::drawRect(VkCommandBuffer commandBuffer,
                      const RectPushConstants& pushConstants) const {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_rectPipeline);
    vkCmdPushConstants(commandBuffer, m_rectPipelineLayout,
                       static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT |
                                                       VK_SHADER_STAGE_FRAGMENT_BIT),
                       0, sizeof(pushConstants), &pushConstants);
    vkCmdDraw(commandBuffer, 6, 1, 0, 0);
}

void UiPass::fillRectPushConstants(RectPushConstants& out, float x0, float y0, float x1,
                                   float y1, float radius, float borderWidth, float blur,
                                   float opacity, const ui::theme::Color& fill,
                                   const ui::theme::Color& border) const {
    out.viewport[0] = static_cast<float>(m_extent.width);
    out.viewport[1] = static_cast<float>(m_extent.height);
    out.rectMin[0] = x0;
    out.rectMin[1] = y0;
    out.rectMax[0] = x1;
    out.rectMax[1] = y1;
    out.radius = radius;
    out.borderWidth = borderWidth;
    out.blur = blur;
    out.opacity = opacity;

    const std::array<float, 4> fillColor = toGpuColor(fill);
    const std::array<float, 4> borderColor = toGpuColor(border);
    std::copy(fillColor.begin(), fillColor.end(), out.fillColor);
    std::copy(borderColor.begin(), borderColor.end(), out.borderColor);
}

std::array<float, 4> UiPass::toGpuColor(const ui::theme::Color& color) const {
    const std::array<float, 4> value{color.r, color.g, color.b, color.a};
    return isSrgbFormat(m_format) ? srgbToLinear(value) : value;
}

float UiPass::appendText(std::vector<TextVertex>& vertices, std::string_view text, float x,
                         float baseline, const std::array<float, 4>& color) const {
    float pen = x;
    const float tabWidth = m_font.glyph(U' ').advance * 4.0F;

    for (std::size_t offset = 0; offset < text.size();) {
        const char32_t character = text::nextUtf8(text, offset);
        if (character == U'\t') {
            pen += tabWidth;
            continue;
        }

        const text::Glyph& glyph = m_font.glyph(character);
        if (glyph.hasPixels) {
            const float left = pen + glyph.x0;
            const float right = pen + glyph.x1;
            const float top = baseline + glyph.y0;
            const float bottom = baseline + glyph.y1;

            vertices.push_back(TextVertex{left, top, glyph.u0, glyph.v0, color[0], color[1],
                                          color[2], color[3]});
            vertices.push_back(TextVertex{right, top, glyph.u1, glyph.v0, color[0], color[1],
                                          color[2], color[3]});
            vertices.push_back(TextVertex{left, bottom, glyph.u0, glyph.v1, color[0],
                                          color[1], color[2], color[3]});
            vertices.push_back(TextVertex{left, bottom, glyph.u0, glyph.v1, color[0],
                                          color[1], color[2], color[3]});
            vertices.push_back(TextVertex{right, top, glyph.u1, glyph.v0, color[0], color[1],
                                          color[2], color[3]});
            vertices.push_back(TextVertex{right, bottom, glyph.u1, glyph.v1, color[0],
                                          color[1], color[2], color[3]});
        }
        pen += glyph.advance;
    }

    return pen;
}

float UiPass::appendText(std::vector<TextVertex>& vertices, std::u32string_view text, float x,
                         float baseline, const std::array<float, 4>& color) const {
    float pen = x;
    const float tabWidth = m_font.glyph(U' ').advance * 4.0F;

    for (const char32_t character : text) {
        if (character == U'\t') {
            pen += tabWidth;
            continue;
        }

        const text::Glyph& glyph = m_font.glyph(character);
        if (glyph.hasPixels) {
            const float left = pen + glyph.x0;
            const float right = pen + glyph.x1;
            const float top = baseline + glyph.y0;
            const float bottom = baseline + glyph.y1;

            vertices.push_back(TextVertex{left, top, glyph.u0, glyph.v0, color[0], color[1],
                                          color[2], color[3]});
            vertices.push_back(TextVertex{right, top, glyph.u1, glyph.v0, color[0], color[1],
                                          color[2], color[3]});
            vertices.push_back(TextVertex{left, bottom, glyph.u0, glyph.v1, color[0],
                                          color[1], color[2], color[3]});
            vertices.push_back(TextVertex{left, bottom, glyph.u0, glyph.v1, color[0],
                                          color[1], color[2], color[3]});
            vertices.push_back(TextVertex{right, top, glyph.u1, glyph.v0, color[0], color[1],
                                          color[2], color[3]});
            vertices.push_back(TextVertex{right, bottom, glyph.u1, glyph.v1, color[0],
                                          color[1], color[2], color[3]});
        }
        pen += glyph.advance;
    }

    return pen;
}

float UiPass::measureText(std::string_view text) const {
    float width = 0.0F;
    const float tabWidth = m_font.glyph(U' ').advance * 4.0F;

    for (std::size_t offset = 0; offset < text.size();) {
        const char32_t character = text::nextUtf8(text, offset);
        if (character == U'\t') {
            width += tabWidth;
        } else {
            width += m_font.glyph(character).advance;
        }
    }
    return width;
}

float UiPass::measureText(std::u32string_view text) const {
    float width = 0.0F;
    const float tabWidth = m_font.glyph(U' ').advance * 4.0F;

    for (const char32_t character : text) {
        if (character == U'\t') {
            width += tabWidth;
        } else {
            width += m_font.glyph(character).advance;
        }
    }
    return width;
}

} // namespace lucid::vk
