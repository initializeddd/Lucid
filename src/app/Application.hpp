#pragma once

#include "app/Settings.hpp"
#include "editor/compile/CompileScript.hpp"
#include "editor/diagnostics/CompilerDiagnostics.hpp"
#include "editor/diagnostics/Diagnostics.hpp"
#include "editor/document/Document.hpp"
#include "editor/syntax/Syntax.hpp"
#include "input/KeyCodes.hpp"
#include "platform/app/PlatformApp.hpp"
#include "platform/terminal/PtyProcess.hpp"
#include "platform/window/Window.hpp"
#include "renderer/Renderer.hpp"
#include "ui/Overlay.hpp"
#include "ui/UiState.hpp"
#include "ui/search/SearchBar.hpp"
#include "workspace/FunctionIndex.hpp"
#include "workspace/Project.hpp"

#include <cstdint>
#include <future>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lucid {

struct ApplicationConfig {
    WindowConfig window{};
    RendererConfig renderer{};
};

class Application {
public:
    explicit Application(ApplicationConfig config = {});
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    void run();

private:
    void handleKey(const input::KeyEvent& event);
    void handleCommandKey(const input::KeyEvent& event, bool commandDown);
    void handleEditorKey(const input::KeyEvent& event, bool commandDown);
    void handleChar(char32_t codepoint);
    void undoDocument();
    void redoDocument();
    void cutSelection();
    void copySelection();
    void pasteClipboard();
    void selectAllText();
    void handleMouseButton(const input::MouseButtonEvent& event);
    void handleMouseMove(const input::MouseMoveEvent& event);
    void handleScroll(const input::ScrollEvent& event);

    void openFilePrompt();
    void openProjectPrompt();
    void openFunctionPrompt();
    void applyCommand(bool forceFolder);
    void createFolder(const std::string& path);
    void openProject(const std::string& path);
    void openFile(const std::string& path);
    [[nodiscard]] bool saveDocument();
    void closeDocument();
    void deleteCurrentFile();
    void deleteCurrentProject();
    void closeProject();

    void moveSuggestion(int delta);
    void acceptSuggestion();
    void refreshSuggestions();
    void onCommandQueryChanged();
    void clearCommandFeedback();
    void refreshHighlight();
    void refreshDiagnostics(double timeSeconds);
    void pollCompilerDiagnostics(double timeSeconds);
    void updateActiveDiagnostic();
    void resetDiagnostics();
    void initializeSettings();
    void openSettings();
    void openSettingsJson();
    void closeSettingsPanel();
    void handleSettingsPanelClick(double x, double y);
    void updateSettingsPanelHover(double x, double y);
    void applySettings(const EditorSettings& settings);
    void applyTheme(const ThemeSettings& theme);
    void persistSettings();
    void refreshSettings();
    void loadSettingsFromDisk();
    void ensureSettingsFile();
    [[nodiscard]] bool isJsonDocument() const;
    [[nodiscard]] bool isCodeDocument() const;
    [[nodiscard]] bool isSettingsDocument() const;
    [[nodiscard]] std::string absoluteDocumentPath() const;
    [[nodiscard]] ui::SettingsErrorLayout settingsErrorLayout() const;
    [[nodiscard]] ui::SettingsPanelLayout settingsPanelLayout() const;
    [[nodiscard]] ui::SettingsControl settingsControlAt(double x, double y) const;
    [[nodiscard]] ui::DiagnosticCardLayout diagnosticCardLayout() const;
    [[nodiscard]] float textWidth(const std::u32string& line, std::size_t column) const;
    void uiSize(int& width, int& height) const;

    void updateVisibleLines();
    void updateSuggestionAnimation(double deltaSeconds);
    void updateCompileTerminalAnimation(double deltaSeconds);
    void refreshCompileScript();
    void runCompileAtCursor();
    void runCompileFunction(editor::CompileFunction function,
                            std::string sourcePath = {});
    void runProjectFunction(const workspace::ProjectFunction& function);
    void ensureFunctionIndex();
    void invalidateFunctionIndex();
    [[nodiscard]] bool ensureCompileTerminalWindow();
    void destroyCompileTerminalWindow();
    void closeCompileTerminal();
    void focusCompileTerminal(bool focused);
    void handleCompileTerminalKey(const input::KeyEvent& event);
    void handleCompileTerminalChar(char32_t codepoint);
    void handleCompileTerminalMouseButton(const input::MouseButtonEvent& event);
    void handleCompileTerminalMouseMove(const input::MouseMoveEvent& event);
    [[nodiscard]] ui::FrameState
    makeCompileTerminalFrameState(double timeSeconds) const;
    void updateCompileTerminalHover(double x, double y);
    void beginDrag(double x, double y, bool terminal);
    void updateDrag(double x, double y);
    void endDrag();
    [[nodiscard]] const editor::CompileFunction* compileFunctionAt(double x,
                                                                   double y) const;
    [[nodiscard]] ui::CompileTerminalLayout compileTerminalLayout() const;
    [[nodiscard]] ui::CompileTerminalLayout compileTerminalBaseLayout() const;
    [[nodiscard]] bool placeCaretAt(double x, double y, bool extendSelection);
    void updateCursorShape(double x, double y);
    [[nodiscard]] int visibleRowCount() const;
    [[nodiscard]] int maxFirstLine() const;
    [[nodiscard]] float suggestionListTop() const;
    [[nodiscard]] int suggestionIndexAt(double x, double y) const;
    [[nodiscard]] bool commandRegionContains(double x, double y) const;
    [[nodiscard]] bool welcomeActionContains(double x, double y) const;
    [[nodiscard]] bool titlebarDragContains(double x, double y) const;
    [[nodiscard]] ui::FrameState makeFrameState(double timeSeconds);
    [[nodiscard]] std::string commandPath() const;

    void setCommandStatus(std::string message, bool error);
    void setStatus(std::string message, bool error);

    ApplicationConfig m_config;
    ui::SearchBar m_searchBar;
    Window m_window;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Window> m_compileTerminalWindow;
    std::unique_ptr<Renderer> m_compileTerminalRenderer;

    workspace::Project m_project;
    bool m_hasProject = false;
    std::string m_projectName;

    editor::Document m_document;
    bool m_hasDocument = false;
    std::string m_filePath;
    int m_firstLine = 0;
    bool m_followCursor = true;
    bool m_welcomeActionHovered = false;
    bool m_mouseSelecting = false;
    float m_charAdvance = 7.0F;

    float m_animationSpeed = 1.0F;
    bool m_showDiagnostics = true;
    bool m_showLineNumbers = true;
    bool m_highlightCurrentLine = true;
    bool m_cursorBlink = true;
    editor::CursorStyle m_cursorStyle = editor::CursorStyle::Line;
    bool m_backgroundMotion = true;
    float m_backgroundDim = 0.0F;
    float m_uiZoom = 1.0F;
    EditorSettings m_settings;

    std::vector<std::string> m_recentProjects;
    std::vector<std::string> m_suggestionPaths;
    std::vector<std::string> m_suggestionDetails;
    std::vector<std::size_t> m_suggestionFunctionIndices;
    std::vector<ui::PathSuggestion> m_suggestions;
    int m_selectedSuggestion = -1;
    double m_suggestionEpoch = 0.0;
    double m_lastTimeSeconds = 0.0;
    double m_animationTimeSeconds = 0.0;
    float m_suggestionSelectionRow = -1.0F;
    float m_suggestionSelectionOpacity = 0.0F;
    float m_suggestionSelectionRowVelocity = 0.0F;
    float m_suggestionSelectionOpacityVelocity = 0.0F;

    std::string m_commandStatus;
    bool m_commandStatusIsError = false;
    std::string m_statusText;
    bool m_statusIsError = false;

    std::vector<std::u32string_view> m_visibleLines;
    std::vector<std::vector<editor::TokenSpan>> m_syntax;
    std::vector<std::span<const editor::TokenSpan>> m_visibleTokens;
    std::uint64_t m_syntaxRevision = 0;
    bool m_syntaxValid = false;

    editor::CompileScript m_compileScript;
    std::uint64_t m_compileScriptRevision = 0;
    bool m_compileScriptValid = false;
    platform::PtyProcess m_compileTerminalProcess;
    bool m_compileTerminalVisible = false;
    bool m_compileTerminalFocused = false;
    bool m_compileTerminalCloseHovered = false;
    std::string m_compileTerminalTitle;
    std::string m_compileTerminalInput;
    int m_compileTerminalLine = 0;
    float m_compileTerminalReveal = 0.0F;
    float m_compileTerminalVelocity = 0.0F;
    float m_compileTerminalOffsetX = 0.0F;
    float m_compileTerminalOffsetY = 0.0F;
    bool m_compileTerminalCloseRequested = false;

    std::string m_windowTitle = "Lucid";
    std::string m_settingsRoot;
    std::string m_settingsError;
    bool m_settingsErrorHovered = false;
    bool m_settingsPanelVisible = false;
    ui::SettingsControl m_settingsHovered = ui::SettingsControl::None;
    std::uint64_t m_settingsRevision = ~std::uint64_t{0};

    std::vector<editor::Diagnostic> m_diagnostics;
    std::uint64_t m_diagnosticsRevision = 0;
    bool m_diagnosticsValid = false;
    std::set<std::string> m_ignoredDiagnostics;
    std::string m_activeDiagnosticKey;
    std::string m_activeDiagnosticMessage;
    std::string m_activeDiagnosticCode;
    std::string m_diagnosticLabel = "C++ ERROR";
    int m_activeDiagnosticLine = 0;
    int m_activeDiagnosticColumn = 0;
    int m_activeDiagnosticLength = 1;
    bool m_diagnosticIgnoreHovered = false;
    float m_diagnosticOffsetX = 0.0F;
    float m_diagnosticOffsetY = 0.0F;

    std::future<editor::CompilerResult> m_compilerFuture;
    bool m_compilerRunning = false;
    std::uint64_t m_compilerRunningRevision = 0;
    std::string m_compilerRunningPath;
    std::uint64_t m_compilerAppliedRevision = 0;
    std::string m_compilerAppliedPath;
    double m_compilerDueTime = 0.0;

    workspace::FunctionIndex m_functionIndex;
    bool m_functionIndexValid = false;
    bool m_functionIndexDirty = true;

    enum class DragTarget {
        None,
        CompileTerminal,
        DiagnosticCard,
    };
    DragTarget m_dragTarget = DragTarget::None;
    double m_dragStartX = 0.0;
    double m_dragStartY = 0.0;
    float m_dragStartOffsetX = 0.0F;
    float m_dragStartOffsetY = 0.0F;
};

} // namespace lucid
