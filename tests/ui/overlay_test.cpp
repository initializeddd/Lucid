#include "ui/Overlay.hpp"

#include <cmath>

namespace {

using lucid::ui::commandBarTop;
using lucid::ui::compileTerminalLayout;
using lucid::ui::diagnosticCardLayout;
using lucid::ui::diagnosticCardLayoutAt;
using lucid::ui::offsetCompileTerminal;
using lucid::ui::settingsControlAt;
using lucid::ui::SettingsControl;
using lucid::ui::settingsErrorLayout;
using lucid::ui::settingsPanelLayout;

[[nodiscard]] bool near(float left, float right) {
    return std::abs(left - right) < 0.01F;
}

} // namespace

int main() {
    const auto error = settingsErrorLayout(1200.0F, 800.0F);
    if (!near(error.text.x0, 764.0F) || !near(error.text.x1, 1184.0F) ||
        !near(error.text.y0, 16.0F) || !near(error.text.y1, 38.0F)) {
        return 1;
    }

    const auto narrowError = settingsErrorLayout(320.0F, 240.0F);
    if (!near(narrowError.text.x0, 16.0F) ||
        !near(narrowError.text.x1, 304.0F)) {
        return 2;
    }

    if (!near(commandBarTop(800.0F), 44.0F) ||
        !near(commandBarTop(400.0F), 44.0F) ||
        !near(commandBarTop(2000.0F), 44.0F) ||
        !near(commandBarTop(40.0F), 16.0F)) {
        return 23;
    }

    const auto terminal = compileTerminalLayout(1200.0F, 800.0F, 200.0F);
    if (!near(terminal.panel.x0, 564.0F) ||
        !near(terminal.panel.x1, 1184.0F) ||
        !near(terminal.panel.y0, 192.0F) ||
        !near(terminal.panel.y1, 442.0F) ||
        !near(terminal.titleBar.y1, 230.0F) ||
        !near(terminal.trafficLights.x0, 578.0F) ||
        !near(terminal.trafficLights.x1, 622.0F) ||
        !near(terminal.output.y0, 230.0F) ||
        !near(terminal.input.y1, 428.0F) ||
        !terminal.trafficLights.contains(terminal.closeButton.x0 + 7.0,
                                         terminal.closeButton.y0 + 10.0)) {
        return 24;
    }
    const auto movedTerminal =
        offsetCompileTerminal(terminal, 10.0F, 20.0F);
    if (!near(movedTerminal.trafficLights.x0, 588.0F) ||
        !near(movedTerminal.trafficLights.y0, 226.0F) ||
        !near(movedTerminal.closeButton.x0, 584.0F)) {
        return 25;
    }

    const auto card = diagnosticCardLayout(1200.0F, 800.0F);
    if (!near(card.card.x1, 1184.0F) || !near(card.card.x0, 744.0F) ||
        !near(card.card.y0, 50.0F)) {
        return 3;
    }
    if (card.anchored) {
        return 4;
    }
    if (!card.card.contains(card.ignoreButton.x0 + 1.0,
                            card.ignoreButton.y0 + 1.0)) {
        return 5;
    }

    const auto anchored = diagnosticCardLayoutAt(1200.0F, 800.0F, 300.0F,
                                                  200.0F, 218.0F);
    if (!anchored.anchored || !near(anchored.card.x0, 276.0F) ||
        !near(anchored.card.y0, 226.0F) ||
        !near(anchored.connector.y0, 218.0F) ||
        !near(anchored.connector.y1, 226.0F)) {
        return 6;
    }

    const auto above = diagnosticCardLayoutAt(1200.0F, 800.0F, 600.0F,
                                               760.0F, 778.0F);
    if (!above.anchored || !near(above.card.y1, 752.0F) ||
        !near(above.connector.y0, 752.0F) ||
        !near(above.connector.y1, 760.0F)) {
        return 7;
    }

    const auto fallback = diagnosticCardLayoutAt(1200.0F, 100.0F, 300.0F,
                                                 20.0F, 38.0F);
    if (fallback.anchored || !near(fallback.card.y0, 50.0F)) {
        return 8;
    }

    const auto panel = settingsPanelLayout(1200.0F, 800.0F);
    if (!near(panel.panel.x0, 290.0F) || !near(panel.panel.x1, 910.0F) ||
        !near(panel.panel.y0, 150.0F) || !near(panel.panel.y1, 650.0F)) {
        return 9;
    }
    if (!near(panel.themeChips[0].x0, panel.panel.x0 + 16.0F) ||
        !near(panel.themeChips[2].x1, panel.panel.x1 - 16.0F) ||
        !near(panel.primarySwatches[0].x0, panel.panel.x0 + 16.0F) ||
        !near(panel.primarySwatches[7].x1, panel.panel.x1 - 16.0F)) {
        return 10;
    }
    if (settingsControlAt(panel, panel.done.x0 + 1.0, panel.done.y0 + 1.0) !=
        SettingsControl::Done) {
        return 11;
    }
    if (settingsControlAt(panel, panel.themeChips[1].x0 + 1.0,
                          panel.themeChips[1].y0 + 1.0) !=
        SettingsControl::ThemeGraphite) {
        return 12;
    }
    if (settingsControlAt(panel, panel.primarySwatches[2].x0 + 1.0,
                          panel.primarySwatches[2].y0 + 1.0) !=
        SettingsControl::PrimaryColor2) {
        return 13;
    }
    if (settingsControlAt(panel, panel.speedButtons[2].x0 + 1.0,
                          panel.speedButtons[2].y0 + 1.0) !=
        SettingsControl::SpeedDouble) {
        return 14;
    }
    if (settingsControlAt(panel, panel.zoomButtons[1].x0 + 1.0,
                          panel.zoomButtons[1].y0 + 1.0) !=
        SettingsControl::Zoom100) {
        return 15;
    }
    if (settingsControlAt(panel, panel.cursorBlink.x0 + 1.0,
                          panel.cursorBlink.y0 + 1.0) !=
        SettingsControl::CursorBlink) {
        return 16;
    }
    if (settingsControlAt(panel, panel.cursorButtons[1].x0 + 1.0,
                          panel.cursorButtons[1].y0 + 1.0) !=
        SettingsControl::CursorBlock) {
        return 17;
    }
    if (settingsControlAt(panel, panel.cursorButtons[3].x0 + 1.0,
                          panel.cursorButtons[3].y0 + 1.0) !=
        SettingsControl::CursorKitty) {
        return 18;
    }
    if (settingsControlAt(panel, panel.backgroundMotion.x0 + 1.0,
                          panel.backgroundMotion.y0 + 1.0) !=
            SettingsControl::BackgroundMotion ||
        panel.backgroundMotion.x0 <= panel.openSettingsJson.x1 ||
        panel.backgroundMotion.x1 >= panel.done.x0) {
        return 19;
    }
    if (panel.iconButtons[2].y1 >= panel.done.y0 ||
        settingsControlAt(panel, panel.iconButtons[2].x0 + 1.0, panel.iconButtons[2].y0 + 1.0) != SettingsControl::IconOutline) return 26;
    if (settingsControlAt(panel, 0.0, 0.0) != SettingsControl::None) {
        return 20;
    }

    const auto smallPanel = settingsPanelLayout(640.0F, 400.0F);
    if (!near(smallPanel.panel.y0, 16.0F) ||
        !near(smallPanel.panel.y1, 384.0F) ||
        !near(smallPanel.done.x1, smallPanel.panel.x1 - 16.0F)) {
        return 21;
    }

    const auto compactPanel = settingsPanelLayout(426.0F, 266.0F);
    if (!near(compactPanel.panel.x0, 16.0F) ||
        !near(compactPanel.panel.x1, 410.0F) ||
        !near(compactPanel.panel.y1, 250.0F) ||
        compactPanel.cursorBlink.y1 >= compactPanel.done.y0 ||
        settingsControlAt(compactPanel, compactPanel.iconButtons[1].x0 + 1.0,
                          compactPanel.iconButtons[1].y0 + 1.0) != SettingsControl::IconAccent ||
        compactPanel.cursorButtons[3].y1 >= compactPanel.done.y0 ||
        settingsControlAt(compactPanel, compactPanel.cursorBlink.x0 + 1.0,
                          compactPanel.cursorBlink.y0 + 1.0) !=
            SettingsControl::CursorBlink ||
        settingsControlAt(compactPanel, compactPanel.cursorButtons[3].x0 + 1.0,
                          compactPanel.cursorButtons[3].y0 + 1.0) !=
            SettingsControl::CursorKitty ||
        settingsControlAt(compactPanel, compactPanel.done.x0 + 1.0,
                          compactPanel.done.y0 + 1.0) != SettingsControl::Done) {
        return 22;
    }

    return 0;
}
