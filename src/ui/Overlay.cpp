#include "ui/Overlay.hpp"

#include "ui/theme/Theme.hpp"

#include <algorithm>

namespace lucid::ui {
namespace {

constexpr float kSettingsErrorWidth = 420.0F;
constexpr float kSettingsErrorHeight = 22.0F;
constexpr float kDiagnosticWidth = 440.0F;
constexpr float kDiagnosticHeight = 70.0F;

[[nodiscard]] Rect centeredCard(float viewportWidth, float width) {
    const float margin = theme::kSearchBarMargin;
    return Rect{viewportWidth - margin - width, 50.0F,
                viewportWidth - margin, 50.0F + kDiagnosticHeight};
}

} // namespace

SettingsErrorLayout settingsErrorLayout(float viewportWidth,
                                        float viewportHeight) noexcept {
    const float margin = theme::kSearchBarMargin;
    const float width =
        std::min(kSettingsErrorWidth, std::max(0.0F, viewportWidth - margin * 2.0F));
    (void)viewportHeight;

    SettingsErrorLayout layout;
    layout.text = Rect{viewportWidth - margin - width, margin,
                       viewportWidth - margin, margin + kSettingsErrorHeight};
    return layout;
}

DiagnosticCardLayout diagnosticCardLayout(float viewportWidth,
                                          float viewportHeight) noexcept {
    const float margin = theme::kSearchBarMargin;
    const float width =
        std::min(kDiagnosticWidth, std::max(0.0F, viewportWidth - margin * 2.0F));
    (void)viewportHeight;

    DiagnosticCardLayout layout;
    layout.card = centeredCard(viewportWidth, width);
    layout.ignoreButton =
        Rect{layout.card.x1 - 28.0F, layout.card.y0 + 8.0F,
             layout.card.x1 - 8.0F, layout.card.y0 + 28.0F};
    return layout;
}

float commandBarTop(float viewportHeight) noexcept {
    constexpr float kTop = 44.0F;
    const float bounded = std::max(theme::kSearchBarMargin,
                                   viewportHeight * 0.25F);
    return std::min(kTop, bounded);
}

SettingsPanelLayout settingsPanelLayout(float viewportWidth,
                                        float viewportHeight) noexcept {
    constexpr float kPanelWidth = 620.0F;
    constexpr float kPanelHeight = 500.0F;
    constexpr float kCompactHeight = 234.0F;
    constexpr float kPadding = 16.0F;
    constexpr float kChipGap = 8.0F;
    constexpr float kRowGap = 6.0F;
    constexpr float kPrimaryGap = 6.0F;

    SettingsPanelLayout layout;
    const float width = std::min(kPanelWidth, std::max(0.0F, viewportWidth - 32.0F));
    const float height = std::min(kPanelHeight, std::max(0.0F, viewportHeight - 32.0F));
    const float x0 = (viewportWidth - width) * 0.5F;
    const float y0 = (viewportHeight - height) * 0.5F;
    const float x1 = x0 + width;
    const float innerWidth = std::max(0.0F, width - kPadding * 2.0F);

    layout.panel = Rect{x0, y0, x1, y0 + height};

    const float blend = std::clamp(
        (height - kCompactHeight) / (kPanelHeight - kCompactHeight), 0.0F, 1.0F);
    const auto vertical = [blend, y0](float compact, float full) {
        return y0 + compact + (full - compact) * blend;
    };

    const float chipWidth = (innerWidth - kChipGap * 2.0F) / 3.0F;
    const float primaryWidth = (innerWidth - kPrimaryGap * 7.0F) / 8.0F;
    const float controlGap = 18.0F;
    const float controlWidth = (innerWidth - controlGap) * 0.5F;
    const float speedX = x0 + kPadding;
    const float zoomX = speedX + controlWidth + controlGap;
    const float speedButtonWidth = (controlWidth - kRowGap * 2.0F) / 3.0F;
    const float zoomButtonWidth = (controlWidth - kRowGap * 3.0F) / 4.0F;
    const float toggleGap = 12.0F;
    const float toggleWidth = (innerWidth - toggleGap) * 0.5F;
    const float toggleX0 = x0 + kPadding;
    const float toggleX1 = toggleX0 + toggleWidth + toggleGap;
    const float cursorButtonWidth = (innerWidth - kRowGap * 3.0F) / 4.0F;

    layout.title = Rect{x0 + kPadding, vertical(2.0F, 16.0F), x1 - kPadding,
                        vertical(12.0F, 36.0F)};

    layout.themeLabel = Rect{x0 + kPadding, vertical(14.0F, 48.0F), x1 - kPadding,
                             vertical(23.0F, 62.0F)};
    for (std::size_t index = 0; index < layout.themeChips.size(); ++index) {
        const float chipX = x0 + kPadding +
                            static_cast<float>(index) * (chipWidth + kChipGap);
        layout.themeChips[index] =
            Rect{chipX, vertical(25.0F, 68.0F), chipX + chipWidth,
                 vertical(43.0F, 116.0F)};
    }

    layout.primaryLabel = Rect{x0 + kPadding, vertical(46.0F, 120.0F),
                               x1 - kPadding, vertical(55.0F, 134.0F)};
    for (std::size_t index = 0; index < layout.primarySwatches.size(); ++index) {
        const float swatchX =
            x0 + kPadding + static_cast<float>(index) * (primaryWidth + kPrimaryGap);
        layout.primarySwatches[index] =
            Rect{swatchX, vertical(57.0F, 138.0F), swatchX + primaryWidth,
                 vertical(71.0F, 162.0F)};
    }

    layout.speedLabel = Rect{speedX, vertical(74.0F, 174.0F),
                             speedX + controlWidth, vertical(83.0F, 188.0F)};
    layout.zoomLabel = Rect{zoomX, vertical(74.0F, 174.0F), zoomX + controlWidth,
                            vertical(83.0F, 188.0F)};
    for (std::size_t index = 0; index < layout.speedButtons.size(); ++index) {
        const float buttonX =
            speedX + static_cast<float>(index) * (speedButtonWidth + kRowGap);
        layout.speedButtons[index] =
            Rect{buttonX, vertical(85.0F, 192.0F), buttonX + speedButtonWidth,
                 vertical(103.0F, 220.0F)};
    }
    for (std::size_t index = 0; index < layout.zoomButtons.size(); ++index) {
        const float buttonX =
            zoomX + static_cast<float>(index) * (zoomButtonWidth + kRowGap);
        layout.zoomButtons[index] =
            Rect{buttonX, vertical(85.0F, 192.0F), buttonX + zoomButtonWidth,
                 vertical(103.0F, 220.0F)};
    }

    layout.editorLabel = Rect{x0 + kPadding, vertical(106.0F, 232.0F),
                              x1 - kPadding, vertical(115.0F, 246.0F)};
    layout.syntaxErrors = Rect{toggleX0, vertical(117.0F, 250.0F),
                               toggleX0 + toggleWidth, vertical(135.0F, 276.0F)};
    layout.lineNumbers = Rect{toggleX1, vertical(117.0F, 250.0F),
                              toggleX1 + toggleWidth, vertical(135.0F, 276.0F)};
    layout.highlightCurrentLine =
        Rect{toggleX0, vertical(137.0F, 282.0F), toggleX0 + toggleWidth,
             vertical(155.0F, 308.0F)};
    layout.cursorBlink = Rect{toggleX1, vertical(137.0F, 282.0F),
                              toggleX1 + toggleWidth, vertical(155.0F, 308.0F)};
    layout.cursorLabel = Rect{x0 + kPadding, vertical(158.0F, 320.0F),
                              x1 - kPadding, vertical(167.0F, 334.0F)};
    for (std::size_t index = 0; index < layout.cursorButtons.size(); ++index) {
        const float buttonX =
            x0 + kPadding +
            static_cast<float>(index) * (cursorButtonWidth + kRowGap);
        layout.cursorButtons[index] =
            Rect{buttonX, vertical(169.0F, 338.0F),
                 buttonX + cursorButtonWidth, vertical(187.0F, 366.0F)};
    }

    if (height >= 300.0F) {
        layout.iconLabel = Rect{x0 + kPadding, vertical(188.0F, 375.0F), x1 - kPadding,
                                vertical(197.0F, 389.0F)};
        for (std::size_t i = 0; i < layout.iconButtons.size(); ++i) {
            const float buttonX = x0 + kPadding + static_cast<float>(i) * (chipWidth + kChipGap);
            layout.iconButtons[i] = Rect{buttonX, vertical(201.0F, 397.0F), buttonX + chipWidth,
                                        vertical(218.0F, 425.0F)};
        }
    } else {
        const float buttonWidth = std::min(72.0F, std::max(36.0F, (innerWidth - 96.0F - kChipGap * 2.0F) / 3.0F));
        const float start = x1 - kPadding - (buttonWidth * 3.0F + kChipGap * 2.0F);
        for (std::size_t i = 0; i < layout.iconButtons.size(); ++i) {
            const float left = start + static_cast<float>(i) * (buttonWidth + kChipGap);
            layout.iconButtons[i] = Rect{left, y0 + 1.0F, left + buttonWidth, y0 + 13.0F};
        }
    }

    const float footerHeight = 20.0F + 8.0F * blend;
    const float footerBottom = y0 + height - (6.0F + 8.0F * blend);
    const float footerY = footerBottom - footerHeight;
    layout.openSettingsJson =
        Rect{x0 + kPadding, footerY, x0 + kPadding + 128.0F, footerBottom};
    layout.done =
        Rect{x1 - kPadding - 84.0F, footerY, x1 - kPadding, footerBottom};

    const float motionGap = 8.0F;
    layout.aiSettings = Rect{layout.openSettingsJson.x1 + motionGap, footerY, layout.openSettingsJson.x1 + motionGap + 96.0F, footerBottom};
    const float motionX0 = layout.aiSettings.x1 + motionGap;
    const float motionX1 = layout.done.x0 - motionGap;
    if (motionX1 > motionX0) {
        layout.backgroundMotion = Rect{motionX0, footerY, motionX1, footerBottom};
    }
    return layout;
}

SettingsControl settingsControlAt(const SettingsPanelLayout& layout, double x,
                                  double y) noexcept {
    const auto hit = [x, y](const Rect& rect) { return rect.contains(x, y); };
    if (hit(layout.done)) {
        return SettingsControl::Done;
    }
    if (hit(layout.aiSettings)) return SettingsControl::AiSettings;
    if (hit(layout.openSettingsJson)) {
        return SettingsControl::OpenSettingsJson;
    }
    if (hit(layout.backgroundMotion)) {
        return SettingsControl::BackgroundMotion;
    }
    for (std::size_t index = 0; index < layout.iconButtons.size(); ++index) {
        if (hit(layout.iconButtons[index])) {
            static constexpr std::array<SettingsControl, 3> kControls{
                SettingsControl::IconClassic, SettingsControl::IconAccent, SettingsControl::IconOutline};
            return kControls[index];
        }
    }
    for (std::size_t index = 0; index < layout.themeChips.size(); ++index) {
        if (hit(layout.themeChips[index])) {
            static constexpr std::array<SettingsControl, 3> kControls{
                SettingsControl::ThemeLucidDark, SettingsControl::ThemeGraphite,
                SettingsControl::ThemeLucidLight};
            return kControls[index];
        }
    }
    for (std::size_t index = 0; index < layout.primarySwatches.size(); ++index) {
        if (hit(layout.primarySwatches[index])) {
            static constexpr std::array<SettingsControl, 8> kControls{
                SettingsControl::PrimaryColor0, SettingsControl::PrimaryColor1,
                SettingsControl::PrimaryColor2, SettingsControl::PrimaryColor3,
                SettingsControl::PrimaryColor4, SettingsControl::PrimaryColor5,
                SettingsControl::PrimaryColor6, SettingsControl::PrimaryColor7};
            return kControls[index];
        }
    }
    for (std::size_t index = 0; index < layout.speedButtons.size(); ++index) {
        if (hit(layout.speedButtons[index])) {
            static constexpr std::array<SettingsControl, 3> kControls{
                SettingsControl::SpeedHalf, SettingsControl::SpeedNormal,
                SettingsControl::SpeedDouble};
            return kControls[index];
        }
    }
    for (std::size_t index = 0; index < layout.zoomButtons.size(); ++index) {
        if (hit(layout.zoomButtons[index])) {
            static constexpr std::array<SettingsControl, 4> kControls{
                SettingsControl::Zoom75, SettingsControl::Zoom100,
                SettingsControl::Zoom125, SettingsControl::Zoom150};
            return kControls[index];
        }
    }
    if (hit(layout.syntaxErrors)) {
        return SettingsControl::ShowSyntaxErrors;
    }
    if (hit(layout.lineNumbers)) {
        return SettingsControl::ShowLineNumbers;
    }
    if (hit(layout.highlightCurrentLine)) {
        return SettingsControl::HighlightCurrentLine;
    }
    if (hit(layout.cursorBlink)) {
        return SettingsControl::CursorBlink;
    }
    for (std::size_t index = 0; index < layout.cursorButtons.size(); ++index) {
        if (hit(layout.cursorButtons[index])) {
            static constexpr std::array<SettingsControl, 4> kControls{
                SettingsControl::CursorLine, SettingsControl::CursorBlock,
                SettingsControl::CursorUnderscore, SettingsControl::CursorKitty};
            return kControls[index];
        }
    }
    return SettingsControl::None;
}

CompileTerminalLayout compileTerminalLayout(float viewportWidth,
                                            float viewportHeight,
                                            float anchorLineTop) noexcept {
    const float margin = theme::kSearchBarMargin;
    const float panelWidth = std::min(
        theme::kCompileTerminalWidth,
        std::max(0.0F, viewportWidth - margin * 2.0F));
    const float availableHeight =
        std::max(0.0F, viewportHeight - margin * 2.0F);
    const float panelHeight =
        std::min(theme::kCompileTerminalHeight, availableHeight);

    CompileTerminalLayout layout;
    const float x1 = viewportWidth - margin;
    const float y0 = std::clamp(anchorLineTop - 8.0F, margin,
                                std::max(margin, viewportHeight - margin - panelHeight));
    layout.panel = Rect{x1 - panelWidth, y0, x1, y0 + panelHeight};

    layout.titleBar = Rect{layout.panel.x0, layout.panel.y0, layout.panel.x1,
                           layout.panel.y0 + theme::kCompileTerminalTitleHeight};

    const float dot = 10.0F;
    const float dotGap = 7.0F;
    const float dotsLeft = layout.panel.x0 + 14.0F;
    const float dotsCenterY =
        layout.panel.y0 + theme::kCompileTerminalTitleHeight * 0.5F;
    layout.trafficLights =
        Rect{dotsLeft, dotsCenterY - dot * 0.5F,
             dotsLeft + dot * 3.0F + dotGap * 2.0F, dotsCenterY + dot * 0.5F};
    layout.closeButton =
        Rect{dotsLeft - 4.0F, dotsCenterY - dot * 0.5F - 4.0F,
             dotsLeft + dot + 4.0F, dotsCenterY + dot * 0.5F + 4.0F};

    const float padding = theme::kCompileTerminalPadding;
    const float inputHeight = 24.0F;
    const float titleBottom =
        layout.panel.y0 + theme::kCompileTerminalTitleHeight;
    layout.output = Rect{layout.panel.x0 + padding, titleBottom,
                         layout.panel.x1 - padding,
                         layout.panel.y1 - padding - inputHeight};
    layout.input = Rect{layout.panel.x0 + padding,
                        layout.panel.y1 - padding - inputHeight,
                        layout.panel.x1 - padding,
                        layout.panel.y1 - padding};
    return layout;
}

DiagnosticCardLayout diagnosticCardLayoutAt(float viewportWidth,
                                            float viewportHeight, float anchorX,
                                            float lineTop,
                                            float lineBottom) noexcept {
    const float margin = theme::kSearchBarMargin;
    const float width =
        std::min(kDiagnosticWidth, std::max(0.0F, viewportWidth - margin * 2.0F));

    DiagnosticCardLayout layout;
    const float x0 =
        std::clamp(anchorX - 24.0F, margin, viewportWidth - margin - width);
    const float below = lineBottom + 8.0F;
    const float above = lineTop - 8.0F - kDiagnosticHeight;

    if (below + kDiagnosticHeight <= viewportHeight - margin) {
        layout.card = Rect{x0, below, x0 + width, below + kDiagnosticHeight};
        layout.anchored = true;
        const float connectorX =
            std::clamp(anchorX, layout.card.x0 + 18.0F, layout.card.x1 - 18.0F);
        layout.connector =
            Rect{connectorX - 1.0F, lineBottom, connectorX + 1.0F, layout.card.y0};
    } else if (above >= margin) {
        layout.card = Rect{x0, above, x0 + width, above + kDiagnosticHeight};
        layout.anchored = true;
        const float connectorX =
            std::clamp(anchorX, layout.card.x0 + 18.0F, layout.card.x1 - 18.0F);
        layout.connector =
            Rect{connectorX - 1.0F, layout.card.y1, connectorX + 1.0F, lineTop};
    } else {
        layout.card = centeredCard(viewportWidth, width);
    }

    layout.ignoreButton =
        Rect{layout.card.x1 - 28.0F, layout.card.y0 + 8.0F,
             layout.card.x1 - 8.0F, layout.card.y0 + 28.0F};
    return layout;
}

DiagnosticCardLayout offsetDiagnosticCard(DiagnosticCardLayout layout, float offsetX,
                                          float offsetY) noexcept {
    const auto move = [offsetX, offsetY](Rect& rect) {
        rect.x0 += offsetX;
        rect.x1 += offsetX;
        rect.y0 += offsetY;
        rect.y1 += offsetY;
    };
    move(layout.card);
    move(layout.ignoreButton);
    move(layout.connector);
    return layout;
}

CompileTerminalLayout offsetCompileTerminal(CompileTerminalLayout layout, float offsetX,
                                            float offsetY) noexcept {
    const auto move = [offsetX, offsetY](Rect& rect) {
        rect.x0 += offsetX;
        rect.x1 += offsetX;
        rect.y0 += offsetY;
        rect.y1 += offsetY;
    };
    move(layout.panel);
    move(layout.titleBar);
    move(layout.trafficLights);
    move(layout.closeButton);
    move(layout.output);
    move(layout.input);
    return layout;
}

} // namespace lucid::ui
