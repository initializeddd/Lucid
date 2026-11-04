#pragma once

#include "ui/UiState.hpp"

#include <array>

namespace lucid::ui {

struct Rect {
    float x0 = 0.0F;
    float y0 = 0.0F;
    float x1 = 0.0F;
    float y1 = 0.0F;

    [[nodiscard]] bool contains(double x, double y) const noexcept {
        return x >= static_cast<double>(x0) && x < static_cast<double>(x1) &&
               y >= static_cast<double>(y0) && y < static_cast<double>(y1);
    }
};

struct SettingsErrorLayout {
    Rect text;
};

struct DiagnosticCardLayout {
    Rect card;
    Rect ignoreButton;
    Rect connector;
    bool anchored = false;
};

struct CompileTerminalLayout {
    Rect panel;
    Rect titleBar;
    Rect trafficLights;
    Rect closeButton;
    Rect output;
    Rect input;
};

struct CompletionListLayout {
    float y = 0.0F;
    float height = 0.0F;
    std::size_t first = 0;
    std::size_t count = 0;
};

[[nodiscard]] CompletionListLayout completionListLayout(float editorTop, float editorBottom,
    float cursorTop, float lineHeight, float padding, std::size_t items, std::size_t selected) noexcept;

struct SettingsPanelLayout {
    Rect panel;
    Rect title;
    Rect themeLabel;
    std::array<Rect, 3> themeChips{};
    Rect primaryLabel;
    std::array<Rect, 8> primarySwatches{};
    Rect speedLabel;
    std::array<Rect, 3> speedButtons{};
    Rect zoomLabel;
    std::array<Rect, 4> zoomButtons{};
    Rect editorLabel;
    Rect syntaxErrors;
    Rect lineNumbers;
    Rect highlightCurrentLine;
    Rect cursorBlink;
    Rect cursorLabel;
    Rect iconLabel;
    std::array<Rect, 3> iconButtons{};
    std::array<Rect, 4> cursorButtons{};
    Rect backgroundMotion;
    Rect openSettingsJson;
    Rect aiSettings;
    Rect done;
};

[[nodiscard]] SettingsControl
settingsControlAt(const SettingsPanelLayout& layout, double x, double y) noexcept;

[[nodiscard]] SettingsErrorLayout settingsErrorLayout(float viewportWidth,
                                                      float viewportHeight) noexcept;
[[nodiscard]] DiagnosticCardLayout diagnosticCardLayout(float viewportWidth,
                                                        float viewportHeight) noexcept;
[[nodiscard]] DiagnosticCardLayout
diagnosticCardLayoutAt(float viewportWidth, float viewportHeight, float anchorX,
                       float lineTop, float lineBottom) noexcept;
[[nodiscard]] CompileTerminalLayout
compileTerminalLayout(float viewportWidth, float viewportHeight,
                      float anchorLineTop) noexcept;
[[nodiscard]] SettingsPanelLayout
settingsPanelLayout(float viewportWidth, float viewportHeight) noexcept;

[[nodiscard]] float commandBarTop(float viewportHeight) noexcept;
[[nodiscard]] float commandSuggestionGap(float viewportHeight) noexcept;

[[nodiscard]] DiagnosticCardLayout offsetDiagnosticCard(DiagnosticCardLayout layout,
                                                        float offsetX,
                                                        float offsetY) noexcept;
[[nodiscard]] CompileTerminalLayout offsetCompileTerminal(CompileTerminalLayout layout,
                                                          float offsetX,
                                                          float offsetY) noexcept;

} // namespace lucid::ui
