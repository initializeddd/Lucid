#include "ui/search/SearchBar.hpp"
#include "text/Utf8.hpp"

#include <algorithm>
#include <cmath>

namespace lucid::ui {
namespace {

constexpr float kSpringOmega = 21.0F;
constexpr float kSpringDamping = 0.86F;
constexpr float kMaxSpringStep = 1.0F / 240.0F;
constexpr float kHiddenThreshold = 0.002F;
constexpr float kHiddenVelocity = 0.05F;
constexpr float kOpenThreshold = 0.995F;
constexpr float kMaxBlur = 2.0F;
constexpr float kSlideDistance = 12.0F;
constexpr float kMinScaleX = 0.96F;
constexpr float kSquash = 0.35F;

std::size_t previousCharacter(std::string_view value, std::size_t offset) {
    if (offset == 0) return 0;
    --offset;
    while (offset > 0 && (static_cast<unsigned char>(value[offset]) & 0xc0U) == 0x80U) --offset;
    return offset;
}

std::size_t nextCharacter(std::string_view value, std::size_t offset) {
    if (offset >= value.size()) return value.size();
    ++offset;
    while (offset < value.size() && (static_cast<unsigned char>(value[offset]) & 0xc0U) == 0x80U) ++offset;
    return offset;
}

int characterGroup(unsigned char c) {
    if (c == ' ' || c == '\t') return 0;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_' || c >= 128) return 1;
    return 2;
}

} // namespace

void SearchBar::open(PromptKind kind) {
    if (!m_open || kind != m_kind) {
        m_query.clear();
        m_cursor = m_anchor = 0;
    }

    if (m_phase == Phase::Hidden) {
        m_value = 0.0F;
        m_velocity = 0.0F;
    }

    m_kind = kind;
    m_phase = Phase::Opening;
    m_visible = true;
    m_open = true;
}

void SearchBar::close() {
    if (m_phase == Phase::Hidden) {
        return;
    }

    m_phase = Phase::Closing;
    m_open = false;
}

void SearchBar::toggle(PromptKind kind) {
    if (m_open && m_kind == kind) {
        close();
    } else {
        open(kind);
    }
}

void SearchBar::append(char32_t codepoint) {
    if (!m_open || codepoint < 32 || codepoint > 0x10FFFF ||
        (codepoint >= 0xD800 && codepoint <= 0xDFFF)) return;
    std::string encoded;
    if (codepoint < 0x80) encoded.push_back(static_cast<char>(codepoint));
    else if (codepoint < 0x800) {
        encoded.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        encoded.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint < 0x10000) {
        encoded.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        encoded.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        encoded.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        encoded.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        encoded.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        encoded.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        encoded.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    if (m_query.size() - (selectionEnd() - selectionStart()) + encoded.size() > kMaxQueryLength) return;
    deleteSelection();
    m_query.insert(m_cursor, encoded);
    m_cursor += encoded.size();
    m_anchor = m_cursor;
}

void SearchBar::backspace() {
    if (!m_open) return;
    if (hasSelection()) { deleteSelection(); return; }
    if (m_cursor == 0) return;
    std::size_t start = m_cursor - 1;
    while (start > 0 && (static_cast<unsigned char>(m_query[start]) & 0xC0) == 0x80) --start;
    m_query.erase(start, m_cursor - start);
    m_cursor = m_anchor = start;
}

void SearchBar::deleteSelection() {
    const auto start = selectionStart();
    m_query.erase(start, selectionEnd() - start);
    m_cursor = m_anchor = start;
}

void SearchBar::deleteForward() {
    if (!m_open) return;
    if (hasSelection()) { deleteSelection(); return; }
    if (m_cursor >= m_query.size()) return;
    std::size_t end = m_cursor + 1;
    while (end < m_query.size() && (static_cast<unsigned char>(m_query[end]) & 0xC0) == 0x80) ++end;
    m_query.erase(m_cursor, end - m_cursor);
}

void SearchBar::moveLeft(bool extend) {
    if (!m_open) return;
    if (!extend && hasSelection()) m_cursor = selectionStart();
    else if (m_cursor > 0) {
        --m_cursor;
        while (m_cursor > 0 && (static_cast<unsigned char>(m_query[m_cursor]) & 0xC0) == 0x80) --m_cursor;
    }
    if (!extend) m_anchor = m_cursor;
}

void SearchBar::moveRight(bool extend) {
    if (!m_open) return;
    if (!extend && hasSelection()) m_cursor = selectionEnd();
    else if (m_cursor < m_query.size()) {
        ++m_cursor;
        while (m_cursor < m_query.size() && (static_cast<unsigned char>(m_query[m_cursor]) & 0xC0) == 0x80) ++m_cursor;
    }
    if (!extend) m_anchor = m_cursor;
}

void SearchBar::moveWordLeft(bool extend) {
    if (!m_open) return;
    if (!extend && hasSelection()) { m_cursor = m_anchor = selectionStart(); return; }
    while (m_cursor > 0 && characterGroup(static_cast<unsigned char>(m_query[previousCharacter(m_query, m_cursor)])) == 0)
        m_cursor = previousCharacter(m_query, m_cursor);
    if (m_cursor > 0) {
        const int group = characterGroup(static_cast<unsigned char>(m_query[previousCharacter(m_query, m_cursor)]));
        while (m_cursor > 0 && characterGroup(static_cast<unsigned char>(m_query[previousCharacter(m_query, m_cursor)])) == group)
            m_cursor = previousCharacter(m_query, m_cursor);
    }
    if (!extend) m_anchor = m_cursor;
}

void SearchBar::moveWordRight(bool extend) {
    if (!m_open) return;
    if (!extend && hasSelection()) { m_cursor = m_anchor = selectionEnd(); return; }
    if (m_cursor < m_query.size()) {
        const int group = characterGroup(static_cast<unsigned char>(m_query[m_cursor]));
        while (m_cursor < m_query.size() && characterGroup(static_cast<unsigned char>(m_query[m_cursor])) == group)
            m_cursor = nextCharacter(m_query, m_cursor);
    }
    while (m_cursor < m_query.size() && characterGroup(static_cast<unsigned char>(m_query[m_cursor])) == 0)
        m_cursor = nextCharacter(m_query, m_cursor);
    if (!extend) m_anchor = m_cursor;
}

void SearchBar::setCursor(std::size_t offset, bool extend) {
    if (!m_open) return;
    m_cursor = std::min(offset, m_query.size());
    while (m_cursor > 0 && m_cursor < m_query.size() &&
           (static_cast<unsigned char>(m_query[m_cursor]) & 0xc0U) == 0x80U) --m_cursor;
    if (!extend) m_anchor = m_cursor;
}

void SearchBar::moveHome(bool extend) { if (m_open) { m_cursor = 0; if (!extend) m_anchor = m_cursor; } }
void SearchBar::moveEnd(bool extend) { if (m_open) { m_cursor = m_query.size(); if (!extend) m_anchor = m_cursor; } }
void SearchBar::selectAll() { if (m_open) { m_anchor = 0; m_cursor = m_query.size(); } }

bool SearchBar::appendUtf8(std::string_view text) {
    if (!m_open) return false;
    std::size_t offset = 0;
    while (offset < text.size()) {
        const char32_t codepoint = lucid::text::nextUtf8(text, offset);
        // Prompts are single-line: avoid silently joining separate pasted lines.
        if (codepoint == U'\n' || codepoint == U'\r') return false;
        if (codepoint == U'\t' || codepoint >= 32) {
            const auto before = m_query;
            const auto beforeCursor = m_cursor;
            const auto beforeAnchor = m_anchor;
            append(codepoint == U'\t' ? U' ' : codepoint);
            if (m_query == before && m_cursor == beforeCursor && m_anchor == beforeAnchor) return false; // The next character does not fit.
        }
    }
    return true;
}

void SearchBar::setQuery(std::string query) {
    if (query.size() > kMaxQueryLength) {
        std::size_t end = kMaxQueryLength;
        while (end > 0 && (static_cast<unsigned char>(query[end]) & 0xC0) == 0x80) --end;
        query.resize(end);
    }
    m_query = std::move(query);
    m_cursor = m_anchor = m_query.size();
}

void SearchBar::update(double timeSeconds, float animationSpeed) {
    if (!m_hasTime) {
        m_lastTime = timeSeconds;
        m_hasTime = true;
        return;
    }

    const double rawDelta = timeSeconds - m_lastTime;
    m_lastTime = timeSeconds;

    if (m_phase == Phase::Hidden) {
        m_value = 0.0F;
        m_velocity = 0.0F;
        return;
    }

    const float clampedDelta = static_cast<float>(std::clamp(rawDelta, 0.0, 0.05)) *
                               std::clamp(animationSpeed, 0.25F, 4.0F);
    float remaining = clampedDelta;
    const float target = m_open ? 1.0F : 0.0F;

    while (remaining > 0.0F) {
        const float step = std::min(remaining, kMaxSpringStep);
        const float acceleration = kSpringOmega * kSpringOmega * (target - m_value) -
                                   2.0F * kSpringDamping * kSpringOmega * m_velocity;
        m_velocity += acceleration * step;
        m_value += m_velocity * step;
        remaining -= step;
    }

    if (m_phase == Phase::Closing && std::abs(m_value) < kHiddenThreshold &&
        std::abs(m_velocity) < kHiddenVelocity) {
        m_phase = Phase::Hidden;
        m_visible = false;
        m_value = 0.0F;
        m_velocity = 0.0F;
        m_query.clear();
        m_cursor = m_anchor = 0;
        return;
    }

    if (m_phase == Phase::Opening && m_value > kOpenThreshold &&
        std::abs(m_velocity) < kHiddenVelocity) {
        m_phase = Phase::Open;
    }
}

FrameState SearchBar::frameState(double timeSeconds, float animationSpeed) {
    update(timeSeconds, animationSpeed);

    FrameState state;
    state.commandVisible = m_visible;
    state.commandQuery = m_query;
    state.commandCursor = m_cursor;
    state.commandSelectionStart = selectionStart();
    state.commandSelectionEnd = selectionEnd();
    state.commandKind = m_kind;
    state.timeSeconds = timeSeconds;

    if (!m_visible) {
        state.opacity = 0.0F;
        return state;
    }

    const float progress = std::clamp(m_value, 0.0F, 1.0F);
    state.opacity = progress;
    state.scaleX = kMinScaleX + (1.0F - kMinScaleX) * progress;
    state.scaleY = 1.0F + (1.0F - state.scaleX) * kSquash;
    state.offsetX = -kSlideDistance * (1.0F - progress);
    state.blur = kMaxBlur * (1.0F - progress);
    return state;
}

} // namespace lucid::ui
