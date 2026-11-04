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
    Ai,
    AiSettings,
    SettingsMenu,
    SessionRestore,
    ProjectSearch,
    ProjectSearchResults,
    ProjectReplace,
    ProjectReplaceReview,
    LanguageLocations,
    Diagnostics,
    AiModelInput,
    AiKeyInput,
    Find,
    Replace,
    OpenFiles,
    RecoveryList,
    RecoveryDetail,
    ModuleList,
    ModuleActions,
    ModuleCommands,
    ModuleImport,
    ModulePreview,
    ModuleCatalog,
    DebugMenu,
    DebugInput,
    DebugStack,
    DebugThreads,
    DebugVariables,
    DebugOutput,
    GitChanges,
    GitFile,
    GitDiff,
    GitCommitReview,
    GitCommitInput,
    GitRemoteReview,
    GitHistory,
    TaskList,
    TaskActions,
    TaskSubtasks,
    TaskHistory,
    TaskTier,
    TaskInput,
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
    HighContrast,
    CursorBlink,
    CursorLine,
    CursorBlock,
    CursorUnderscore,
    CursorKitty,
    IconClassic,
    IconAccent,
    IconOutline,
    PrimaryColor0,
    PrimaryColor1,
    PrimaryColor2,
    PrimaryColor3,
    PrimaryColor4,
    PrimaryColor5,
    PrimaryColor6,
    PrimaryColor7,
    BackgroundMotion,
    BackgroundScene,
    BackgroundColorMode,
    StartupSession,
    OpenSettingsJson,
    AiSettings,
    Done,
};

enum class SuggestionKind {
    File,
    Directory,
    CreateFile,
    CreateDirectory,
    Project,
    Function,
    Task,
    ReadOnlyText,
};

struct PathSuggestion {
    std::string_view path;
    SuggestionKind kind = SuggestionKind::File;
    std::string_view detail;
};

struct FrameState {
    bool commandVisible = false;
    std::string_view commandQuery;
    std::size_t commandCursor = 0;
    std::size_t commandSelectionStart = 0;
    std::size_t commandSelectionEnd = 0;
    std::string_view commandPlaceholder;
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
    float suggestionMoreAbove = 0.0F;
    float suggestionMoreBelow = 0.0F;

    bool showWelcome = true;
    bool hasProject = false;
    bool welcomeActionHovered = false;
    int welcomeShortcutScroll = 0;
    bool welcomeShortcutsExpanded = false;
    float welcomeShortcutProgress = 0.0F;
    float startupProgress = 1.0F;
    std::string_view projectName;

    bool settingsErrorVisible = false;
    bool settingsErrorHovered = false;
    std::string_view settingsErrorText;

    bool settingsPanelVisible = false;
    SettingsControl settingsHovered = SettingsControl::None;
    std::string_view settingsTheme;
    std::string_view settingsIconTheme;
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
    std::string_view statusPosition;
    float statusPositionReveal = 0.0F;
    std::string_view filePath;
    bool dirty = false;
    bool showLineNumbers = true;
    bool highlightCurrentLine = true;
    bool cursorBlink = true;
    editor::CursorStyle cursorStyle = editor::CursorStyle::Line;
    bool backgroundMotion = true;
    int backgroundScene = 0;
    bool backgroundNaturalColors = false;
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
    std::vector<int> debugBreakpointLines;
    int debugStoppedLine = -1;
    std::string_view debugInline;

    bool compileTerminalVisible = false;
    bool compileTerminalStandalone = false;
    bool compileTerminalFocused = false;
    bool compileTerminalRunning = false;
    bool compileTerminalFailed = false;
    bool compileTerminalCloseHovered = false;
    float compileTerminalOpacity = 0.0F;
    float compileTerminalReveal = 0.0F;
    float compileTerminalOffsetX = 0.0F;
    float compileTerminalOffsetY = 0.0F;
    int compileTerminalLine = 0;
    int compileOutputScroll = 0;
    std::string_view compileTerminalTitle;
    std::string_view compileTerminalOutput;
    std::size_t compileSelectionStart = 0;
    std::size_t compileSelectionEnd = 0;
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

    bool completionVisible = false;
    std::vector<std::string_view> completions;
    int selectedCompletion = 0;
    std::vector<PathSuggestion> suggestions;
    int selectedSuggestion = -1;

    std::string_view commandStatus;
    bool commandStatusIsError = false;
    std::size_t commandStatusColumns = 1;
    std::size_t commandStatusRows = 0;
    std::string_view statusText;
    bool statusIsError = false;
};

} // namespace lucid::ui
