#include "ui/Welcome.hpp"

#include <algorithm>

namespace lucid::ui {

WelcomeLayout welcomeLayout(float viewportWidth, float viewportHeight, float scale) noexcept {
    WelcomeLayout layout;
    layout.centerX = viewportWidth * 0.5F;
    layout.keyHeight = 24.0F * scale;

    const float margin = 16.0F * scale;
    const float actionHeight = 40.0F * scale;
    const float actionGap = 34.0F * scale;
    const float hintGap = 20.0F * scale;
    const float hintLineHeight = 24.0F * scale;
    layout.hintLineHeight = hintLineHeight;

    if (viewportWidth >= 650.0F * scale) {
        layout.logoSize = 140.0F * scale;
        layout.contentWidth = 372.0F * scale;

        const float columnGap = 44.0F * scale;
        const float dividerWidth = 1.0F * scale;
        const float totalWidth = layout.logoSize + columnGap + dividerWidth +
                                 columnGap + layout.contentWidth;
        const float left = std::max(margin, (viewportWidth - totalWidth) * 0.5F);

        layout.logoLeft = left;
        layout.contentLeft = left + layout.logoSize + columnGap + dividerWidth +
                             columnGap;

        layout.hintColumns = viewportWidth >= 1000.0F * scale ? 3 : 2;
        const float contentHeight = actionHeight;
        const float groupHeight = std::max(layout.logoSize, contentHeight);
        const float hintsHeight = hintLineHeight * static_cast<float>((18 + layout.hintColumns - 1) / layout.hintColumns);
        const float top = std::max(margin, (viewportHeight - groupHeight - hintGap - hintsHeight) * 0.5F);

        layout.logoTop = top + (groupHeight - layout.logoSize) * 0.5F;
        layout.actionX0 = layout.contentLeft;
        layout.actionX1 = layout.contentLeft + layout.contentWidth;
        layout.actionY0 = top + (groupHeight - contentHeight) * 0.5F;
        layout.actionY1 = layout.actionY0 + actionHeight;
        layout.hintTop = top + groupHeight + hintGap;
        layout.hintLeft = layout.centerX;
        layout.hintAlignLeft = false;

        layout.showDivider = true;
        layout.dividerX0 = layout.logoLeft + layout.logoSize + columnGap;
        layout.dividerX1 = layout.dividerX0 + dividerWidth;
        layout.dividerY0 = top;
        layout.dividerY1 = top + groupHeight;
        return layout;
    }

    layout.hintColumns = viewportWidth < 440.0F * scale ? 1 : 2;
    const float rowsHeight = hintLineHeight * static_cast<float>((18 + layout.hintColumns - 1) / layout.hintColumns);
    layout.logoSize = std::clamp(viewportHeight - actionHeight - hintGap - rowsHeight - margin * 2.0F - actionGap, 64.0F * scale, 160.0F * scale);
    layout.logoLeft = layout.centerX - layout.logoSize * 0.5F;
    const float actionWidth =
        std::min(340.0F * scale, std::max(0.0F, viewportWidth - margin * 2.0F));
    const float groupHeight =
        layout.logoSize + actionGap + actionHeight + hintGap +
        rowsHeight;

    layout.logoTop = std::max(margin, (viewportHeight - groupHeight) * 0.5F);
    layout.actionY0 = layout.logoTop + layout.logoSize + actionGap;
    layout.actionX0 = layout.centerX - actionWidth * 0.5F;
    layout.actionX1 = layout.actionX0 + actionWidth;
    layout.actionY1 = layout.actionY0 + actionHeight;
    layout.hintTop = layout.actionY1 + hintGap;
    layout.hintLeft = layout.centerX;
    layout.hintAlignLeft = false;
    layout.contentLeft = layout.actionX0;
    layout.contentWidth = actionWidth;
    return layout;
}

} // namespace lucid::ui
