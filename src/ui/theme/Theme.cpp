#include "ui/theme/Theme.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace lucid::ui::theme {
namespace {

constexpr Color rgb(int red, int green, int blue) {
    return Color{static_cast<float>(red) / 255.0F,
                 static_cast<float>(green) / 255.0F,
                 static_cast<float>(blue) / 255.0F, 1.0F};
}

[[nodiscard]] std::string lowercase(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

} // namespace

Palette lucidDarkPalette() {
    Palette palette{};
    palette.background = rgb(28, 28, 28);
    palette.surface = rgb(36, 36, 36);
    palette.border = rgb(55, 55, 59);
    palette.text = rgb(230, 230, 234);
    palette.textMuted = rgb(154, 154, 163);
    palette.lineNumber = rgb(85, 85, 92);
    palette.caret = rgb(244, 244, 247);
    palette.selection = rgb(48, 48, 74);
    palette.primary = rgb(120, 126, 255);
    palette.error = rgb(248, 113, 113);
    palette.warning = rgb(251, 191, 36);

    palette.syntaxKeyword = rgb(199, 146, 234);
    palette.syntaxType = rgb(255, 203, 107);
    palette.syntaxString = rgb(195, 232, 141);
    palette.syntaxNumber = rgb(247, 140, 108);
    palette.syntaxComment = rgb(112, 120, 135);
    palette.syntaxPreprocessor = rgb(240, 113, 120);
    palette.syntaxFunction = palette.primary;
    palette.syntaxPunctuation = rgb(122, 129, 144);
    palette.syntaxCompileComment = palette.syntaxComment;
    palette.syntaxCompileDirective = rgb(156, 160, 168);
    palette.syntaxCompileFunction = rgb(255, 199, 92);
    palette.syntaxCompileCommand = rgb(240, 210, 111);
    palette.syntaxCompileVariable = rgb(255, 224, 135);
    return palette;
}

Palette graphitePalette() {
    Palette palette = lucidDarkPalette();
    palette.background = rgb(26, 26, 26);
    palette.surface = rgb(35, 35, 35);
    palette.border = rgb(58, 58, 58);
    palette.text = rgb(232, 228, 220);
    palette.textMuted = rgb(167, 160, 150);
    palette.lineNumber = rgb(86, 80, 70);
    palette.caret = rgb(255, 248, 236);
    palette.selection = rgb(58, 52, 43);
    palette.primary = rgb(224, 179, 106);
    palette.error = rgb(240, 120, 120);
    palette.warning = rgb(242, 201, 76);
    palette.syntaxKeyword = rgb(211, 170, 120);
    palette.syntaxType = rgb(240, 205, 140);
    palette.syntaxString = rgb(190, 205, 143);
    palette.syntaxNumber = rgb(228, 150, 110);
    palette.syntaxComment = rgb(125, 117, 105);
    palette.syntaxPreprocessor = rgb(224, 128, 112);
    palette.syntaxFunction = palette.primary;
    palette.syntaxPunctuation = rgb(137, 129, 117);
    palette.syntaxCompileFunction = rgb(237, 191, 113);
    palette.syntaxCompileCommand = rgb(232, 204, 142);
    palette.syntaxCompileVariable = rgb(246, 220, 158);
    return palette;
}

Palette lucidLightPalette() {
    Palette palette{};
    palette.background = rgb(244, 244, 246);
    palette.surface = rgb(255, 255, 255);
    palette.border = rgb(212, 212, 218);
    palette.text = rgb(28, 28, 34);
    palette.textMuted = rgb(102, 102, 111);
    palette.lineNumber = rgb(160, 160, 168);
    palette.caret = rgb(28, 28, 34);
    palette.selection = rgb(217, 221, 251);
    palette.primary = rgb(79, 86, 232);
    palette.error = rgb(211, 59, 59);
    palette.warning = rgb(183, 121, 0);

    palette.syntaxKeyword = rgb(136, 45, 176);
    palette.syntaxType = rgb(154, 103, 0);
    palette.syntaxString = rgb(47, 112, 41);
    palette.syntaxNumber = rgb(172, 70, 37);
    palette.syntaxComment = rgb(112, 112, 122);
    palette.syntaxPreprocessor = rgb(176, 45, 62);
    palette.syntaxFunction = palette.primary;
    palette.syntaxPunctuation = rgb(104, 104, 116);
    palette.syntaxCompileComment = palette.syntaxComment;
    palette.syntaxCompileDirective = rgb(92, 92, 104);
    palette.syntaxCompileFunction = rgb(148, 94, 0);
    palette.syntaxCompileCommand = rgb(126, 91, 0);
    palette.syntaxCompileVariable = rgb(115, 76, 0);
    return palette;
}

bool paletteByName(std::string_view name, Palette& out) noexcept {
    const std::string key = lowercase(name);
    if (key == "lucid-dark" || key == "dark" || key.empty()) {
        out = lucidDarkPalette();
        return true;
    }
    if (key == "graphite") {
        out = graphitePalette();
        return true;
    }
    if (key == "lucid-light" || key == "light") {
        out = lucidLightPalette();
        return true;
    }
    return false;
}

std::string_view defaultThemeName() noexcept {
    return "lucid-dark";
}

Color selectionTint(const Color& background, const Color& primary) noexcept {
    constexpr float kPrimaryMix = 0.20F;
    return Color{background.r + (primary.r - background.r) * kPrimaryMix,
                 background.g + (primary.g - background.g) * kPrimaryMix,
                 background.b + (primary.b - background.b) * kPrimaryMix,
                 background.a};
}

void applyPalette(const Palette& palette) noexcept {
    kBackground = palette.background;
    kSurface = palette.surface;
    kBorder = palette.border;
    kText = palette.text;
    kTextMuted = palette.textMuted;
    kLineNumber = palette.lineNumber;
    kCaret = palette.caret;
    kSelection = palette.selection;
    kPrimary = palette.primary;
    kAccent = palette.primary;
    kError = palette.error;
    kWarning = palette.warning;

    kLightTheme = luminance(kBackground) > 0.5F;
    kLogoForeground = kLightTheme ? Color{0.0F, 0.0F, 0.0F, 1.0F}
                                  : Color{1.0F, 1.0F, 1.0F, 1.0F};
    kLogoAccent = palette.primary;

    kSyntaxKeyword = palette.syntaxKeyword;
    kSyntaxType = palette.syntaxType;
    kSyntaxString = palette.syntaxString;
    kSyntaxNumber = palette.syntaxNumber;
    kSyntaxComment = palette.syntaxComment;
    kSyntaxPreprocessor = palette.syntaxPreprocessor;
    kSyntaxFunction = palette.syntaxFunction;
    kSyntaxPunctuation = palette.syntaxPunctuation;
    kSyntaxCompileComment = palette.syntaxCompileComment;
    kSyntaxCompileDirective = palette.syntaxCompileDirective;
    kSyntaxCompileFunction = palette.syntaxCompileFunction;
    kSyntaxCompileCommand = palette.syntaxCompileCommand;
    kSyntaxCompileVariable = palette.syntaxCompileVariable;

    kSuggestionSurface = palette.surface;
    kSuggestionSelected = palette.selection;
}

} // namespace lucid::ui::theme
