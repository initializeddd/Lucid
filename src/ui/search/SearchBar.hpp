#pragma once

#include "ui/UiState.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace lucid::ui {

class SearchBar {
public:
    static constexpr std::size_t kMaxQueryLength = 256;

    void open(PromptKind kind = PromptKind::File);
    void close();
    void toggle(PromptKind kind = PromptKind::File);
    void append(char32_t codepoint);
    void backspace();
    void setQuery(std::string query);

    [[nodiscard]] bool visible() const noexcept { return m_visible; }
    [[nodiscard]] bool isOpen() const noexcept { return m_open; }
    [[nodiscard]] PromptKind kind() const noexcept { return m_kind; }
    [[nodiscard]] std::string_view query() const noexcept { return m_query; }

    [[nodiscard]] FrameState frameState(double timeSeconds,
                                       float animationSpeed = 1.0F);

private:
    enum class Phase {
        Hidden,
        Opening,
        Open,
        Closing,
    };

    void update(double timeSeconds, float animationSpeed);

    Phase m_phase = Phase::Hidden;
    bool m_visible = false;
    bool m_open = false;
    PromptKind m_kind = PromptKind::File;
    float m_value = 0.0F;
    float m_velocity = 0.0F;
    double m_lastTime = 0.0;
    bool m_hasTime = false;
    std::string m_query;
};

} // namespace lucid::ui
