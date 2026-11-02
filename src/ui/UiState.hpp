#pragma once

#include "editor/cursor/CursorStyle.hpp"
#include "editor/syntax/Syntax.hpp"
#include "ui/theme/Theme.hpp"

#include <span>
#include <string_view>
#include <vector>

namespace lucid::ui {

enum class PromptKind {
    Project,
    File,
    Function,
};

enum class SettingsControl {
    None,
    ThemeLucidDark,
    ThemeGraphite,
    ThemeLucidLight,
    SpeedHalf,
    SpeedNormal,
    SpeedDouble,
    Zoom75,
    Zoom100,
    Zoom125,
    Zoom150,
    ShowSyntaxErrors,
    ShowLineNumbers,
    HighlightCurrentLine,
    CursorBlink,
    CursorLine,
    CursorBlock,
    CursorUnderscore,
    CursorKitty,
    PrimaryColor0,
    PrimaryColor1,
    PrimaryColor2,
    PrimaryColor3,
    PrimaryColor4,
    PrimaryColor5,
    PrimaryColor6,
    PrimaryColor7,
    BackgroundMotion,
    OpenSettingsJson,
    Done,
};

enum class SuggestionKind {
    File,
    Directory,
    CreateFile,
    CreateDirectory,
    Project,
    Function,
};

struct PathSuggestion {
    std::string_view path;
    SuggestionKind kind = SuggestionKind::File;
    std::string_view detail;
};

struct FrameState {
    bool commandVisible = false;
    std::string_view commandQuery;
    PromptKind commandKind = PromptKind::File;
    double timeSeconds = 0.0;
    float animationTime = 0.0F;
    float opacity = 1.0F;
    float scaleX = 1.0F;
    float scaleY = 1.0F;
    float offsetX = 0.0F;
    float blur = 0.0F;
    float animationSpeed = 1.0F;
    double suggestionEpoch = 0.0;
    float suggestionSelectionRow = -1.0F;
    float suggestionSelectionOpacity = 0.0F;

    bool showWelcome = true;
    bool hasProject = false;
    bool welcomeActionHovered = false;
    std::string_view projectName;

    bool settingsErrorVisible = false;
    bool settingsErrorHovered = false;
    std::string_view settingsErrorText;

    bool settingsPanelVisible = false;
    SettingsControl settingsHovered = SettingsControl::None;
    std::string_view settingsTheme;
    float settingsAnimationSpeed = 1.0F;
    float settingsZoomFactor = 1.0F;
    bool settingsShowSyntaxErrors = true;
    bool settingsShowLineNumbers = true;
    bool settingsHighlightCurrentLine = true;
    bool settingsCursorBlink = true;
    editor::CursorStyle settingsCursorStyle = editor::CursorStyle::Line;
    theme::Color settingsPrimary{};
    bool settingsBackgroundMotion = true;
    bool settingsFileAvailable = true;

    bool hasDocument = false;
    std::string_view filePath;
    bool dirty = false;
    bool showLineNumbers = true;
    bool highlightCurrentLine = true;
    bool cursorBlink = true;
    editor::CursorStyle cursorStyle = editor::CursorStyle::Line;
    bool backgroundMotion = true;
    float backgroundDim = 0.0F;
    bool showWindowTitle = false;
    std::string_view windowTitle;
    std::vector<std::u32string_view> visibleLines;
    std::vector<std::span<const editor::TokenSpan>> visibleTokens;
    int firstLine = 0;
    int cursorLine = 0;
    int cursorColumn = 0;
    int cursorRow = 0;
    bool hasSelection = false;
    int selectionStartLine = 0;
    int selectionStartColumn = 0;
    int selectionEndLine = 0;
    int selectionEndColumn = 0;

    std::vector<int> compileFunctionLines;

    bool compileTerminalVisible = false;
    bool compileTerminalStandalone = false;
    bool compileTerminalFocused = false;
    bool compileTerminalRunning = false;
    bool compileTerminalCloseHovered = false;
    float compileTerminalOpacity = 0.0F;
    float compileTerminalReveal = 0.0F;
    float compileTerminalOffsetX = 0.0F;
    float compileTerminalOffsetY = 0.0F;
    int compileTerminalLine = 0;
    std::string_view compileTerminalTitle;
    std::string_view compileTerminalOutput;
    std::string_view compileTerminalInput;

    bool diagnosticsVisible = false;
    std::string_view diagnosticMessage;
    std::string_view diagnosticCode;
    std::string_view diagnosticLabel;
    int diagnosticLine = 0;
    int diagnosticColumn = 0;
    int diagnosticLength = 1;
    bool diagnosticAnchorValid = false;
    float diagnosticAnchorX = 0.0F;
    float diagnosticLineTop = 0.0F;
    float diagnosticLineBottom = 0.0F;
    bool diagnosticIgnoreHovered = false;
    float diagnosticOffsetX = 0.0F;
    float diagnosticOffsetY = 0.0F;

    std::vector<PathSuggestion> suggestions;
    int selectedSuggestion = -1;

    std::string_view commandStatus;
    bool commandStatusIsError = false;
    std::string_view statusText;
    bool statusIsError = false;
};

} // namespace lucid::ui
