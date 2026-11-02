#include "ui/Welcome.hpp"

#include <cmath>
#include <cstddef>

namespace {

bool nearlyEqual(float a, float b, float epsilon = 0.001F) {
    return std::abs(a - b) <= epsilon;
}

} // namespace

int main() {
    using lucid::ui::welcomeLayout;

    const auto wide = welcomeLayout(1280.0F, 820.0F, 1.0F);
    if (!nearlyEqual(wide.centerX, 640.0F)) {
        return 1;
    }
    if (!wide.showDivider || wide.hintColumns != 3 || !wide.hintAlignLeft) {
        return 2;
    }
    if (!nearlyEqual(wide.logoSize, 140.0F) ||
        !nearlyEqual(wide.contentWidth, 372.0F)) {
        return 3;
    }
    if ((wide.actionX0 + wide.actionX1) * 0.5F <= wide.centerX ||
        wide.actionX1 > 1280.0F || wide.actionX0 < wide.dividerX1) {
        return 4;
    }
    if (wide.logoLeft >= wide.dividerX0 ||
        wide.dividerX0 >= wide.dividerX1 ||
        wide.dividerX1 >= wide.contentLeft) {
        return 5;
    }
    if (!nearlyEqual(wide.hintLeft, wide.contentLeft) ||
        wide.hintTop <= wide.actionY1 ||
        wide.hintTop + wide.hintLineHeight * 2.0F > 820.0F) {
        return 6;
    }
    if (wide.dividerY0 < 0.0F || wide.dividerY1 > 820.0F ||
        wide.actionY1 > 820.0F) {
        return 7;
    }

    const auto narrow = welcomeLayout(640.0F, 500.0F, 1.0F);
    if (narrow.showDivider || narrow.hintColumns != 2 || narrow.hintAlignLeft) {
        return 8;
    }
    if (!nearlyEqual(narrow.actionX0 + narrow.actionX1, 640.0F)) {
        return 9;
    }
    if (!nearlyEqual(narrow.logoLeft + narrow.logoSize * 0.5F, 320.0F)) {
        return 10;
    }
    if (narrow.actionY0 <= narrow.logoTop + narrow.logoSize ||
        narrow.hintTop <= narrow.actionY1 ||
        narrow.hintTop + narrow.hintLineHeight * 3.0F > 500.0F) {
        return 11;
    }

    const auto tiny = welcomeLayout(300.0F, 500.0F, 1.0F);
    if (tiny.actionX0 < 16.0F || tiny.actionX1 > 284.0F ||
        tiny.hintColumns != 2) {
        return 12;
    }
    if (!nearlyEqual(tiny.actionX0 + tiny.actionX1, 300.0F)) {
        return 13;
    }

    const auto threshold = welcomeLayout(650.0F, 400.0F, 1.0F);
    if (!threshold.showDivider || threshold.actionX0 < 0.0F ||
        threshold.actionX1 > 650.0F) {
        return 14;
    }

    const auto minimum = welcomeLayout(640.0F, 400.0F, 1.0F);
    if (minimum.showDivider ||
        minimum.hintTop + minimum.hintLineHeight * 3.0F > 400.0F ||
        minimum.actionY1 > 400.0F) {
        return 16;
    }

    const auto retina = welcomeLayout(2560.0F, 1640.0F, 2.0F);
    if (!nearlyEqual(retina.logoSize, 280.0F) ||
        !nearlyEqual(retina.keyHeight, 48.0F) ||
        !nearlyEqual(retina.hintLineHeight, 48.0F) ||
        !retina.showDivider) {
        return 15;
    }

    return 0;
}
