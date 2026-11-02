#pragma once

#include <array>
#include <string_view>

namespace lucid::ui::theme {

struct Color {
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 1.0F;
};

struct Palette {
    Color background;
    Color surface;
    Color border;
    Color text;
    Color textMuted;
    Color lineNumber;
    Color caret;
    Color selection;
    Color primary;
    Color error;
    Color warning;
    Color syntaxKeyword;
    Color syntaxType;
    Color syntaxString;
    Color syntaxNumber;
    Color syntaxComment;
    Color syntaxPreprocessor;
    Color syntaxFunction;
    Color syntaxPunctuation;
    Color syntaxCompileComment;
    Color syntaxCompileDirective;
    Color syntaxCompileFunction;
    Color syntaxCompileCommand;
    Color syntaxCompileVariable;
};

[[nodiscard]] constexpr float luminance(const Color& color) noexcept {
    return 0.2126F * color.r + 0.7152F * color.g + 0.0722F * color.b;
}

[[nodiscard]] Palette lucidDarkPalette();
[[nodiscard]] Palette graphitePalette();
[[nodiscard]] Palette lucidLightPalette();

[[nodiscard]] bool paletteByName(std::string_view name, Palette& out) noexcept;
[[nodiscard]] std::string_view defaultThemeName() noexcept;
[[nodiscard]] Color selectionTint(const Color& background,
                                  const Color& primary) noexcept;
void applyPalette(const Palette& palette) noexcept;

// Colors available from the settings panel. Any #RRGGBB value is valid in settings.json.
inline constexpr std::array<Color, 8> kPrimarySwatches{
    Color{0.47059F, 0.49412F, 1.0F, 1.0F},
    Color{0.43137F, 0.65882F, 1.0F, 1.0F},
    Color{0.30196F, 0.81569F, 0.88235F, 1.0F},
    Color{0.48235F, 0.84706F, 0.56078F, 1.0F},
    Color{0.87843F, 0.70196F, 0.41569F, 1.0F},
    Color{1.0F, 0.62353F, 0.40784F, 1.0F},
    Color{1.0F, 0.47843F, 0.56471F, 1.0F},
    Color{0.78039F, 0.57255F, 0.91765F, 1.0F},
};

// Runtime palette. Apply a preset or custom palette through applyPalette().
inline Color kBackground{0.10980F, 0.10980F, 0.10980F, 1.0F};
inline Color kSurface{0.14118F, 0.14118F, 0.14118F, 1.0F};
inline Color kBorder{0.21569F, 0.21569F, 0.23137F, 1.0F};
inline Color kText{0.90196F, 0.90196F, 0.91765F, 1.0F};
inline Color kTextMuted{0.60392F, 0.60392F, 0.63922F, 1.0F};
inline Color kLineNumber{0.33333F, 0.33333F, 0.36078F, 1.0F};
inline Color kCaret{0.96078F, 0.96078F, 0.96863F, 1.0F};
inline Color kSelection{0.18824F, 0.18824F, 0.29020F, 1.0F};
inline Color kPrimary{0.47059F, 0.49412F, 1.0F, 1.0F};
inline Color kAccent = kPrimary;
inline Color kError{0.97255F, 0.44314F, 0.44314F, 1.0F};
inline Color kWarning{0.98431F, 0.74902F, 0.14118F, 1.0F};

inline bool kLightTheme = false;
inline Color kLogoForeground{1.0F, 1.0F, 1.0F, 1.0F};
inline Color kLogoAccent = kPrimary;

inline Color kSyntaxKeyword{0.78039F, 0.57255F, 0.91765F, 1.0F};
inline Color kSyntaxType{1.0F, 0.79608F, 0.41961F, 1.0F};
inline Color kSyntaxString{0.76471F, 0.90980F, 0.55294F, 1.0F};
inline Color kSyntaxNumber{0.96863F, 0.54902F, 0.42353F, 1.0F};
inline Color kSyntaxComment{0.43922F, 0.47059F, 0.52941F, 1.0F};
inline Color kSyntaxPreprocessor{0.94118F, 0.44314F, 0.47059F, 1.0F};
inline Color kSyntaxFunction = kPrimary;
inline Color kSyntaxPunctuation{0.47843F, 0.50588F, 0.56471F, 1.0F};
inline Color kSyntaxCompileComment = kSyntaxComment;
inline Color kSyntaxCompileDirective{0.61176F, 0.62745F, 0.65882F, 1.0F};
inline Color kSyntaxCompileFunction{1.0F, 0.78039F, 0.36078F, 1.0F};
inline Color kSyntaxCompileCommand{0.94118F, 0.82353F, 0.43529F, 1.0F};
inline Color kSyntaxCompileVariable{1.0F, 0.87843F, 0.52941F, 1.0F};

inline Color kSuggestionSurface{0.14118F, 0.14118F, 0.14118F, 1.0F};
inline Color kSuggestionSelected{0.18824F, 0.18824F, 0.29020F, 1.0F};

inline constexpr float kSearchBarMargin = 16.0F;
inline constexpr float kSearchBarWidth = 280.0F;
inline constexpr float kSearchBarHeight = 28.0F;
inline constexpr float kSearchBarRadius = 4.0F;
inline constexpr float kSearchBarBorder = 1.0F;
inline constexpr float kSearchBarPadding = 8.0F;
inline constexpr float kFontPixelSize = 12.0F;

inline constexpr float kEditorTop = 52.0F;
inline constexpr float kEditorBottom = 30.0F;
inline constexpr float kEditorLineHeight = 18.0F;
inline constexpr float kEditorCodeLeft = 52.0F;
inline constexpr float kEditorGutterRight = 40.0F;
inline constexpr float kSuggestionGap = 28.0F;
inline constexpr float kSuggestionLineHeight = 20.0F;
inline constexpr float kSuggestionRadius = 3.0F;
inline constexpr float kSuggestionPaddingX = 7.0F;
inline constexpr float kSuggestionRailWidth = 2.0F;
inline constexpr float kSuggestionInset = 4.0F;
inline constexpr int kMaxSuggestions = 8;
inline constexpr float kCompileTerminalWidth = 620.0F;
inline constexpr float kCompileTerminalHeight = 250.0F;
inline constexpr float kCompileTerminalRadius = 12.0F;
inline constexpr float kCompileTerminalMargin = 12.0F;
inline constexpr float kCompileTerminalTitleHeight = 38.0F;
inline constexpr float kCompileTerminalPadding = 14.0F;

} // namespace lucid::ui::theme
