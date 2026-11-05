#pragma once

#include "ui/UiState.hpp"

#include <cstddef>
#include <cstdint>
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
    // Returns false if the single-line or length limit shortened the paste.
    bool appendUtf8(std::string_view text);
    void backspace();
    void deleteForward();
    void moveLeft(bool extend = false);
    void moveRight(bool extend = false);
    void moveWordLeft(bool extend = false);
    void moveWordRight(bool extend = false);
    void moveHome(bool extend = false);
    void moveEnd(bool extend = false);
    void selectAll();
    void setCursor(std::size_t offset, bool extend = false);
    void deleteSelection();
    void setQuery(std::string query);

    [[nodiscard]] bool visible() const noexcept { return m_visible; }
    [[nodiscard]] bool isOpen() const noexcept { return m_open; }
    [[nodiscard]] PromptKind kind() const noexcept { return m_kind; }
    [[nodiscard]] std::uint64_t generation() const noexcept { return m_generation; }
    [[nodiscard]] std::string_view query() const noexcept { return m_query; }
    [[nodiscard]] std::size_t cursor() const noexcept { return m_cursor; }
    [[nodiscard]] std::size_t selectionStart() const noexcept { return m_cursor < m_anchor ? m_cursor : m_anchor; }
    [[nodiscard]] std::size_t selectionEnd() const noexcept { return m_cursor > m_anchor ? m_cursor : m_anchor; }
    [[nodiscard]] bool hasSelection() const noexcept { return m_cursor != m_anchor; }

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
    std::uint64_t m_generation = 0;
    PromptKind m_kind = PromptKind::File;
    float m_value = 0.0F;
    float m_velocity = 0.0F;
    double m_lastTime = 0.0;
    bool m_hasTime = false;
    std::string m_query;
    std::size_t m_cursor = 0;
    std::size_t m_anchor = 0;
};

} // namespace lucid::ui
