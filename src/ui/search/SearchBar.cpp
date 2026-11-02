#include "ui/search/SearchBar.hpp"

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

} // namespace

void SearchBar::open(PromptKind kind) {
    if (!m_open || kind != m_kind) {
        m_query.clear();
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
    if (m_visible && m_kind == kind) {
        close();
    } else {
        open(kind);
    }
}

void SearchBar::append(char32_t codepoint) {
    if (!m_open || codepoint < 32 || codepoint > 126) {
        return;
    }
    if (m_query.size() >= kMaxQueryLength) {
        return;
    }
    m_query.push_back(static_cast<char>(codepoint));
}

void SearchBar::backspace() {
    if (!m_open || m_query.empty()) {
        return;
    }
    m_query.pop_back();
}

void SearchBar::setQuery(std::string query) {
    if (query.size() > kMaxQueryLength) {
        query.resize(kMaxQueryLength);
    }
    m_query = std::move(query);
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
