#pragma once

namespace lucid::ui {

struct WelcomeLayout {
    float centerX = 0.0F;
    float logoLeft = 0.0F;
    float logoTop = 0.0F;
    float logoSize = 0.0F;

    float actionX0 = 0.0F;
    float actionY0 = 0.0F;
    float actionX1 = 0.0F;
    float actionY1 = 0.0F;
    float keyHeight = 0.0F;
    float hintTop = 0.0F;
    float hintLineHeight = 0.0F;
    float hintLeft = 0.0F;
    int hintColumns = 2;
    bool hintAlignLeft = false;

    float contentLeft = 0.0F;
    float contentWidth = 0.0F;

    bool showDivider = false;
    float dividerX0 = 0.0F;
    float dividerY0 = 0.0F;
    float dividerX1 = 0.0F;
    float dividerY1 = 0.0F;
};

[[nodiscard]] WelcomeLayout welcomeLayout(float viewportWidth, float viewportHeight,
                                          float scale) noexcept;

} // namespace lucid::ui
