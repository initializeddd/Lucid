#pragma once

#include "ui/UiState.hpp"

#include <array>
#include <cstdint>
#include <memory>

namespace lucid {

class Window;

struct RendererConfig {
    std::array<float, 4> clearColor{0.10980F, 0.10980F, 0.10980F, 1.0F};
#if defined(LUCID_DEBUG) && LUCID_ENABLE_VALIDATION
    bool enableValidation = true;
#else
    bool enableValidation = false;
#endif
};

class Renderer {
public:
    Renderer(Window& window, const RendererConfig& config);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    void drawFrame(const ui::FrameState& state);
    void setUiZoom(float zoom);
    void setClearColor(const std::array<float, 4>& color);
    void waitIdle();
    [[nodiscard]] float logicalCharacterAdvance() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace lucid
