#pragma once

#include "app/Settings.hpp"
#include "debug/Session.hpp"
#include "debug/AdapterProcess.hpp"
#include "editor/lsp/Client.hpp"
#include "workspace/search/ProjectSearch.hpp"
#include "workspace/session/Session.hpp"
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
#include "workspace/tasks/Tasks.hpp"
#include "workspace/recovery/Snapshot.hpp"
#include "workspace/git/GitReview.hpp"
#include "modules/Modules.hpp"

#include <atomic>
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
    std::string initialPath;
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
    friend struct ApplicationInteractionTest;
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
    void openDocumentSwitcher();
    void openTasks();
    void openDebugger();
    void toggleBreakpoint();
    void pollDebugger();
    void stopDebugger();
    void refreshDebugSuggestions();
    void applyDebugCommand();
    void checkpointRecovery();
    [[nodiscard]] bool saveSession(std::string& error);
    void restoreSession(const workspace::session::State& state);
    void openRecovery(bool includeHistory = true);
    void refreshRecoverySuggestions();
    void applyRecovery();
    void openModules();
    void refreshModuleSuggestions();
    void applyModuleCommand();
    void openGitReview();
    void refreshGitSuggestions();
    void applyGitReview();
    void pollGitRemote();
    void showTaskView(ui::PromptKind view);
    void refreshTaskSuggestions();
    void applyTaskCommand();
    void captureTaskContext(workspace::Task& task) const;
    [[nodiscard]] bool saveActiveTaskContext();
    [[nodiscard]] bool activateTask(std::string_view id);
    [[nodiscard]] bool commitTaskBoard(workspace::TaskBoard board);
    void openProjectPrompt();
    void openFunctionPrompt();
    void openAiSettings();
    void applyAiSettings();
    void openAiPrompt();
    void openAiCleanup();
    void openFind();
    void openProjectSearch(bool replace);
    void applyProjectSearch();
    void pollProjectSearch();
    void openReplace();
    void applyReplacement();
    void findInDocument(bool advance, bool backwards = false);
    void pollAi();
    void openCompletion();
    void refreshCompletions();
    void pollLanguageServer();
    void requestLanguageNavigation(bool references);
    void requestLanguageFormatting();
    void applyLanguageLocation();
    void applyCommand(bool forceFolder);
    void createFolder(const std::string& path);
    void openProject(const std::string& path);
    void openExternalPath(const std::string& path);
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
    void syncAccessibility(const ui::FrameState& frame);
    void openSettings();
    void changeZoom(float delta);
    void toggleHighContrast();
    void openSettingsJson();
    void closeSettingsPanel();
    void handleSettingsPanelClick(double x, double y);
    void activateSettingsControl(ui::SettingsControl control);
    void moveSettingsFocus(int direction);
    void updateSettingsPanelHover(double x, double y);
    void applySettings(const EditorSettings& settings);
    void applyTheme(const ThemeSettings& theme);
    bool persistSettings();
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
    void pollFunctionIndex();
    void invalidateFunctionIndex();
    void scheduleProjectIndex();
    void pollProjectIndex();
    [[nodiscard]] bool ensureCompileTerminalWindow();
    void destroyCompileTerminalWindow();
    void closeCompileTerminal();
    void focusCompileTerminal(bool focused);
    void handleCompileTerminalKey(const input::KeyEvent& event);
    void handleCompileTerminalChar(char32_t codepoint);
    void handleCompileTerminalMouseButton(const input::MouseButtonEvent& event);
    void handleCompileTerminalMouseMove(const input::MouseMoveEvent& event);
    [[nodiscard]] std::size_t compileOutputPosition(double x, double y, bool standalone) const;
    void copyCompileSelection();
    void scrollCompileOutput(double offset, bool standalone);
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
    [[nodiscard]] bool welcomeExpandContains(double x, double y) const;
    [[nodiscard]] bool titlebarDragContains(double x, double y) const;
    [[nodiscard]] ui::FrameState makeFrameState(double timeSeconds);
    [[nodiscard]] std::string commandPath() const;

    void setCommandStatus(std::string message, bool error);
    void setStatus(std::string message, bool error);

    std::string m_accessibilityKey;
    ApplicationConfig m_config;
    ui::SearchBar m_searchBar;
    Window m_window;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Window> m_compileTerminalWindow;
    std::unique_ptr<Renderer> m_compileTerminalRenderer;

    workspace::Project m_project;
    bool m_hasProject = false;
    std::string m_projectName;

    enum class TaskChoiceKind { NewTask, Identity, SelectTask, OpenWorkspace, LeaveWorkspace, AddSubtask,
        ViewSubtasks, Assign, ToggleWorking, ToggleDone, History, Solution,
        PreviousPage, NextPage, ToggleSubtask, Tier1, Tier2, Tier3, Label };
    struct TaskChoice { TaskChoiceKind kind; std::string id; };
    enum class TaskInputKind { Identity, Title, Subtask, Assignee, Solution };
    std::filesystem::path m_recoveryFile;
    std::vector<std::filesystem::path> m_recoveryFiles;
    struct RecoverySummary { std::string file; bool saved = false, history = false; std::size_t openCount = 0; };
    std::vector<RecoverySummary> m_recoverySummaries;
    workspace::recovery::Snapshot m_recoveryPreview;
    std::size_t m_recoverySelected = 0, m_recoveryPage = 0;
    std::vector<int> m_recoveryChoices;
    double m_recoveryLastTime = 0.0;
    std::filesystem::path m_moduleRoot;
    std::vector<modules::Module> m_modules;
    std::vector<int> m_moduleChoices;
    std::size_t m_moduleSelected = 0;
    std::size_t m_modulePage = 0;
    debug::AdapterProcess m_debugAdapter;
    debug::Session m_debugSession;
    std::map<std::string, std::vector<int>> m_debugBreakpoints;
    std::string m_debugSource, m_debugInline, m_debugLastWarning, m_debugLastMessage;
    std::vector<int> m_debugChoices;
    std::size_t m_debugPage = 0;
    bool m_debugClosing = false;
    double m_debugCloseDeadline = 0.0;
    double m_debugStartDeadline = 0.0;
    debug::State m_debugLastState = debug::State::Idle;
    std::vector<workspace::GitChange> m_gitChanges;
    std::vector<workspace::GitHistoryEntry> m_gitHistory;
    workspace::GitReview m_gitReview;
    workspace::GitCommitPreview m_gitCommitPreview;
    workspace::GitUpstream m_gitUpstream;
    workspace::GitRemoteAction m_gitRemoteAction = workspace::GitRemoteAction::Fetch;
    struct GitRemoteResult { bool ok = false; std::string output, error; };
    std::future<GitRemoteResult> m_gitRemoteFuture;
    std::string m_gitRemoteRoot;
    std::size_t m_gitSelectedFile = 0;
    std::size_t m_gitPage = 0;
    std::vector<int> m_gitChoices;
    workspace::TaskBoard m_taskBoard;
    std::string m_activeTaskId;
    std::string m_selectedTaskId;
    std::string m_taskDraftTitle;
    std::vector<TaskChoice> m_taskChoices;
    std::size_t m_taskPage = 0;
    TaskInputKind m_taskInputKind = TaskInputKind::Identity;

    struct OpenDocument {
        std::string path;
        editor::Document document;
        int firstLine = 0;
        std::string diskContents;
    };
    std::vector<OpenDocument> m_inactiveDocuments;
    editor::Document m_document;
    bool m_hasDocument = false;
    std::string m_filePath;
    std::string m_diskContents;
    int m_firstLine = 0;
    bool m_followCursor = true;
    bool m_welcomeActionHovered = false;
    int m_welcomeShortcutScroll = 0;
    bool m_welcomeShortcutsExpanded = false;
    float m_welcomeShortcutProgress = 0.0F;
    editor::TextPosition m_findAnchor{};
    bool m_replacePending = false;
    std::string m_replaceQuery;
    std::string m_replaceFile;
    std::uint64_t m_replaceRevision = 0;
    bool m_completionOpen = false;
    editor::lsp::Client m_languageServer;
    std::string m_languageRoot;
    std::string m_languageAttemptRoot;
    std::string m_languageFile;
    std::uint64_t m_languageRevision = 0;
    struct LanguageLocation { std::string path; std::size_t line = 0, character = 0; };
    std::vector<LanguageLocation> m_languageLocations;
    std::vector<int> m_languageLocationChoices;
    std::size_t m_languageLocationPage = 0;
    bool m_languageNavigationQueued = false, m_languageReferences = false;
    std::uint64_t m_languageNavigationRequest = 0, m_languageNavigationRevision = 0;
    std::string m_languageNavigationFile;
    std::size_t m_languageNavigationLine = 0, m_languageNavigationColumn = 0;
    bool m_languageFormatQueued = false;
    std::uint64_t m_languageFormatRequest = 0, m_languageFormatRevision = 0;
    std::string m_languageFormatFile, m_languageFormatOriginal, m_languageFormatOutput;
    std::string m_languageDiagnosticsFile;
    std::uint64_t m_languageDiagnosticsRevision = ~std::uint64_t{0};
    std::uint64_t m_completionRequest = 0;
    std::uint64_t m_completionRequestRevision = 0;
    std::size_t m_completionRequestLine = 0, m_completionRequestColumn = 0;
    int m_completionSelected = 0;
    std::size_t m_completionStart = 0;
    std::vector<std::string> m_completions;
    std::set<std::string> m_projectCompletionNames;
    bool m_mouseSelecting = false;
    bool m_wordSelecting = false;
    editor::TextPosition m_wordSelectionStart{};
    editor::TextPosition m_wordSelectionEnd{};
    int m_clickCount = 0;
    double m_lastClickTime = -1.0;
    double m_lastClickX = 0.0;
    double m_lastClickY = 0.0;
    float m_charAdvance = 7.0F;

    float m_animationSpeed = 1.0F;
    bool m_showDiagnostics = true;
    bool m_showLineNumbers = true;
    bool m_highlightCurrentLine = true;
    bool m_cursorBlink = true;
    editor::CursorStyle m_cursorStyle = editor::CursorStyle::Line;
    bool m_backgroundMotion = true;
    float m_backgroundDim = 0.0F;
    std::string m_statusPosition;
    float m_statusPositionReveal = 0.0F;
    float m_uiZoom = 1.0F;
    EditorSettings m_settings;

    std::vector<std::string> m_recentProjects;
    bool m_restoringSession = false;
    std::vector<std::string> m_suggestionPaths;
    std::vector<std::string> m_suggestionDetails;
    std::vector<std::size_t> m_suggestionFunctionIndices;
    std::vector<ui::PathSuggestion> m_suggestions;
    std::size_t m_suggestionScroll = 0;
    bool m_suggestionHasMoreBelow = false;
    float m_suggestionMoreAbove = 0.0F;
    float m_suggestionMoreBelow = 0.0F;
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
    std::string m_compileSelectionSnapshot;
    std::size_t m_compileSelectionAnchor = 0;
    std::size_t m_compileSelectionEnd = 0;
    bool m_compileSelecting = false;
    int m_compileTerminalLine = 0;
    int m_compileOutputScroll = 0;
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
    int m_settingsMenuPage = 0;
    std::size_t m_backgroundScenePage = 0;
    std::vector<int> m_backgroundSceneChoices;
    ui::SettingsControl m_settingsHovered = ui::SettingsControl::None;
    std::uint64_t m_settingsRevision = ~std::uint64_t{0};

    std::vector<editor::Diagnostic> m_diagnostics;
    workspace::ProjectSearchResults m_projectSearchResults;
    std::string m_projectSearchQuery, m_projectReplacement;
    std::size_t m_projectSearchPage = 0;
    bool m_projectSearchReplace = false;
    std::future<workspace::ProjectSearchResults> m_projectSearchFuture;
    std::string m_projectSearchRoot;
    std::vector<int> m_projectSearchChoices;
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

    struct AiResult { std::string output; std::string error; };
    std::future<AiResult> m_aiFuture;
    bool m_aiDownloadingModel = false;
    std::shared_ptr<std::atomic<int>> m_aiProgress;
    std::shared_ptr<std::atomic_bool> m_aiCancel;
    std::string m_aiResponse;
    std::vector<std::string> m_aiLines;
    std::size_t m_aiScroll = 0;
    std::string m_aiFile;
    std::uint64_t m_aiRevision = 0;
    editor::TextPosition m_aiStart{};
    editor::TextPosition m_aiEnd{};
    bool m_aiReady = false;
    bool m_aiCleanup = false;
    bool m_aiDebugExplanation = false;
    bool m_aiDebugOpenRouter = false;
    int m_aiSettingsWorkflow = 0;
    std::string m_aiSecret;
    std::string m_aiDebugContext;
    std::future<editor::CompilerResult> m_compilerFuture;
    bool m_compilerRunning = false;
    std::uint64_t m_compilerRunningRevision = 0;
    std::string m_compilerRunningPath;
    std::uint64_t m_compilerAppliedRevision = 0;
    std::string m_compilerAppliedPath;
    double m_compilerDueTime = 0.0;

    workspace::FunctionIndex m_functionIndex;
    struct FunctionIndexResult { workspace::FunctionIndex index; std::string root, error; std::uint64_t generation = 0; };
    std::future<FunctionIndexResult> m_functionIndexFuture;
    std::uint64_t m_functionIndexGeneration = 0;
    struct ProjectIndexResult { workspace::Project project; std::string root; std::uint64_t generation = 0; };
    std::future<ProjectIndexResult> m_projectIndexFuture;
    std::uint64_t m_projectIndexGeneration = 0;
    bool m_projectIndexPending = false;
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
