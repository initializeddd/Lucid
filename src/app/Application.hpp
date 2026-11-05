#pragma once

#include "app/Settings.hpp"
#include "debug/Session.hpp"
#include "debug/AdapterProcess.hpp"
#include "editor/lsp/Client.hpp"
#include "editor/lsp/TextEdits.hpp"
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
#include "modules/ContainerInspection.hpp"
#include "modules/ContainerActions.hpp"
#include "modules/ModuleProcess.hpp"
#include "modules/Registry.hpp"
#include "modules/SecretScan.hpp"
#include "modules/CMakeModel.hpp"
#include "modules/CMakeCacheValues.hpp"
#include "modules/CMakeBuild.hpp"
#include "modules/MarkdownOutline.hpp"
#include "modules/MarkdownPreview.hpp"
#include "modules/MarkdownLinks.hpp"
#include "modules/ExternalLinks.hpp"
#include "modules/AccessibilityAudit.hpp"
#include "modules/ApiRequests.hpp"
#include "modules/ApiAssertions.hpp"
#include "modules/ApiPreflight.hpp"
#include "modules/SqlChanges.hpp"
#include "modules/SqlRowDiff.hpp"
#include "modules/DesignTokenAliases.hpp"
#include "modules/JUnitImport.hpp"
#include "ui/VisualReviewCrop.hpp"
#include "ui/VisualMaskSelection.hpp"
#include "modules/VisualConfigEdit.hpp"
#include "modules/GitHubPullRequests.hpp"
#include "modules/PullRequestPatch.hpp"
#include "modules/DependencyRisk.hpp"
#include "modules/ReleaseNotes.hpp"
#include "modules/SqlWorkbench.hpp"
#include "modules/Coverage.hpp"
#include "modules/CoverageSummary.hpp"
#include "modules/BenchmarkComparison.hpp"
#include "modules/BinaryInspector.hpp"
#include "modules/LocalizationWorkspace.hpp"
#include "modules/DesignTokens.hpp"
#include "modules/SerialCapture.hpp"
#include "modules/SerialDevices.hpp"
#include "modules/BoardProfiles.hpp"
#include "modules/OpenOcdFlash.hpp"
#include "modules/ShaderCompiler.hpp"
#include "modules/ShaderIncludes.hpp"
#include "modules/SpirvInspection.hpp"
#include "modules/ShaderPreview.hpp"
#include "modules/VisualComparison.hpp"
#include "ui/VisualReviewPreview.hpp"
#include "modules/LogCorrelation.hpp"
#include "modules/PythonEnvironments.hpp"
#include "modules/PythonRuntime.hpp"
#include "modules/PythonPreference.hpp"
#include "modules/CTestRunner.hpp"
#include "modules/CTestHistory.hpp"
#include "modules/HexFile.hpp"
#include "modules/LinkerMap.hpp"
#include "modules/ToolchainProfiles.hpp"
#include "modules/ToolchainTargets.hpp"
#include "modules/TargetConfigure.hpp"
#include "modules/SvdRegisters.hpp"
#include "modules/UsbDescriptors.hpp"
#include "modules/CrashDump.hpp"
#include "modules/UnixCrashReport.hpp"
#include "modules/AppleCrashReport.hpp"
#include "modules/AppleIpsReport.hpp"
#include "modules/CrashSymbols.hpp"
#include "modules/RustOwnership.hpp"
#include "modules/RustDiagnosticRun.hpp"
#include "modules/DiagramPreview.hpp"
#include "modules/WebPreviewBundle.hpp"
#if defined(_WIN32) && defined(LUCID_WEBVIEW2_UI_TEST)
#include "platform/browser/WebPreviewPresentation.hpp"
#endif
#if defined(__APPLE__)
#include "platform/browser/MacWebPreview.hpp"
#endif

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <map>
#include <optional>
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
    void closeSwitcherSelection();
    void openTasks();
    void openDebugger();
    void toggleBreakpoint();
    void pollDebugger();
    void stopDebugger();
    void refreshDebugSuggestions();
    void applyDebugCommand();
    bool checkpointRecovery();
    bool checkpointInactiveEdits();
    [[nodiscard]] bool saveSession(std::string& error);
    void restoreSession(const workspace::session::State& state);
    void openRecovery(bool includeHistory = true);
    void refreshRecoverySuggestions();
    [[nodiscard]] bool recoverySnapshotProtected() const;
    void applyRecovery();
    void openModules();
    void refreshModuleSuggestions();
    void applyModuleCommand();
    bool navigateSnippet(bool backwards);
    bool pythonModuleReady(std::string& error);
    bool ctestModuleReady(std::string& error);
    bool hexModuleReady(std::string& error);
    void pollModules();
#if defined(_WIN32) && defined(LUCID_WEBVIEW2_UI_TEST)
    [[nodiscard]] platform::WebPreviewContext windowsWebPreviewContext(bool& authorized) const;
#endif
    void requestVisualCrop(std::size_t x, std::size_t y);
    void prepareVisualConfigChange(const modules::VisualConfigChange& change);
    void updateVisualMaskDrag(double x, double y);
    [[nodiscard]] bool visualConfigAuthorized() const;
    void refreshModuleCatalog();
    void openGitReview();
    void refreshGitSuggestions();
    void applyGitReview();
    void pollGitRemote();
    void pollGitBranch();
    void pollGitSecretScan();
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
    void reopenProjectSearchResults();
    void applyProjectSearch();
    void pollProjectSearch();
    void openReplace();
    void applyReplacement();
    void findInDocument(bool advance, bool backwards = false);
    void pollAi();
    void openCompletion();
    void refreshCompletions();
    void pollLanguageServer();
    [[nodiscard]] bool ensureLanguageServer(std::string& error);
    void requestLanguageNavigation(bool references, bool declaration = false);
    void openLanguageStatus();
    void applyLanguageStatus();
    void requestLanguageFormatting();
    void requestLanguageSignatures();
    void openLanguageSymbols(bool currentFile = false);
    void applyLanguageSymbols();
    void expireLanguageRequests(double now);
    void cancelLanguageRequest(std::uint64_t& request);
    void requestLanguageRename();
    void applyLanguageRename();
    void applyLanguageLocation();
    void applyCommand(bool forceFolder);
    void createFolder(const std::string& path);
    bool openProject(const std::string& path);
    void openExternalPath(const std::string& path);
    void openFile(const std::string& path, bool allowCreate = true);
    [[nodiscard]] bool saveDocument();
    void closeDocument();
    void reopenClosedDocument();
    void deleteCurrentFile();
    void deleteCurrentProject();
    void closeProject();

    void moveSuggestion(int delta);
    void acceptSuggestion();
    void refreshSuggestions();
    void refreshMenuViewport();
    void selectSuggestionRow(std::size_t index);
    void onCommandQueryChanged();
    void clearCommandFeedback();
    void refreshHighlight();
    void refreshDiagnostics(double timeSeconds);
    void pollCompilerDiagnostics(double timeSeconds);
    void updateActiveDiagnostic();
    void navigateDiagnostic(bool backwards);
    void openDiagnosticList();
    void applyDiagnosticList();
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
    bool ensureSettingsFile();
    [[nodiscard]] bool isJsonDocument() const;
    [[nodiscard]] bool isCodeDocument() const;
    [[nodiscard]] bool isSettingsDocument() const;
    [[nodiscard]] std::string absoluteDocumentPath() const;
    [[nodiscard]] ui::SettingsErrorLayout settingsErrorLayout() const;
    [[nodiscard]] ui::SettingsPanelLayout settingsPanelLayout() const;
    [[nodiscard]] ui::SettingsControl settingsControlAt(double x, double y) const;
    [[nodiscard]] ui::DiagnosticCardLayout diagnosticCardLayout() const;
    [[nodiscard]] float diagnosticAnchorX() const;
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
    void selectAllCompileOutput();
    void copyCompileSelection();
    void updateCompileOutput();
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
    [[nodiscard]] bool placeCommandCaretAt(double x, double y, bool extendSelection, bool dragging = false);
    void updateCursorShape(double x, double y);
    [[nodiscard]] int visibleRowCount() const;
    [[nodiscard]] int maxFirstLine() const;
    [[nodiscard]] float suggestionListTop() const;
    [[nodiscard]] std::size_t commandStatusColumns() const;
    [[nodiscard]] std::size_t commandStatusRows() const;
    [[nodiscard]] std::size_t visibleSuggestionCount() const;
    [[nodiscard]] std::size_t pagedContentCount() const;
    [[nodiscard]] int suggestionIndexAt(double x, double y) const;
    [[nodiscard]] bool commandRegionContains(double x, double y) const;
    [[nodiscard]] bool welcomeActionContains(double x, double y) const;
    [[nodiscard]] bool welcomeExpandContains(double x, double y) const;
    [[nodiscard]] bool titlebarDragContains(double x, double y) const;
    [[nodiscard]] ui::FrameState makeFrameState(double timeSeconds);
    [[nodiscard]] std::string commandPath() const;

    void setCommandStatus(std::string message, bool error);
    [[nodiscard]] bool dependencyArtifactReady(std::string& error) const;
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
    std::map<std::string, std::filesystem::path> m_inactiveRecoveryFiles;
    std::filesystem::path m_cachedRecoveryPath;
    std::optional<workspace::recovery::Snapshot> m_cachedRecoverySnapshot;
    std::vector<std::filesystem::path> m_recoveryFiles;
    struct RecoverySummary { std::string file; bool saved = false, history = false; std::size_t openCount = 0; std::string diskState; };
    std::vector<RecoverySummary> m_recoverySummaries;
    workspace::recovery::Snapshot m_recoveryPreview;
    std::size_t m_recoverySelected = 0, m_recoveryPage = 0;
    std::vector<int> m_recoveryChoices;
    double m_recoveryLastTime = 0.0;
    std::filesystem::path m_moduleRoot;
    std::vector<modules::Module> m_modules;
    modules::Module m_modulePreview;
    std::string m_modulePreviewManifest;
    std::string m_modulePreviewSource;
    std::string m_personalSnippetDraft;
    bool m_personalSnippetTemplate = false;
    struct ModuleFetchResult {
        bool catalog = false;
        std::size_t generation = 0;
        std::string id, manifest, source, error;
        std::vector<modules::RegistryEntry> entries;
    };
    std::vector<modules::RegistryEntry> m_moduleCatalog;
    std::future<ModuleFetchResult> m_moduleFetchFuture;
    std::future<modules::SecretScanResult> m_moduleScanFuture;
    std::shared_ptr<std::atomic_bool> m_moduleScanCancel;
    modules::SecretScanResult m_moduleScanResults;
    std::string m_moduleScanRoot;
    int m_moduleScanFilter = 0;
    bool m_moduleScanShowingResults = false;
    struct CMakeModelResult { modules::CMakeModel model; std::string error; };
    std::future<CMakeModelResult> m_moduleCMakeFuture;
    modules::CMakeModel m_moduleCMakeModel;
    std::string m_moduleCMakeRoot;
    std::string m_moduleCMakeBuild = "build";
    bool m_moduleCMakePrepare = false;
    bool m_moduleCMakeShowing = false;
    bool m_moduleCMakeOptions = false;
    bool m_moduleCMakeEditing = false, m_moduleCMakeStale = false;
    std::optional<std::size_t> m_moduleCMakeOption;
    std::vector<modules::CMakeCacheValue> m_cmakeValues;
    std::optional<std::size_t> m_cmakeValueSelected;
    bool m_cmakeValueMode=false, m_cmakeValueInput=false;
    struct CMakeValuesResult {std::vector<modules::CMakeCacheValue> values;std::string error;};
    std::future<CMakeValuesResult> m_cmakeValuesFuture;
    struct CMakeCachePrepareResult { modules::CMakeCacheUpdate update; std::string error; };
    std::future<CMakeCachePrepareResult> m_cmakeCachePrepareFuture;
    std::future<std::string> m_cmakeCacheSaveFuture;
    modules::CMakeCacheUpdate m_cmakeCacheUpdate;
    std::string m_cmakeCacheTarget;
    std::optional<std::size_t> m_moduleCMakeTarget;
    int m_cmakeBuildStage=0;
    modules::CMakeBuildPlan m_cmakeBuildPlan;
    modules::ModuleProcessResult m_cmakeBuildResult;
    std::future<modules::CMakeBuildPlan> m_cmakeBuildPrepareFuture;
    std::future<modules::ModuleProcessResult> m_cmakeBuildFuture;
    std::shared_ptr<std::atomic_bool> m_cmakeBuildCancel;
    std::uint64_t m_cmakeBuildGeneration=0;
    bool m_moduleMarkdownShowing = false;
    modules::MarkdownOutline m_moduleMarkdownOutline;
    std::string m_moduleMarkdownFile;
    std::uint64_t m_moduleMarkdownRevision = 0;
    struct MarkdownPreviewResult { modules::MarkdownPreview preview; std::string error; };
    std::future<MarkdownPreviewResult> m_markdownPreviewFuture;
    bool m_markdownPreviewShowing = false;
    std::vector<std::pair<std::string, ui::SuggestionKind>> m_markdownPreviewLines;
    std::future<modules::MarkdownLinkResults> m_markdownLinksFuture;
    std::shared_ptr<std::atomic_bool> m_markdownLinksCancel;
    modules::MarkdownLinkResults m_markdownLinksResults;
    std::string m_markdownLinksRoot;
    int m_markdownLinksFilter = 0;
    bool m_markdownLinksShowing = false;
    bool m_markdownExternalMode = false;
    std::optional<std::size_t> m_markdownExternalSelected;
    std::future<modules::LinkCheck> m_markdownExternalFuture;
    std::shared_ptr<std::atomic_bool> m_markdownExternalCancel;
    std::size_t m_markdownExternalRequestIndex = 0;
    std::uint64_t m_markdownExternalGeneration = 0;
    std::string m_markdownExternalModule;
    struct CoverageResult { modules::CoverageReport report; modules::CoverageComparison comparison; std::map<std::string, std::string> baselines; std::string error; bool comparing = false; };
    std::future<CoverageResult> m_coverageFuture;
    std::shared_ptr<std::atomic_bool> m_coverageCancel;
    modules::CoverageReport m_coverageReport;
    modules::CoverageSummary m_coverageSummary;
    modules::CoverageComparison m_coverageComparison;
    bool m_coverageChoosingBaseline = false, m_coverageComparing = false;
    bool m_coverageFilesMode = false;
    std::string m_coverageFile;
    std::map<std::string, std::string> m_coverageBaselines;
    std::string m_coverageRoot;
    bool m_coverageCobertura = false;
    bool m_coverageShowing = false;
    int m_coverageFilter = 0; // 0: all, 1: uncovered lines, 2: covered lines, 3: uncovered branches, 4: unknown branches.
    struct BenchmarkResult { modules::BenchmarkComparison comparison; std::string error; };
    std::future<BenchmarkResult> m_benchmarkFuture;
    std::shared_ptr<std::atomic_bool> m_benchmarkCancel;
    modules::BenchmarkComparison m_benchmarkComparison;
    std::string m_benchmarkRoot, m_benchmarkBaseline;
    std::optional<std::size_t> m_benchmarkSelected;
    int m_benchmarkFilter = 0;
    bool m_benchmarkSelectingCandidate = false, m_benchmarkShowing = false;
    struct BinaryResult { modules::BinaryInspection inspection; std::shared_ptr<const std::string> snapshot; bool selecting = false; std::size_t slice = 0; };
    std::future<BinaryResult> m_binaryFuture;
    std::shared_ptr<std::atomic_bool> m_binaryCancel;
    modules::BinaryInspection m_binaryInspection;
    std::shared_ptr<const std::string> m_binarySnapshot;
    std::size_t m_binarySlice = 0;
    std::string m_binaryRoot;
    bool m_binaryShowing = false;
    std::optional<unsigned> m_binaryCategory; // Sections, libraries, or symbols.
    struct LocalizationResult { std::vector<modules::LocalizationEntry> entries; std::string sourceSnapshot, translationSnapshot, error; };
    std::future<LocalizationResult> m_localizationFuture;
    std::shared_ptr<std::atomic_bool> m_localizationCancel;
    std::vector<modules::LocalizationEntry> m_localizationEntries;
    std::string m_localizationRoot, m_localizationSource, m_localizationTranslation, m_localizationSourceSnapshot, m_localizationTranslationSnapshot;
    modules::LocalizationEditPlan m_localizationEditPlan;
    std::string m_localizationEditModule, m_localizationEditContextFile;
    std::uint64_t m_localizationEditContextRevision = 0;
    bool m_localizationEditContextHasDocument = false, m_localizationEditPreview = false;
    std::optional<std::size_t> m_localizationSelected;
    bool m_localizationSelectingTranslation = false, m_localizationShowing = false, m_localizationNeedsTranslation = false;
    struct DesignTokenResult { modules::DesignTokenReport report; modules::CssTokenUsageReport usages; std::vector<modules::DesignAliasResult> aliases; std::string snapshot, error; };
    std::future<DesignTokenResult> m_designTokenFuture;
    std::shared_ptr<std::atomic_bool> m_designTokenCancel;
    modules::DesignTokenReport m_designTokenReport;
    modules::CssTokenUsageReport m_designTokenUsages;
    bool m_designTokenUsageShowing = false;
    std::vector<modules::DesignAliasResult> m_designTokenAliases;
    std::uint64_t m_designTokenGeneration=0;
    bool m_designTokenAliasEnabled=false;
    std::string m_designTokenRoot, m_designTokenFile, m_designTokenSnapshot, m_designTokenModule;
    std::optional<std::size_t> m_designTokenSelected;
    bool m_designTokenShowing = false;
    struct LogResult { modules::LogCorrelationResult report; std::map<std::string, std::string> snapshots; std::string error; };
    std::future<LogResult> m_logFuture;
    std::shared_ptr<std::atomic_bool> m_logCancel;
    modules::LogCorrelationResult m_logReport;
    std::map<std::string, std::string> m_logSnapshots;
    std::vector<std::string> m_logPaths;
    std::string m_logRoot, m_logQuery, m_logModule;
    std::uint64_t m_logGeneration = 0;
    std::optional<std::size_t> m_logSelected;
    int m_logInputStage = 0;
    bool m_logShowing = false, m_logJson = false;
    struct PythonDiscoveryResult { std::vector<modules::PythonEnvironment> environments; modules::PythonPreference preference; std::string error, preferenceError; };
    std::future<PythonDiscoveryResult> m_pythonDiscoveryFuture;
    std::vector<modules::PythonEnvironment> m_pythonEnvironments;
    std::filesystem::path m_pythonUnlistedInterpreter;
    std::string m_pythonRoot;
    bool m_pythonShowing = false;
    bool m_pythonManage = false, m_pythonPreferenceReady = false;
    std::optional<std::size_t> m_pythonSelected;
    modules::PythonPreference m_pythonPreference;
    std::future<modules::PythonRuntimeResult> m_pythonRuntimeFuture;
    std::future<modules::PythonServerProvisionResult> m_pythonProvisionFuture;
    std::shared_ptr<std::atomic_bool> m_pythonProvisionCancel;
    std::optional<bool> m_pythonProvisionBootstrap;
    std::string m_pythonProvisionRoot;
    std::filesystem::path m_pythonProvisionInterpreter;
    std::uint64_t m_pythonProvisionGeneration = 0;
    bool m_pythonCreating = false;
    struct JUnitImportResult { modules::JUnitReport report; std::string snapshot, path, error; };
    std::future<JUnitImportResult> m_junitFuture;
    std::shared_ptr<std::atomic_bool> m_junitCancel;
    modules::JUnitReport m_junitCurrent, m_junitPrevious;
    std::vector<modules::JUnitComparisonEntry> m_junitComparison;
    std::string m_junitRoot, m_junitModule, m_junitPath, m_junitSnapshot, m_junitPreviousPath, m_junitPreviousSnapshot;
    std::optional<std::size_t> m_junitSelected;
    std::uint64_t m_junitGeneration=0;
    int m_junitInputMode=0;
    bool m_junitShowing=false, m_junitComparing=false;
    std::future<modules::CTestDiscoveryResult> m_ctestDiscoveryFuture;
    std::future<modules::CTestRunResult> m_ctestRunFuture;
    std::shared_ptr<std::atomic_bool> m_ctestCancel;
    modules::TestDiscovery m_ctestDiscovery;
    std::map<std::string, modules::TestResult> m_ctestLast, m_ctestPrevious;
    modules::CTestHistorySnapshot m_ctestHistory;
    std::vector<modules::TestHistorySummary> m_ctestSummaries;
    std::vector<modules::FailedRerunCandidate> m_ctestRerunCandidates;
    std::optional<std::size_t> m_ctestRerunSelected;
    bool m_ctestRerunShowing = false;
    bool m_ctestHistoryReady = false;
    std::string m_ctestHistoryError;
    std::filesystem::path m_ctestBuild, m_ctestExecutable;
    std::string m_ctestRoot, m_ctestRunning;
    std::optional<std::size_t> m_ctestSelected;
    int m_ctestInputStage = 0;
    bool m_ctestShowing = false;
    struct HexOperation { modules::HexFile file; std::string error, root, path; bool ok = false, saving = false; };
    std::future<HexOperation> m_hexFuture;
    std::optional<modules::HexFile> m_hexFile;
    std::string m_hexRoot, m_hexPath, m_hexBaseline;
    std::size_t m_hexViewOffset = 0, m_hexEditOffset = 0;
    std::string m_hexLastNeedle;
    std::size_t m_hexNextSearch = 0;
    enum class HexOffsetAction { Navigate, Overwrite, Select, Insert };
    HexOffsetAction m_hexOffsetAction = HexOffsetAction::Navigate;
    bool m_hexShowing = false, m_hexBytesInsert = false, m_hexReplaceSelection = false, m_hexSaving = false;
    struct LinkerResult { modules::LinkerMap map; std::string error; };
    std::future<LinkerResult> m_linkerFuture;
    std::shared_ptr<std::atomic_bool> m_linkerCancel;
    modules::LinkerMap m_linkerMap;
    std::string m_linkerRoot;
    std::size_t m_linkerSectionOffset = 0;
    bool m_linkerShowing = false;
    std::future<modules::SvdRegisters> m_svdFuture;
    std::shared_ptr<std::atomic_bool> m_svdCancel;
    modules::SvdRegisters m_svdRegisters;
    std::optional<std::size_t> m_svdSelected;
    bool m_svdShowing = false, m_svdInput = false;
    struct ToolchainResult { std::vector<modules::ToolchainProfile> profiles; std::string error, snapshot; };
    std::future<ToolchainResult> m_toolchainFuture;
    std::shared_ptr<std::atomic_bool> m_toolchainCancel;
    std::vector<modules::ToolchainProfile> m_toolchainProfiles;
    std::string m_toolchainRoot, m_toolchainModule, m_toolchainFile, m_toolchainSnapshot;
    modules::TargetEditDraft m_toolchainDraft;
    modules::ToolchainProfile m_toolchainEditProfile;
    int m_toolchainEditMode=0; // 0 browse, 1 add, 2 replace, 3 remove, 4 review add, 5 review replace, 6 review remove
    int m_targetConfigureStage=0; // build path, executable, review, running, result
    std::string m_targetConfigureBuild;
    modules::TargetConfigurePlan m_targetConfigurePlan;
    modules::ModuleProcessResult m_targetConfigureResult;
    std::future<modules::ModuleProcessResult> m_targetConfigureFuture;
    std::shared_ptr<std::atomic_bool> m_targetConfigureCancel;
    std::future<std::string> m_toolchainSaveFuture;
    std::shared_ptr<std::atomic_bool> m_toolchainSaveCancel;
    std::uint64_t m_toolchainGeneration = 0;
    std::optional<std::size_t> m_toolchainSelected;
    bool m_toolchainShowing = false;
    struct CrashInspectionResult { modules::CrashDump dump; modules::UnixCrashReport gdb; modules::AppleCrashReport apple; modules::AppleIpsReport ips; bool isGdb=false, isLldb=false, isApple=false, isIps=false; };
    std::future<CrashInspectionResult> m_crashFuture;
    modules::UnixCrashReport m_crashGdb;
    bool m_crashIsGdb=false, m_crashIsLldb=false, m_crashIsApple=false, m_crashIsIps=false;
    modules::AppleIpsReport m_crashIps;
    modules::AppleCrashReport m_crashApple;
    std::shared_ptr<std::atomic_bool> m_crashCancel;
    modules::CrashDump m_crashDump;
    modules::CrashSourceMap m_crashSourceMap;
    std::string m_crashMapIdentity;
    bool m_crashMapInput = false;
    std::string m_crashRoot;
    bool m_crashShowing = false;
    struct RustOwnershipResult { std::vector<modules::RustOwnershipDiagnostic> diagnostics; std::string error; };
    std::future<RustOwnershipResult> m_rustOwnershipFuture;
    std::shared_ptr<std::atomic_bool> m_rustOwnershipCancel;
    std::vector<modules::RustOwnershipDiagnostic> m_rustOwnershipDiagnostics;
    std::vector<modules::RustOwnershipSpan> m_rustOwnershipSpans;
    std::vector<modules::RustOwnershipSpan> m_rustSuggestionCandidates;
    std::optional<modules::RustSuggestionEdit> m_rustSuggestionEdit;
    std::string m_rustSuggestionFile, m_rustSuggestionSnapshot, m_rustSuggestionModule;
    std::optional<std::size_t> m_rustOwnershipSelected;
    std::string m_rustOwnershipRoot;
    bool m_rustOwnershipShowing = false;
    int m_rustRunStage=0; // executable input, review, running, results
    std::string m_rustRunModule;
    std::uint64_t m_rustRunGeneration=0;
    modules::RustDiagnosticPlan m_rustRunPlan;
    modules::RustDiagnosticRun m_rustRunResult;
    std::future<modules::RustDiagnosticRun> m_rustRunFuture;
    std::shared_ptr<std::atomic_bool> m_rustRunCancel;
    struct ReleaseNotesResult { std::vector<modules::ReleaseNoteEntry> entries; std::string warning; };
    std::future<ReleaseNotesResult> m_releaseNotesFuture;
    std::vector<modules::ReleaseNoteEntry> m_releaseNotesEntries;
    modules::ReleaseNoteDraft m_releaseNotesDraft;
    bool m_releaseNotesPreview = false;
    int m_releaseNotesFilter = 0;
    bool m_releaseNotesEditingDate = false;
    std::string m_releaseNotesVersion, m_releaseNotesDate;
    std::string m_releaseNotesRoot, m_releaseNotesFile, m_releaseNotesModule;
    std::uint64_t m_releaseNotesRevision = 0, m_releaseNotesGeneration = 0;
    bool m_releaseNotesShowing = false;
    struct ContainerResult { modules::ContainerListing listing; modules::ContainerDetails details; modules::ModuleProcessResult process; std::string error; };
    struct ContainerActionPrepareResult { modules::ContainerActionReview review; std::string error; };
    std::future<ContainerActionPrepareResult> m_containerActionPrepareFuture;
    std::vector<modules::ContainerProjectAction> m_containerProjectActions;
    modules::ContainerActionReview m_containerActionReview;
    std::string m_containerActionConfigPath, m_containerActionConfigSnapshot;
    bool m_containerActionMode = false;
    std::future<ContainerResult> m_containerFuture;
    std::shared_ptr<std::atomic_bool> m_containerCancel;
    modules::ContainerListing m_containerListing;
    modules::ContainerDetails m_containerDetails;
    std::filesystem::path m_containerRuntime;
    std::string m_containerModule, m_containerOutput, m_containerProjectRoot;
    std::uint64_t m_containerGeneration = 0;
    std::optional<std::size_t> m_containerSelected;
    std::string m_containerOperation;
    int m_containerStage = 0;
    bool m_containerShowing = false, m_containerRunningLogs = false, m_containerRunningDetails = false;
    std::future<modules::ShaderCompilation> m_shaderFuture;
    std::future<modules::ShaderIncludeGraph> m_shaderIncludesFuture;
    std::shared_ptr<std::atomic_bool> m_shaderIncludesCancel;
    modules::ShaderIncludeGraph m_shaderIncludes;
    bool m_shaderIncludesShowing = false;
    struct SpirvResult { modules::SpirvInspection inspection; std::string error; };
    std::future<SpirvResult> m_spirvFuture;
    std::shared_ptr<std::atomic_bool> m_spirvCancel;
    SpirvResult m_spirvResult;
    bool m_spirvShowing = false;
    struct ShaderPreviewResult { modules::ShaderPreviewImage image; std::string error; };
    std::future<ShaderPreviewResult> m_shaderPreviewFuture;
    std::shared_ptr<std::atomic_bool> m_shaderPreviewCancel;
    std::string m_shaderPreviewBytes;
    int m_shaderPreviewStage = 0; // 1 review, 2 running, 3 ready, 4 failed.
    bool m_shaderPreviewViewActive = false;
    std::shared_ptr<std::atomic_bool> m_shaderCancel;
    modules::ShaderCompilation m_shaderResult;
    std::filesystem::path m_shaderCompiler;
    std::string m_shaderRoot, m_shaderFile, m_shaderStage, m_shaderModule;
    std::string m_shaderOutput;
    std::optional<std::string> m_shaderExpectedOutput;
    bool m_shaderArtifact = false;
    std::uint64_t m_shaderRevision = 0, m_shaderGeneration = 0;
    bool m_shaderShowing = false, m_shaderCompleted = false;
    struct WebBundleResult { modules::WebPreviewBundle bundle; std::string error; };
    std::future<WebBundleResult> m_webBundleFuture;
#if defined(__APPLE__)
    std::unique_ptr<platform::MacWebPreview> m_webRenderer;
#endif
#if defined(_WIN32) && defined(LUCID_WEBVIEW2_UI_TEST)
    std::unique_ptr<platform::WebPreviewPresentation> m_windowsWebPresentation;
    bool m_webCleanupReported = false, m_webPresentationActive = false;
    std::chrono::steady_clock::time_point m_webCleanupSince{};
#endif
    std::string m_webRoot, m_webFile, m_webModule, m_webError;
    std::uint64_t m_webRevision = 0, m_webGeneration = 0;
    bool m_webShowing = false, m_webStarted = false, m_webReady = false, m_webViewActive = false;
    std::future<modules::SerialDeviceScan> m_serialScanFuture;
    std::future<modules::SerialCaptureResult> m_serialFuture;
    std::shared_ptr<modules::SerialMonitorBuffer> m_serialMonitor;
    std::chrono::steady_clock::time_point m_serialMonitorRefresh{};
    std::shared_ptr<std::atomic_bool> m_serialCancel;
    modules::SerialDeviceScan m_serialScan;
    modules::SerialCaptureResult m_serialResult;
    modules::BoardProfile m_boardProfile;
    std::string m_serialModule;
    std::uint64_t m_serialGeneration = 0;
    std::size_t m_serialSelected = 0;
    unsigned m_serialBaud = 115200;
    int m_serialStage = 0;
    bool m_serialShowing = false;
    bool m_usbBoardInput = false, m_usbBoardShowing = false, m_usbBoardConfirmed = false;
    std::string m_usbBoardRoot, m_usbBoardModule;
    std::uint64_t m_usbBoardGeneration = 0;
    modules::BoardMappings m_usbBoardMappings;
    modules::UsbDescriptorScan m_usbBoardScan;
    std::future<modules::UsbDescriptorScan> m_usbBoardFuture;
    struct FlashPrepareResult { modules::FirmwareSnapshot snapshot; std::string error; };
    std::future<FlashPrepareResult> m_flashPrepareFuture;
    std::future<modules::ModuleProcessResult> m_flashFuture;
    std::shared_ptr<std::atomic_bool> m_flashCancel;
    modules::FirmwareSnapshot m_flashSnapshot;
    modules::ModuleProcessResult m_flashResult;
    std::filesystem::path m_flashExecutable;
    std::string m_flashRoot, m_flashFile;
    std::uint64_t m_flashRevision = 0, m_flashGeneration = 0;
    struct DependencyResult { modules::DependencyRiskReport report; std::string error; };
    struct DependencyDetailResult { modules::OsvDetail detail; std::string error; };
    std::future<DependencyResult> m_dependencyFuture;
    std::future<DependencyDetailResult> m_dependencyDetailFuture;
    std::future<modules::DependencyArtifactVerification> m_dependencyArtifactFuture;
    std::shared_ptr<std::atomic_bool> m_dependencyArtifactCancel;
    modules::DependencyArtifactVerification m_dependencyArtifactResult;
    std::string m_dependencyArtifactPath, m_dependencyArtifactRoot, m_dependencyDocumentSnapshot;
    std::size_t m_dependencyArtifactIndex = 0;
    int m_dependencyArtifactStage = 0; // 1 review, 2 verifying, 3 result

    std::shared_ptr<std::atomic_bool> m_dependencyCancel;
    modules::DependencyRiskReport m_dependencyReport;
    modules::OsvDetail m_dependencyDetail;
    bool m_dependencyProvenanceShowing = false;
    std::optional<std::size_t> m_dependencyProvenanceSelected;
    std::vector<std::string> m_dependencyIds;
    std::string m_dependencySelectedId;
    bool m_dependencyDetailReady = false;
    std::string m_dependencyFile, m_dependencyModule, m_dependencyError;
    std::uint64_t m_dependencyRevision = 0, m_dependencyGeneration = 0;
    bool m_dependencyShowing = false, m_dependencyReady = false, m_dependencyQuerying = false, m_dependencyQueried = false;
    struct PullRequestResult { modules::PullRequestData data; std::string error; };
    std::future<PullRequestResult> m_pullRequestFuture;
    std::shared_ptr<std::atomic_bool> m_pullRequestCancel;
    modules::PullRequestData m_pullRequestData;
    std::string m_pullRequestUrl, m_pullRequestModule, m_pullRequestError;
    std::uint64_t m_pullRequestGeneration = 0;
    bool m_pullRequestShowing = false, m_pullRequestFetched = false;
    struct PullPatchResult { std::vector<modules::PatchHunk> hunks; std::string error; };
    std::future<PullPatchResult> m_pullPatchFuture;
    std::optional<std::size_t> m_pullPatchSelected;
    std::size_t m_pullPatchPending = 0;
    std::uint64_t m_pullPatchGeneration = 0;
    std::vector<modules::PatchHunk> m_pullPatchHunks;
    std::string m_pullPatchError;
    std::string m_pullCommentBody;
    std::size_t m_pullCommentLine = 0;
    bool m_pullCommentConfirm = false;
    int m_pullReviewMode = 0; // 0: inline comment, 1: approve, 2: request changes, 3: comment review.
    bool m_pullReviewPosting = false;
    std::future<std::string> m_pullCommentFuture;
    std::future<modules::ApiAssertionReport> m_apiAssertionFuture;
    std::shared_ptr<std::atomic_bool> m_apiAssertionCancel;
    modules::ApiAssertionReport m_apiAssertions;
    int m_apiAssertionStage = 0;
    std::uint64_t m_apiAssertionGeneration = 0, m_apiAssertionResponseGeneration = 0;
    std::future<modules::ApiResponse> m_apiFuture;
    std::shared_ptr<std::atomic_bool> m_apiCancel;
    std::vector<modules::ApiRequest> m_apiRequests;
    struct ApiPreflightResult { modules::ApiPreflightReport report; std::string error; };
    std::future<ApiPreflightResult> m_apiPreflightFuture;
    std::shared_ptr<std::atomic_bool> m_apiPreflightCancel;
    modules::ApiPreflightReport m_apiPreflight;
    std::optional<std::size_t> m_apiPreflightSelected;
    std::string m_apiEnvironmentFile, m_apiEnvironmentSnapshot;
    std::uint64_t m_apiPreflightGeneration=0;
    bool m_apiPreflightShowing=false;
    std::vector<modules::ApiEnvironment> m_apiEnvironments;
    bool m_apiEditMode = false, m_apiEditAdding = false, m_apiEditConfirm = false;
    std::vector<std::string> m_apiEditNames;
    std::optional<std::size_t> m_apiEditSelected;
    std::string m_apiEditBaseline;
    bool m_apiChoosingEnvironment = false, m_apiProfileMode = false;
    std::string m_apiRoot;
    modules::ApiResponse m_apiResponse;
    int m_apiResponseView = 0;
    int m_apiSaveStage = 0;
    std::future<std::string> m_apiSaveFuture;
    std::shared_ptr<std::atomic_bool> m_apiSaveCancel;
    std::string m_apiSavePath;
    std::uint64_t m_apiSaveGeneration = 0;
    std::string m_apiFile, m_apiModule;
    std::uint64_t m_apiRevision = 0, m_apiGeneration = 0;
    std::optional<std::size_t> m_apiSelected;
    bool m_apiShowing = false, m_apiConfirmed = false;
    std::future<modules::SqlChangePreview> m_sqlChangeFuture;
    std::future<std::string> m_sqlChangeSaveFuture;
    std::shared_ptr<std::atomic_bool> m_sqlChangeCancel;
    std::future<modules::SqlRowDiff> m_sqlRowFuture;
    std::shared_ptr<std::atomic_bool> m_sqlRowCancel;
    modules::SqlRowDiff m_sqlRowDiff;
    bool m_sqlRowShowing = false;
    std::size_t m_sqlRowIndex = 0;
    std::uint64_t m_sqlRowGeneration = 0;
    modules::SqlChangePreview m_sqlChange;
    std::shared_ptr<std::string> m_sqlChangeBindingsSnapshot;
    std::string m_sqlChangeBindingsPath;
    bool m_sqlChangeMode = false;
    int m_sqlChangeStage = 0;
    std::future<modules::SqlQueryResult> m_sqlFuture;
    std::shared_ptr<const std::string> m_sqlSnapshot;
    std::future<modules::SqlColumnsResult> m_sqlColumnsFuture;
    std::shared_ptr<std::atomic_bool> m_sqlColumnsCancel;
    std::shared_ptr<const std::string> m_sqlColumnsSnapshot;
    modules::SqlColumnsResult m_sqlColumns;
    std::size_t m_sqlColumnsObject = 0;
    std::future<modules::SqlSchemaResult> m_sqlSchemaFuture;
    std::shared_ptr<std::atomic_bool> m_sqlSchemaCancel;
    modules::SqlSchemaResult m_sqlSchema;
    std::optional<std::size_t> m_sqlSchemaSelected;
    std::uint64_t m_sqlSchemaGeneration = 0;
    bool m_sqlSchemaShowing = false;
    std::shared_ptr<std::atomic_bool> m_sqlCancel;
    modules::SqlQueryResult m_sqlResult;
    std::future<modules::SqlCsvExport> m_sqlCsvFuture;
    std::future<std::string> m_sqlCsvSaveFuture;
    std::shared_ptr<std::atomic_bool> m_sqlCsvCancel;
    modules::SqlCsvExport m_sqlCsv;
    std::string m_sqlCsvPath;
    int m_sqlCsvStage = 0;
    std::string m_sqlRoot, m_sqlFile, m_sqlModule, m_sqlQuery, m_sqlDatabase;
    std::uint64_t m_sqlRevision = 0, m_sqlGeneration = 0;
    bool m_sqlShowing = false, m_sqlExplain = false, m_sqlBound = false;
    struct VisualConfigResult { modules::VisualConfigDraft draft; std::string error; };
    std::future<VisualConfigResult> m_visualConfigFuture;
    std::future<std::string> m_visualConfigApplyFuture;
    std::shared_ptr<std::atomic_bool> m_visualConfigCancel;
    std::shared_ptr<modules::VisualReview> m_visualConfigReview;
    modules::VisualConfigDraft m_visualConfigDraft;
    modules::VisualConfigChange m_visualConfigChange{};
    std::string m_visualConfigOriginal, m_visualConfigFile;
    std::uint64_t m_visualConfigRevision=0, m_visualConfigGeneration=0;
    int m_visualConfigStage=0, m_visualConfigOperation=0;
    bool m_visualMaskMode=false, m_visualMaskDragging=false, m_visualMaskSelected=false, m_visualConfigFromDrag=false;
    double m_visualMaskStartX=0, m_visualMaskStartY=0;
    ui::VisualPreviewRect m_visualMaskFitted{};
    modules::VisualMask m_visualMask{};
    struct VisualResult { std::shared_ptr<modules::VisualReview> review; ui::VisualReviewPreview preview; std::string error; };
    std::future<VisualResult> m_visualFuture;
    std::future<std::string> m_visualApprovalFuture;
    std::shared_ptr<std::atomic_bool> m_visualCancel;
    std::shared_ptr<modules::VisualReview> m_visualReview;
    ui::VisualReviewPreview m_visualPreview;
    struct VisualCropResult { ui::VisualReviewPreview preview; std::string error; std::size_t x=0, y=0; };
    std::future<VisualCropResult> m_visualCropFuture;
    std::shared_ptr<std::atomic_bool> m_visualCropCancel;
    std::shared_ptr<modules::VisualReview> m_visualCropReview;
    ui::VisualReviewPreview m_visualCropPreview;
    std::size_t m_visualCropX=0, m_visualCropY=0;
    bool m_visualCropMode=false;
    std::string m_visualRoot, m_visualModule, m_visualBaseline, m_visualConfigPath;
    std::uint64_t m_visualGeneration = 0;
    bool m_visualShowing = false, m_visualVisible = false, m_visualConfirm = false, m_visualConfigured = false;
    struct DiagramPreviewResult { modules::DiagramLayout layout; std::string error; };
    std::future<DiagramPreviewResult> m_diagramPreviewFuture;
    std::shared_ptr<std::atomic_bool> m_diagramPreviewCancel;
    modules::DiagramLayout m_diagramPreview;
    std::string m_diagramPreviewFile;
    std::uint64_t m_diagramPreviewRevision = 0;
    std::uint64_t m_diagramPreviewPromptGeneration = 0;
    std::string m_diagramPreviewModule;
    bool m_diagramPreviewVisible = false;
    double m_diagramZoom = 1.0, m_diagramPanX = 0.0, m_diagramPanY = 0.0;
    double m_diagramDragX = 0.0, m_diagramDragY = 0.0;
    bool m_diagramDragging = false;
    std::future<modules::AccessibilityAudit> m_accessibilityFuture;
    modules::AccessibilityAudit m_accessibilityAudit;
    std::string m_accessibilityFile;
    std::uint64_t m_accessibilityRevision = 0;
    std::uint64_t m_accessibilityPromptGeneration = 0;
    std::string m_accessibilityModule;
    bool m_accessibilityShowing = false;
    struct SnippetSession { std::string file, root, snapshot; std::vector<modules::SnippetRange> fields; std::size_t active = 0, finalOffset = 0, finalFieldIndex = 0; std::uint64_t revision = 0; };
    std::optional<SnippetSession> m_snippetSession;
    void synchronizeSnippet(std::uint64_t previousRevision);
    struct ModuleHistoryResult { std::vector<workspace::GitFileHistoryEntry> entries; std::vector<workspace::GitRangeHistoryEntry> ranges; workspace::GitReview diff; std::string error; bool isDiff = false, isSnapshot = false, stoppedAtBoundary = false; };
    std::future<ModuleHistoryResult> m_moduleHistoryFuture;
    std::vector<workspace::GitFileHistoryEntry> m_moduleHistoryEntries;
    std::vector<workspace::GitRangeHistoryEntry> m_moduleRangeHistory;
    workspace::GitReview m_moduleHistoryDiff;
    std::string m_moduleHistoryRoot, m_moduleHistoryFile;
    bool m_moduleHistoryShowing = false, m_moduleHistoryDiffShowing = false;
    bool m_moduleHistorySnapshotShowing = false;
    std::optional<std::size_t> m_moduleHistorySelected;
    bool m_moduleHistoryRange = false;
    std::uint64_t m_moduleHistoryRevision = 0;
    std::size_t m_moduleFetchGeneration = 0;
    std::vector<int> m_moduleChoices;
    std::size_t m_moduleSelected = 0;
    std::size_t m_modulePage = 0;
    ui::PromptKind m_pagedLayoutKind = ui::PromptKind::File;
    std::size_t m_pagedLayoutCapacity = 0;
    bool m_pagedLayoutValid = false;
    std::size_t m_pagedScroll = 0;
    debug::AdapterProcess m_debugAdapter;
    debug::Session m_debugSession;
    std::map<std::string, std::vector<int>> m_debugBreakpoints;
    std::string m_debugSource, m_debugInline, m_debugLastWarning, m_debugLastMessage;
    bool m_debugAwaitArguments = false, m_debugAwaitWorkingDirectory = false, m_debugAwaitEnvironment = false;
    std::string m_debugPendingProgram, m_debugPendingRoot, m_debugPendingSource, m_debugPendingDirectory;
    std::vector<std::string> m_debugPendingArguments;
    std::vector<int> m_debugChoices;
    std::size_t m_debugPage = 0;
    bool m_debugClosing = false;
    double m_debugCloseDeadline = 0.0;
    double m_debugStartDeadline = 0.0;
    double m_debugPauseDeadline = 0.0;
    double m_debugThreadsRefreshAt = 0.0;
    debug::State m_debugLastState = debug::State::Idle;
    std::vector<workspace::GitChange> m_gitChanges;
    std::vector<workspace::GitHistoryEntry> m_gitHistory;
    workspace::GitReview m_gitReview;
    workspace::GitCommitPreview m_gitCommitPreview;
    std::future<modules::SecretScanResult> m_gitSecretFuture;
    modules::SecretScanResult m_gitSecretResult;
    workspace::GitCommitPreview m_gitSecretPreview;
    std::string m_gitSecretRoot, m_gitSecretFailure;
    bool m_gitSecretRequired = false, m_gitSecretFinished = false, m_gitSecretAcknowledged = false;
    workspace::GitUpstream m_gitUpstream;
    workspace::GitRemoteAction m_gitRemoteAction = workspace::GitRemoteAction::Fetch;
    struct GitRemoteResult { bool ok = false; std::string output, error; };
    std::future<GitRemoteResult> m_gitRemoteFuture;
    std::future<GitRemoteResult> m_gitBranchFuture;
    workspace::GitBranches m_gitBranches;
    workspace::GitBranch m_gitBranchTarget;
    std::string m_gitBranchRoot;
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
    std::vector<OpenDocument> m_closedDocuments;
    editor::Document m_document;
    bool m_hasDocument = false;
    std::string m_filePath;
    std::string m_diskContents;
    int m_firstLine = 0;
    double m_editorScrollRemainder = 0.0;
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
    std::string m_languageKind;
    std::filesystem::path m_languagePythonInterpreter;
    std::string m_languageAttemptRoot;
    std::string m_languageFile;
    std::uint64_t m_languageRevision = 0;
    struct LanguageLocation { std::string path; std::size_t line = 0, character = 0; std::string name; };
    std::vector<LanguageLocation> m_languageLocations;
    std::vector<int> m_languageStatusChoices;
    std::size_t m_languageStatusPage = 0;
    bool m_languageSignatureQueued = false;
    std::uint64_t m_languageSignatureRequest = 0, m_languageSignatureRevision = 0;
    std::string m_languageSignatureFile;
    std::size_t m_languageSignatureLine = 0, m_languageSignatureColumn = 0, m_languageSignaturePage = 0;
    double m_languageSignatureDeadline = 0.0;
    std::vector<std::pair<std::string, std::string>> m_languageSignatures;
    std::vector<int> m_languageSignatureChoices;
    std::vector<int> m_languageLocationChoices;
    std::size_t m_languageLocationPage = 0;
    bool m_languageNavigationQueued = false, m_languageReferences = false, m_languageDeclaration = false, m_languageWorkspaceSymbols = false, m_languageDocumentSymbols = false;
    std::string m_languageSymbolQuery;
    std::uint64_t m_languageNavigationRequest = 0, m_languageNavigationRevision = 0;
    std::string m_languageNavigationFile;
    std::size_t m_languageNavigationLine = 0, m_languageNavigationColumn = 0;
    bool m_languageFormatQueued = false;
    std::uint64_t m_languageFormatRequest = 0, m_languageFormatRevision = 0;
    std::string m_languageFormatFile, m_languageFormatOriginal, m_languageFormatOutput, m_languageFormatRange;
    bool m_languageRenameQueued = false;
    std::uint64_t m_languageRenameRequest = 0, m_languageRenameRevision = 0;
    std::string m_languageRenameFile, m_languageRenameName;
    std::size_t m_languageRenameLine = 0, m_languageRenameColumn = 0, m_languageRenamePage = 0;
    std::vector<editor::lsp::WorkspaceFileEdit> m_languageRenameEdits;
    std::optional<std::size_t> m_languageRenameReviewFile;
    std::vector<int> m_languageRenameChoices;
    double m_languageRenameDeadline = 0.0, m_languageNavigationDeadline = 0.0, m_languageFormatDeadline = 0.0;
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
    bool m_commandSelecting = false;
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
    bool m_allowBackgroundSceneOverride = true;
    float m_backgroundDim = 0.0F;
    std::string m_statusPosition;
    float m_statusPositionReveal = 0.0F;
    float m_uiZoom = 1.0F;
    EditorSettings m_settings;

    std::vector<std::string> m_recentProjects;
    bool m_restoringSession = false;
    std::string m_unrestoredProject;
    std::vector<workspace::recovery::FileContext> m_unrestoredFiles;
    std::optional<workspace::session::State> m_pendingSession;
    bool m_rememberSessionChoice = false;
    std::vector<std::string> m_suggestionPaths;
    std::vector<std::string> m_suggestionDetails;
    std::vector<std::size_t> m_suggestionFunctionIndices;
    std::vector<ui::PathSuggestion> m_suggestions;
    std::size_t m_suggestionScroll = 0;
    double m_menuScrollRemainder = 0.0;
    ui::PromptKind m_menuScrollKind = ui::PromptKind::File;
    int m_menuViewportWidth = 0, m_menuViewportHeight = 0;
    bool m_menuViewportValid = false;
    bool m_menuReselectionRequired = false;
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
    double m_compileScrollRemainder = 0.0;
    float m_compileTerminalReveal = 0.0F;
    float m_compileTerminalVelocity = 0.0F;
    float m_compileTerminalOffsetX = 0.0F;
    float m_compileTerminalOffsetY = 0.0F;
    bool m_compileTerminalCloseRequested = false;

    std::string m_windowTitle = "Lucid";
    std::string m_settingsRoot;
    std::string m_settingsBaselineRoot;
    std::optional<std::string> m_settingsBaseline;
    std::string m_settingsError;
    bool m_settingsErrorHovered = false;
    bool m_settingsPanelVisible = false;
    int m_settingsMenuPage = 0;
    std::size_t m_backgroundScenePage = 0;
    std::vector<int> m_backgroundSceneChoices;
    ui::SettingsControl m_settingsHovered = ui::SettingsControl::None;
    std::uint64_t m_settingsRevision = ~std::uint64_t{0};

    std::vector<editor::Diagnostic> m_diagnostics;
    std::vector<int> m_diagnosticListChoices;
    std::size_t m_diagnosticListPage = 0;
    workspace::ProjectSearchResults m_projectSearchResults;
    std::vector<bool> m_projectSearchSelected;
    std::string m_projectSearchQuery, m_projectReplacement;
    std::vector<std::string> m_projectSearchHistory;
    std::string m_projectSearchDraft;
    std::size_t m_projectSearchHistoryPosition = 0;
    std::size_t m_projectSearchPage = 0;
    std::optional<std::size_t> m_lastProjectSearchMatch;
    std::optional<std::size_t> m_projectReviewFile;
    bool m_projectSearchReplace = false, m_projectSearchWholeWord = false, m_projectSearchCaseSensitive = true, m_projectSearchIncludeGenerated = false, m_projectSearchShowingSkipped = false;
    bool m_projectSearchEditingFilter = false;
    std::string m_projectSearchPathFilter, m_projectSearchFilterQuery;
    std::future<workspace::ProjectSearchResults> m_projectSearchFuture;
    std::shared_ptr<std::atomic_bool> m_projectSearchCancel;
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
    std::vector<std::size_t> m_aiLineOffsets;
    std::size_t m_aiWrapColumns = 0;
    std::size_t m_aiScroll = 0;
    std::string m_aiFile;
    std::uint64_t m_aiRevision = 0;
    editor::TextPosition m_aiStart{};
    editor::TextPosition m_aiEnd{};
    bool m_aiReady = false;
    bool m_aiCleanup = false;
    bool m_aiDebugExplanation = false;
    bool m_aiDiagnosticExplanation = false;
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
