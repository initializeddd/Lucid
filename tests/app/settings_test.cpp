#include "app/Settings.hpp"

#include <cmath>

namespace {

bool near(float left, float right) {
    return std::abs(left - right) < 0.001F;
}

bool colorNear(const std::optional<lucid::ui::theme::Color>& color, int red,
               int green, int blue) {
    return color.has_value() && near(color->r, static_cast<float>(red) / 255.0F) &&
           near(color->g, static_cast<float>(green) / 255.0F) &&
           near(color->b, static_cast<float>(blue) / 255.0F);
}

} // namespace

int main() {
    using lucid::parseSettings;

    const auto defaults = parseSettings(lucid::defaultSettingsJson());
    if (!defaults.valid || !near(defaults.settings.animationSpeed, 1.0F) ||
        !defaults.settings.showSyntaxErrors ||
        !defaults.settings.showLineNumbers ||
        !defaults.settings.highlightCurrentLine ||
        !defaults.settings.cursorBlink ||
        defaults.settings.cursorStyle != lucid::editor::CursorStyle::Line ||
        !defaults.settings.backgroundMotion ||
        !near(defaults.settings.zoomFactor, 1.0F) ||
        defaults.settings.theme.name != "lucid-dark" ||
        defaults.settings.theme.primary.has_value()) {
        return 1;
    }

    const auto custom = parseSettings(
        R"({"animationSpeed": 2.0, "showSyntaxErrors": false, "showLineNumbers": false, "highlightCurrentLine": false, "cursorBlink": false, "cursorStyle": "kitty", "backgroundMotion": false, "zoomFactor": 1.25, "primary": "#6ea8ff"})");
    if (!custom.valid || !near(custom.settings.animationSpeed, 2.0F) ||
        custom.settings.showSyntaxErrors || custom.settings.showLineNumbers ||
        custom.settings.highlightCurrentLine || custom.settings.cursorBlink ||
        custom.settings.cursorStyle != lucid::editor::CursorStyle::Kitty ||
        custom.settings.backgroundMotion ||
        !near(custom.settings.zoomFactor, 1.25F) ||
        !colorNear(custom.settings.theme.primary, 110, 168, 255)) {
        return 2;
    }

    const auto themes = parseSettings(
        R"({"theme": "graphite", "primary": "#E0B36A"})");
    if (!themes.valid || themes.settings.theme.name != "graphite" ||
        !colorNear(themes.settings.theme.primary, 224, 179, 106)) {
        return 3;
    }

    const auto serialized = lucid::serializeSettings(custom.settings);
    const auto roundTrip = parseSettings(serialized);
    if (!roundTrip.valid ||
        roundTrip.settings.cursorStyle != lucid::editor::CursorStyle::Kitty ||
        roundTrip.settings.theme.name != "lucid-dark" ||
        !colorNear(roundTrip.settings.theme.primary, 110, 168, 255)) {
        return 4;
    }

    if (parseSettings(R"({"unknown": true})").valid) {
        return 5;
    }
    if (parseSettings(R"({"zoomFactor": 0.1})").valid) {
        return 6;
    }
    if (parseSettings(R"({"animationSpeed": "fast"})").valid) {
        return 7;
    }
    if (parseSettings(R"({"theme": "unknown"})").valid) {
        return 8;
    }
    const auto legacyTheme = parseSettings(
        R"({"theme": "midnight", "customTheme": {"accent": "#6ea8ff", "background": "#101418"}})");
    if (!legacyTheme.valid || legacyTheme.settings.theme.name != "lucid-dark" ||
        !colorNear(legacyTheme.settings.theme.primary, 110, 168, 255)) {
        return 9;
    }
    if (!parseSettings(R"({"theme": "custom"})").valid ||
        parseSettings(R"({"customTheme": {"accent": "not-a-color"}})").valid) {
        return 10;
    }
    if (parseSettings(R"({"primary": "not-a-color"})").valid) {
        return 12;
    }
    if (parseSettings(R"({"cursorStyle": "beam"})").valid) {
        return 13;
    }
    if (parseSettings(R"({"backgroundMotion": 1})").valid) {
        return 14;
    }

    const auto legacyCursor = parseSettings(
        R"({"cursorStyle": "underline"})");
    if (!legacyCursor.valid ||
        legacyCursor.settings.cursorStyle !=
            lucid::editor::CursorStyle::Underscore) {
        return 15;
    }

    lucid::ui::theme::Palette palette;
    if (!lucid::ui::theme::paletteByName("lucid-light", palette) ||
        lucid::ui::theme::luminance(palette.background) < 0.5F) {
        return 16;
    }
    if (lucid::ui::theme::paletteByName("missing", palette) ||
        lucid::ui::theme::paletteByName("midnight", palette) ||
        lucid::ui::theme::paletteByName("custom", palette)) {
        return 17;
    }

    const auto tint = lucid::ui::theme::selectionTint(
        palette.background, lucid::ui::theme::Color{0.43137F, 0.65882F, 1.0F, 1.0F});
    if (!near(tint.r, palette.background.r +
                          (0.43137F - palette.background.r) * 0.20F) ||
        !near(tint.g, palette.background.g +
                          (0.65882F - palette.background.g) * 0.20F) ||
        !near(tint.b, palette.background.b +
                          (1.0F - palette.background.b) * 0.20F) ||
        !near(tint.a, palette.background.a)) {
        return 18;
    }

    return 0;
}
