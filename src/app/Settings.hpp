#pragma once

#include "editor/cursor/CursorStyle.hpp"
#include "ui/theme/Theme.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace lucid {

struct ThemeSettings {
    std::string name = "lucid-dark";
    std::optional<ui::theme::Color> primary;
};

struct EditorSettings {
    float animationSpeed = 1.0F;
    bool showSyntaxErrors = true;
    bool showLineNumbers = true;
    bool highlightCurrentLine = true;
    bool cursorBlink = true;
    editor::CursorStyle cursorStyle = editor::CursorStyle::Line;
    bool backgroundMotion = true;
    int backgroundScene = 0;
    bool highContrast = false;
    float zoomFactor = 1.0F;
    std::string iconTheme = "classic";
    std::string taskIdentity;
    std::string aiAskProvider = "local", aiCleanupProvider = "local", aiDebugProvider = "local";
    std::string aiAskModel, aiCleanupModel, aiDebugModel;
    ThemeSettings theme{};
};

struct SettingsResult {
    EditorSettings settings{};
    bool valid = false;
    std::string error;
};

[[nodiscard]] SettingsResult parseSettings(std::string_view text);
[[nodiscard]] std::string defaultSettingsJson();
[[nodiscard]] std::string serializeSettings(const EditorSettings& settings);
[[nodiscard]] std::filesystem::path settingsDirectory();

} // namespace lucid
