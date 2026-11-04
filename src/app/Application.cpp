#include "ai/CodeCleanup.hpp"
#include "ai/OpenRouter.hpp"
#include "platform/security/Credentials.hpp"
#include "app/Application.hpp"
#include "ai/LocalModel.hpp"
#include "ai/ModelDownload.hpp"

#include "core/Version.hpp"
#include "editor/compile/CompileScript.hpp"
#include "editor/completion/Completion.hpp"
#include "editor/lsp/TextEdits.hpp"
#include "editor/diagnostics/Diagnostics.hpp"
#include "editor/search/TextSearch.hpp"
#include "editor/json/Json.hpp"
#include "logging/Log.hpp"
#include "text/Utf8.hpp"
#include "text/Wrap.hpp"
#include "ui/Overlay.hpp"
#include "ui/Welcome.hpp"
#include "ui/theme/Theme.hpp"
#include "workspace/PathSearch.hpp"
#include "workspace/recovery/History.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

namespace lucid {
namespace {
using SettingsChoice = std::pair<const char*, ui::SettingsControl>;
constexpr std::array<const char*, 10> backgroundNames = {"City transit", "Lunar horizon", "Mountain parallax", "", "Ocean lighthouse", "", "Orbital station", "Forest fireflies", "", ""};
// Keep the persisted IDs of the remaining scenes stable.
constexpr std::array<int, 6> backgroundChoices = {0, 1, 2, 4, 6, 7};
const std::vector<SettingsChoice>& settingsChoices(int page) {
    using C = ui::SettingsControl;
    static const std::vector<std::vector<SettingsChoice>> groups = {
        {},
        {{"Lucid dark", C::ThemeLucidDark}, {"Graphite", C::ThemeGraphite}, {"Lucid light", C::ThemeLucidLight}},
        {{"Accent 1", C::PrimaryColor0}, {"Accent 2", C::PrimaryColor1}, {"Accent 3", C::PrimaryColor2}, {"Accent 4", C::PrimaryColor3}, {"Accent 5", C::PrimaryColor4}, {"Accent 6", C::PrimaryColor5}, {"Accent 7", C::PrimaryColor6}, {"Accent 8", C::PrimaryColor7}},
        {{"75%", C::Zoom75}, {"100%", C::Zoom100}, {"125%", C::Zoom125}, {"150%", C::Zoom150}},
        {{"Line", C::CursorLine}, {"Block", C::CursorBlock}, {"Underscore", C::CursorUnderscore}, {"Kitty", C::CursorKitty}, {"Toggle cursor blink", C::CursorBlink}},
        {{"Classic", C::IconClassic}, {"Accent", C::IconAccent}, {"Outline", C::IconOutline}},
        {{"Toggle syntax errors", C::ShowSyntaxErrors}, {"Toggle line numbers", C::ShowLineNumbers}, {"Toggle current-line highlight", C::HighlightCurrentLine}, {"Toggle high contrast", C::HighContrast}, {"Startup session", C::StartupSession}},
        {{"Half speed", C::SpeedHalf}, {"Normal speed", C::SpeedNormal}, {"Double speed", C::SpeedDouble}, {"Toggle background motion", C::BackgroundMotion}, {"Background scene", C::BackgroundScene}, {"Background colors", C::BackgroundColorMode}}
    };
    return groups.at(static_cast<std::size_t>(page));
}

[[nodiscard]] bool isTaskPrompt(ui::PromptKind kind) {
    return kind == ui::PromptKind::TaskList || kind == ui::PromptKind::TaskActions ||
        kind == ui::PromptKind::TaskSubtasks || kind == ui::PromptKind::TaskHistory ||
        kind == ui::PromptKind::TaskTier || kind == ui::PromptKind::TaskInput;
}

[[nodiscard]] bool isAdaptivePagedPrompt(ui::PromptKind kind) {
    using K = ui::PromptKind;
    return isTaskPrompt(kind) || kind == K::RecoveryList || kind == K::RecoveryDetail ||
        kind == K::SessionRestore || kind == K::LanguageLocations || kind == K::Diagnostics ||
        kind == K::ProjectSearchResults || kind == K::ProjectReplaceReview ||
        kind == K::ModuleList || kind == K::ModuleActions || kind == K::ModuleCommands || kind == K::ModulePreview || kind == K::ModuleCatalog ||
        kind == K::DebugMenu || kind == K::DebugStack || kind == K::DebugThreads || kind == K::DebugVariables || kind == K::DebugOutput ||
        kind == K::GitChanges || kind == K::GitFile || kind == K::GitDiff || kind == K::GitCommitReview ||
        kind == K::GitRemoteReview || kind == K::GitHistory;
}

[[nodiscard]] bool sessionProjectAvailable(const workspace::session::State& state) {
    std::error_code error;
    const std::filesystem::path root(state.project);
    return !state.project.empty() && root.is_absolute() &&
           std::filesystem::is_directory(root, error) && !error;
}

[[nodiscard]] bool acceptsPromptText(ui::PromptKind kind) {
    using K = ui::PromptKind;
    switch (kind) {
    case K::Project: case K::File: case K::Function: case K::Ai:
    case K::ProjectSearch: case K::ProjectReplace: case K::AiModelInput:
    case K::Find: case K::Replace: case K::OpenFiles: case K::DebugInput:
    case K::GitCommitInput: case K::TaskList: case K::TaskInput: case K::ModuleImport: case K::ModuleCatalog:
        return true;
    default:
        return false;
    }
}

constexpr std::size_t kIndentWidth = 4;
constexpr std::size_t kMaxTerminalInput = 512;

void appendUtf8(std::string& output, char32_t codepoint) {
    if (codepoint <= 0x7F) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

void popUtf8(std::string& text) {
    if (text.empty()) {
        return;
    }
    std::size_t index = text.size() - 1;
    while (index > 0 &&
           (static_cast<unsigned char>(text[index]) & 0xC0) == 0x80) {
        --index;
    }
    text.erase(index);
}

[[nodiscard]] std::string normalizeRelative(std::string_view raw) {
    if (raw.empty()) {
        return {};
    }

    const bool trailingSlash = raw.back() == '/' || raw.back() == '\\';
    std::string result =
        std::filesystem::path{std::string(raw)}.lexically_normal().generic_string();

    while (result.rfind("./", 0) == 0) {
        result.erase(0, 2);
    }
    if (result == ".") {
        return {};
    }
    if (trailingSlash && !result.empty() && result.back() != '/') {
        result.push_back('/');
    }
    return result;
}

[[nodiscard]] std::string expandUserPath(std::string_view raw) {
    std::string path(raw);
    if (path.empty() || path.front() != '~') {
        return path;
    }
    if (path.size() > 1 && path[1] != '/') {
        return path;
    }

    if (const char* home = std::getenv("HOME"); home != nullptr) {
        path.replace(0, 1, home);
    }
    return path;
}

[[nodiscard]] std::filesystem::path homeDirectory() {
    if (const char* home = std::getenv("HOME"); home != nullptr) {
        return std::filesystem::path(home);
    }

    std::error_code error;
    const std::filesystem::path current = std::filesystem::current_path(error);
    return error ? std::filesystem::path{} : current;
}

constexpr std::string_view kProjectsFolderName = "Lucid";

[[nodiscard]] std::filesystem::path projectsDirectory() {
    static const std::filesystem::path directory = [] {
        const std::filesystem::path home = homeDirectory();
        if (home.empty()) {
            return std::filesystem::path{};
        }

        std::filesystem::path path =
            home / "Documents" / std::string(kProjectsFolderName);
        std::error_code error;
        std::filesystem::create_directories(path, error);
        if (error) {
            log::warn("Could not create projects directory " + path.string() + ": " +
                      error.message());
            return std::filesystem::path{};
        }
        return path;
    }();
    return directory;
}

[[nodiscard]] std::string resolveProjectPath(std::string_view raw) {
    if (raw.empty()) {
        return {};
    }

    std::filesystem::path path(expandUserPath(raw));
    if (!path.is_absolute()) {
        const std::filesystem::path base = projectsDirectory();
        if (!base.empty()) {
            path = base / path;
        }
    }
    return path.generic_string();
}

[[nodiscard]] std::vector<std::string>
suggestDirectories(std::string_view query, const std::filesystem::path& fallback,
                   const std::filesystem::path& relativeBase, std::size_t limit,
                   bool showFallbackPath) {
    namespace fs = std::filesystem;

    std::vector<std::string> suggestions;
    if (limit == 0) {
        return suggestions;
    }

    const std::string expanded = expandUserPath(query);
    const auto resolveRelative = [&relativeBase](const fs::path& path) {
        if (path.empty() || path.is_absolute() || relativeBase.empty()) {
            return path;
        }
        return relativeBase / path;
    };

    fs::path scanDirectory;
    std::string displayDirectory;
    std::string nameQuery;

    if (query == "~") {
        scanDirectory = homeDirectory();
        displayDirectory = "~/";
    } else if (expanded.empty()) {
        scanDirectory = fallback;
        if (showFallbackPath) {
            displayDirectory = fallback.string();
            if (!displayDirectory.empty() && displayDirectory.back() != '/') {
                displayDirectory.push_back('/');
            }
        }
    } else if (expanded.back() == '/' || expanded.back() == '\\') {
        scanDirectory = resolveRelative(fs::path(expanded));
        displayDirectory = std::string(query);
        if (!displayDirectory.empty() && displayDirectory.back() != '/') {
            displayDirectory.push_back('/');
        }
    } else {
        const fs::path target = resolveRelative(fs::path(expanded));
        scanDirectory = target.parent_path();
        nameQuery = target.filename().string();
        const std::size_t slash = query.find_last_of("/\\");
        displayDirectory =
            slash == std::string_view::npos
                ? std::string()
                : std::string(query.substr(0, slash + 1));
    }

    if (scanDirectory.empty()) {
        scanDirectory = ".";
    }

    std::error_code error;
    fs::directory_iterator iterator(scanDirectory,
                                    fs::directory_options::skip_permission_denied, error);
    const fs::directory_iterator end;
    std::vector<std::string> names;
    while (!error && iterator != end) {
        const fs::directory_entry& entry = *iterator;
        std::error_code entryError;
        if (entry.is_directory(entryError) && !entryError) {
            std::string name = entry.path().filename().string();
            const bool hidden = !name.empty() && name.front() == '.';
            const bool wantsHidden = !nameQuery.empty() && nameQuery.front() == '.';
            if (!name.empty() && (!hidden || wantsHidden)) {
                names.push_back(std::move(name));
            }
        }
        iterator.increment(error);
    }

    std::sort(names.begin(), names.end());
    for (const std::string_view match :
         workspace::filterPaths(names, nameQuery, limit)) {
        std::string suggestion = displayDirectory + std::string(match);
        if (suggestion.empty() || suggestion.back() != '/') {
            suggestion.push_back('/');
        }
        suggestions.push_back(std::move(suggestion));
    }
    return suggestions;
}

} // namespace

Application::Application(ApplicationConfig config)
    : m_config(std::move(config)), m_window(m_config.window) {
    log::info(std::string(kAppDisplayName) + " " + std::string(kVersionString));

    const std::filesystem::path projects = projectsDirectory();
    if (!projects.empty()) {
        log::info("Projects directory: " + projects.string());
    }

    m_window.setKeyCallback([this](const input::KeyEvent& event) { handleKey(event); });
    m_window.setCharCallback([this](char32_t codepoint) { handleChar(codepoint); });
    m_window.setMouseButtonCallback([this](input::MouseButtonEvent event) {
        event.x /= static_cast<double>(m_uiZoom);
        event.y /= static_cast<double>(m_uiZoom);
        handleMouseButton(event);
    });
    m_window.setMouseMoveCallback([this](input::MouseMoveEvent event) {
        event.x /= static_cast<double>(m_uiZoom);
        event.y /= static_cast<double>(m_uiZoom);
        handleMouseMove(event);
    });
    m_window.setScrollCallback(
        [this](const input::ScrollEvent& event) { handleScroll(event); });

    m_window.setDropCallback([this](std::vector<std::string> paths) {
        if (!paths.empty()) openExternalPath(paths.front());
    });

    platform::configureApplicationWindow(m_window.handle(), m_config.window.title);
    // Native menu commands bypass handleKey, so keep the startup choice modal
    // there too. Otherwise a new prompt can hide the pending session forever.
    const auto afterSessionChoice = [this](auto action) {
        return [this, action]() {
            if (m_pendingSession) {
                setCommandStatus("Choose whether to restore the previous workspace first", false);
                return;
            }
            action();
        };
    };
    platform::installApplicationMenu({
        .openProject = afterSessionChoice([this]() { openProjectPrompt(); }),
        .openFile = afterSessionChoice([this]() { openFilePrompt(); }),
        .save = [this]() { (void)saveDocument(); },
        .closeFile = [this]() {
            if (m_searchBar.isOpen() && m_searchBar.kind() == ui::PromptKind::OpenFiles) closeSwitcherSelection();
            else closeDocument();
        },
        .deleteFile = [this]() { deleteCurrentFile(); },
        .deleteProject = [this]() { deleteCurrentProject(); },
        .settings = afterSessionChoice([this]() { openSettings(); }),
        .runCompile = [this]() { runCompileAtCursor(); },
        .runFunction = afterSessionChoice([this]() { openFunctionPrompt(); }),
        .closeTerminal = [this]() { closeCompileTerminal(); },
        .undo = [this]() { undoDocument(); },
        .redo = [this]() { redoDocument(); },
        .cut = [this]() { cutSelection(); },
        .copy = [this]() { copySelection(); },
        .paste = [this]() { pasteClipboard(); },
        .selectAll = [this]() { selectAllText(); },
        .complete = [this]() { openCompletion(); },
        .askAi = [this]() { if (m_hasDocument && !m_searchBar.isOpen()) openAiPrompt(); },
        .find = afterSessionChoice([this]() { openFind(); }),
        .replace = afterSessionChoice([this]() { openReplace(); }),
        .switchFile = afterSessionChoice([this]() { openDocumentSwitcher(); }),
        .tasks = afterSessionChoice([this]() { openTasks(); }),
        .gitReview = afterSessionChoice([this]() { openGitReview(); }),
        .aiCleanup = afterSessionChoice([this]() { openAiCleanup(); }),
        .modules = afterSessionChoice([this]() { openModules(); }),
        .recovery = afterSessionChoice([this]() { openRecovery(); }),
        .zoomIn = [this]() { changeZoom(0.1F); },
        .zoomOut = [this]() { changeZoom(-0.1F); },
        .zoomReset = [this]() { changeZoom(0.0F); },
        .contrast = [this]() { toggleHighContrast(); },
        .debugger = afterSessionChoice([this]() { openDebugger(); }),
        .breakpoint = [this]() { toggleBreakpoint(); },
    });

    m_renderer = std::make_unique<Renderer>(m_window, m_config.renderer);
    initializeSettings();
    m_renderer->setUiZoom(m_uiZoom);
    m_charAdvance = m_renderer->logicalCharacterAdvance();

    platform::prepareOpenFiles();
    platform::setOpenFileHandler([this](std::string path) { openExternalPath(path); });
    workspace::session::State previous; std::string sessionError;
    const auto sessionSettings = settingsDirectory();
    const bool hasSession = !std::getenv("LUCID_SESSION_DISABLED") && !sessionSettings.empty() && workspace::session::read(sessionSettings.parent_path() / "Session", previous, sessionError);
    if (hasSession) m_recentProjects = previous.recentProjects;
    if (!m_config.initialPath.empty()) openExternalPath(m_config.initialPath);
    else if (hasSession && !previous.project.empty() && !std::getenv("LUCID_DEMO_MENU")) {
        if (m_settings.sessionRestore == "restore" && sessionProjectAvailable(previous)) restoreSession(previous);
        else if (m_settings.sessionRestore == "ask" || m_settings.sessionRestore == "restore") {
            m_pendingSession = std::move(previous);
            m_searchBar.open(ui::PromptKind::SessionRestore);
            clearCommandFeedback();
            if (sessionProjectAvailable(*m_pendingSession))
                setCommandStatus("Choose whether to reopen the previous workspace", false);
            else
                setCommandStatus("Previous project is unavailable; reconnect it or start fresh", true);
            refreshSuggestions();
        }
    }
    if (const char* demoProject = std::getenv("LUCID_DEMO_MENU")) {
        openProject(demoProject);
        openFilePrompt();
    }
}

Application::~Application() {
    if (m_projectSearchCancel) m_projectSearchCancel->store(true);
    if (m_debugAdapter.alive()) {
        stopDebugger();
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (m_debugAdapter.alive() && std::chrono::steady_clock::now() < until) {
            pollDebugger();
            if (m_debugAdapter.alive()) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        m_debugAdapter.close();
    }
    platform::setOpenFileHandler({});
    if (m_aiCancel) m_aiCancel->store(true);
}

void Application::run() {
    while (true) {
        m_window.pollEvents();

        if (m_window.shouldClose()) {
            if (!saveDocument()) {
                m_window.cancelClose();
                checkpointRecovery();
                continue;
            }
            if (!saveActiveTaskContext()) {
                m_window.cancelClose();
                checkpointRecovery();
                setStatus("Could not save task context; Lucid remains open.", true);
                continue;
            }
            std::string sessionError;
            if (!saveSession(sessionError)) { checkpointRecovery(); log::error("Could not save session: " + sessionError); }
            else if (!m_recoveryFile.empty()) { std::error_code ec; std::filesystem::remove(m_recoveryFile, ec); }
            break;
        }

        const double timeSeconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
                .count();
        pollDebugger();
        if (timeSeconds - m_recoveryLastTime >= 1.0) { checkpointRecovery(); m_recoveryLastTime = timeSeconds; }
        const auto frame = makeFrameState(timeSeconds);
        syncAccessibility(frame);
        m_renderer->drawFrame(frame);

        if (m_compileTerminalCloseRequested) {
            m_compileTerminalCloseRequested = false;
            closeCompileTerminal();
        } else if (m_compileTerminalWindow != nullptr) {
            if (m_compileTerminalWindow->shouldClose()) {
                closeCompileTerminal();
            } else if (m_compileTerminalRenderer != nullptr) {
                int width = 0;
                int height = 0;
                m_compileTerminalWindow->framebufferSize(width, height);
                if (width > 0 && height > 0) {
                    m_compileTerminalRenderer->drawFrame(
                        makeCompileTerminalFrameState(timeSeconds));
                }
            }
        }

        m_charAdvance = m_renderer->logicalCharacterAdvance();
    }

    m_renderer->waitIdle();
    log::info("Lucid closed.");
}

void Application::handleKey(const input::KeyEvent& event) {
#if defined(__APPLE__)
    const bool commandDown = event.super;
#else
    const bool commandDown = event.control;
#endif

    if (m_pendingSession && m_searchBar.isOpen() && m_searchBar.kind() == ui::PromptKind::SessionRestore) {
        handleCommandKey(event, commandDown);
        return;
    }

    if (m_compileTerminalFocused && m_compileTerminalVisible && commandDown &&
        event.action == input::KeyAction::Press && event.key == input::Key::A) {
        selectAllCompileOutput(); return;
    }
    if (m_compileTerminalFocused && m_compileTerminalVisible && commandDown &&
        event.action == input::KeyAction::Press && event.key == input::Key::C &&
        m_compileSelectionAnchor != m_compileSelectionEnd) { copyCompileSelection(); return; }

    if (event.action == input::KeyAction::Press && commandDown) {
        if (event.key == input::Key::Equal) { changeZoom(0.1F); return; }
        if (event.key == input::Key::Minus) { changeZoom(-0.1F); return; }
        if (event.key == input::Key::Zero) { changeZoom(0.0F); return; }
        if (event.key == input::Key::U && event.shift) { toggleHighContrast(); return; }
    }
    if (m_settingsPanelVisible) {
        if (event.action == input::KeyAction::Press &&
            event.key == input::Key::Escape) {
            closeSettingsPanel();
            return;
        }
        if (event.action == input::KeyAction::Press && commandDown &&
            event.key == input::Key::Comma) {
            closeSettingsPanel();
            return;
        }
        if (event.action == input::KeyAction::Press || event.action == input::KeyAction::Repeat) {
            if (event.key == input::Key::Tab || event.key == input::Key::Down || event.key == input::Key::Right) {
                moveSettingsFocus(event.key == input::Key::Tab && event.shift ? -1 : 1);
            } else if (event.key == input::Key::Up || event.key == input::Key::Left) {
                moveSettingsFocus(-1);
            } else if (event.key == input::Key::Enter || event.key == input::Key::Space) {
                activateSettingsControl(m_settingsHovered);
            }
        }
        return;
    }

    if (event.action == input::KeyAction::Press && commandDown && event.shift &&
        event.key == input::Key::Space && m_hasDocument && !m_searchBar.isOpen() &&
        !m_compileTerminalFocused) {
        openCompletion();
        return;
    }
    if (m_completionOpen && event.action != input::KeyAction::Release) {
        if (event.key == input::Key::Escape) { m_completionOpen = false; return; }
        if (event.key == input::Key::Up || event.key == input::Key::Down) {
            if (!m_completions.empty()) m_completionSelected = std::clamp(
                m_completionSelected + (event.key == input::Key::Down ? 1 : -1),
                0, static_cast<int>(m_completions.size()) - 1);
            return;
        }
        if ((event.key == input::Key::Enter || event.key == input::Key::Tab) && !m_completions.empty()) {
            const auto line = m_document.cursorLine();
            const auto end = m_document.cursorColumn();
            const std::string chosen = m_completions[static_cast<std::size_t>(m_completionSelected)];
            m_document.setCursor(line, m_completionStart);
            m_document.extendSelectionTo(line, end);
            m_document.pasteUtf8(chosen);
            m_completionOpen = false;
            return;
        }
        if (commandDown || event.alt || event.key == input::Key::Left ||
            event.key == input::Key::Right || event.key == input::Key::Home ||
            event.key == input::Key::End || event.key == input::Key::Delete ||
            event.key == input::Key::Enter || event.key == input::Key::Tab)
            m_completionOpen = false;
    }

    if (m_searchBar.isOpen() && m_searchBar.kind() == ui::PromptKind::AiKeyInput && commandDown && event.key != input::Key::V) return;
    if (m_searchBar.isOpen() && m_searchBar.kind() == ui::PromptKind::ProjectSearch && commandDown && event.shift &&
        event.key == input::Key::W && event.action == input::KeyAction::Press) {
        if (m_projectSearchFuture.valid()) { setCommandStatus("Search is running; cancel it before changing options", false); return; }
        m_projectSearchWholeWord = !m_projectSearchWholeWord;
        setCommandStatus(m_projectSearchWholeWord ? "Whole words on; Ctrl/Command+Shift+W toggles" : "Whole words off; Ctrl/Command+Shift+W toggles", false);
        return;
    }
    if (m_searchBar.isOpen() && acceptsPromptText(m_searchBar.kind()) && commandDown &&
        (event.key == input::Key::Backspace || event.key == input::Key::Delete)) {
        handleCommandKey(event, commandDown);
        return;
    }
    if (m_searchBar.isOpen() && commandDown && event.key == input::Key::V && event.action == input::KeyAction::Press) {
        pasteClipboard();
        return;
    }
    if (m_searchBar.isOpen() && m_searchBar.kind() == ui::PromptKind::OpenFiles && commandDown && !event.shift &&
        event.key == input::Key::W && event.action == input::KeyAction::Press) {
        closeSwitcherSelection(); return;
    }
    if (event.action == input::KeyAction::Press && commandDown) {
        switch (event.key) {
        case input::Key::E:
            if (event.shift) reopenClosedDocument(); else openDocumentSwitcher();
            return;
        case input::Key::B:
            if (event.shift) toggleBreakpoint();
            return;
        case input::Key::M:
            if (event.shift) openModules();
            return;
        case input::Key::L:
            if (event.shift) openAiCleanup();
            return;
        case input::Key::G:
            if (event.shift) openGitReview();
            return;
        case input::Key::T:
            if (event.shift) openTasks();
            return;
        case input::Key::F:
            if (event.shift) { openProjectSearch(false); return; }
#if defined(__APPLE__)
            if (event.alt) openReplace();
            else openFind();
#else
            openFind();
#endif
            return;
#if !defined(__APPLE__)
        case input::Key::H:
            if (event.shift) { openProjectSearch(true); return; }
            openReplace();
            return;
#endif
        case input::Key::K:
            if (event.shift && m_hasDocument && !m_searchBar.isOpen()) {
                m_document.deleteLine();
                m_followCursor = true;
            } else {
                openFilePrompt();
            }
            return;
        case input::Key::J:
            if (!event.shift && !event.alt && m_hasDocument && !m_searchBar.isOpen()) {
                m_document.joinLines(); m_followCursor = true;
            }
            return;
        case input::Key::N:
            openFilePrompt();
            return;
        case input::Key::O:
            if (event.shift) { reopenProjectSearchResults(); return; }
            openProjectPrompt();
            return;
        case input::Key::P:
            openProjectPrompt();
            return;
        case input::Key::A:
            if (event.shift) {
                if (m_hasDocument && !m_searchBar.isOpen()) openAiPrompt();
                else setStatus("Open a file before asking AI", true);
            } else selectAllText();
            return;
        case input::Key::C:
            copySelection();
            return;
        case input::Key::D:
            if (event.shift) { openDebugger(); return; }
            if (m_hasDocument && !m_searchBar.isOpen()) {
                m_document.duplicateLine();
                m_followCursor = true;
            }
            return;
        case input::Key::X:
            cutSelection();
            return;
        case input::Key::Z:
            if (event.shift) {
                redoDocument();
            } else {
                undoDocument();
            }
            return;
        case input::Key::Y:
            if (event.shift) { openRecovery(); return; }
            redoDocument();
            return;
        case input::Key::V:
            pasteClipboard();
            return;
        case input::Key::Backspace:
        case input::Key::Delete:
            if (m_hasDocument && !m_searchBar.isOpen()) {
                if (event.shift && event.alt) {
                    deleteCurrentProject();
                } else if (event.shift) {
                    deleteCurrentFile();
                } else {
                    // Let the editor handle word deletion (Ctrl on Windows
                    // and Linux) or deletion to a line boundary (Command on macOS).
                    break;
                }
            }
            return;
        case input::Key::Enter:
            if (m_hasDocument && !m_searchBar.isOpen()) {
                if (event.shift) {
                    m_document.insertLineAbove();
                } else {
                    m_document.insertLineBelow();
                }
                m_followCursor = true;
            }
            return;
        case input::Key::Slash:
            if (m_hasDocument && !m_searchBar.isOpen()) {
                m_document.toggleLineComment();
                m_followCursor = true;
            }
            return;
        case input::Key::Comma:
            if (!m_searchBar.isOpen()) {
                openSettings();
            }
            return;
        case input::Key::R:
            if (event.shift) {
                openFunctionPrompt();
            } else if (m_hasDocument && !m_searchBar.isOpen()) {
                runCompileAtCursor();
            }
            return;
        case input::Key::S:
            (void)saveDocument();
            return;
        case input::Key::W:
            closeDocument();
            return;
        default:
            break;
        }
    }

    if (m_searchBar.isOpen()) {
        handleCommandKey(event, commandDown);
        return;
    }

    if (!m_hasDocument && event.action == input::KeyAction::Press && event.key == input::Key::Enter && !commandDown && !event.alt) {
        m_welcomeShortcutsExpanded = !m_welcomeShortcutsExpanded;
        if (m_welcomeShortcutsExpanded) m_welcomeShortcutScroll = 0;
        return;
    }
    if (!m_hasDocument && m_welcomeShortcutsExpanded && event.action != input::KeyAction::Release && (event.key == input::Key::Up || event.key == input::Key::Down)) {
        m_welcomeShortcutScroll = std::clamp(m_welcomeShortcutScroll + (event.key == input::Key::Down ? 1 : -1), 0, 21); return;
    }
    if (m_compileTerminalFocused && m_compileTerminalVisible) {
        if (event.action == input::KeyAction::Release) {
            return;
        }
        if (event.action == input::KeyAction::Press &&
            event.key == input::Key::Escape) {
            focusCompileTerminal(false);
            return;
        }
        if (!m_compileTerminalProcess.alive()) {
            return;
        }

        switch (event.key) {
        case input::Key::Enter:
            if (!m_compileTerminalInput.empty()) {
                m_compileTerminalProcess.appendEcho(m_compileTerminalInput);
            }
            m_compileTerminalProcess.write("\n");
            m_compileTerminalInput.clear();
            break;
        case input::Key::Tab:
            m_compileTerminalProcess.write("\t");
            break;
        case input::Key::Backspace:
            m_compileTerminalProcess.write("\x7f");
            popUtf8(m_compileTerminalInput);
            break;
        case input::Key::Delete:
            m_compileTerminalProcess.write("\x1b[3~");
            break;
        case input::Key::Left:
            m_compileTerminalProcess.write("\x1b[D");
            break;
        case input::Key::Right:
            m_compileTerminalProcess.write("\x1b[C");
            break;
        case input::Key::Up:
            m_compileTerminalProcess.write("\x1b[A");
            break;
        case input::Key::Down:
            m_compileTerminalProcess.write("\x1b[B");
            break;
        case input::Key::Home:
            m_compileTerminalProcess.write("\x1b[H");
            break;
        case input::Key::End:
            m_compileTerminalProcess.write("\x1b[F");
            break;
        default:
            break;
        }
        return;
    }

    if (m_hasDocument) {
        handleEditorKey(event, commandDown);
        if (m_completionOpen) refreshCompletions();
    }
}

void Application::handleCommandKey(const input::KeyEvent& event, bool commandDown) {
    if (event.action == input::KeyAction::Press && event.key == input::Key::Escape) {
        if (m_searchBar.kind() == ui::PromptKind::Ai && m_aiCancel) m_aiCancel->store(true);
        if (m_searchBar.kind() == ui::PromptKind::ProjectSearch && m_projectSearchCancel) m_projectSearchCancel->store(true);
        const auto kind = m_searchBar.kind();
        if (kind == ui::PromptKind::SessionRestore) {
            m_pendingSession.reset(); m_rememberSessionChoice = false;
            m_searchBar.close(); return;
        }
        if (kind == ui::PromptKind::ProjectReplace || kind == ui::PromptKind::ProjectReplaceReview) {
            if (kind == ui::PromptKind::ProjectReplaceReview && m_projectReviewFile) {
                m_projectReviewFile.reset(); m_projectSearchPage = 0; m_selectedSuggestion = -1;
                clearCommandFeedback(); refreshSuggestions(); return;
            }
            m_projectSearchPage = 0; m_searchBar.open(ui::PromptKind::ProjectSearchResults); clearCommandFeedback(); refreshSuggestions(); return;
        }
        if (kind == ui::PromptKind::ProjectSearchResults) {
            m_searchBar.open(ui::PromptKind::ProjectSearch); m_searchBar.setQuery(m_projectSearchQuery);
            m_projectSearchHistoryPosition = m_projectSearchHistory.size(); m_projectSearchDraft = m_projectSearchQuery;
            clearCommandFeedback(); return;
        }
        if (kind == ui::PromptKind::AiSettings) {
            if (m_aiDownloadingModel && m_aiCancel) m_aiCancel->store(true);
            m_settingsMenuPage = 0;
            m_searchBar.open(ui::PromptKind::SettingsMenu);
            clearCommandFeedback();
            setCommandStatus("Choose a setting; Enter opens, Escape closes", false);
            selectSuggestionRow(7);
            return;
        }
        if (kind == ui::PromptKind::SettingsMenu && m_settingsMenuPage != 0) {
            const int previousPage = m_settingsMenuPage;
            m_settingsMenuPage = previousPage == 8 ? 7 : 0;
            clearCommandFeedback();
            setCommandStatus(m_settingsMenuPage == 0 ? "Choose a setting; Enter opens, Escape closes" : "Enter applies; Escape goes back", false);
            selectSuggestionRow(previousPage == 8 ? 4U : static_cast<std::size_t>(previousPage - 1));
            return;
        }
        if (kind == ui::PromptKind::AiKeyInput || kind == ui::PromptKind::AiModelInput) {
            std::fill(m_aiSecret.begin(), m_aiSecret.end(), '\0'); m_aiSecret.clear();
            openAiSettings(); selectSuggestionRow(kind == ui::PromptKind::AiKeyInput ? 6U : static_cast<std::size_t>(m_aiSettingsWorkflow * 2 + 1)); return;
        }
        if (kind == ui::PromptKind::DebugStack || kind == ui::PromptKind::DebugThreads || kind == ui::PromptKind::DebugVariables || kind == ui::PromptKind::DebugOutput || kind == ui::PromptKind::DebugInput) { m_debugAwaitArguments = false; m_debugAwaitWorkingDirectory = false; m_debugAwaitEnvironment = false; m_debugPage = 0; m_searchBar.open(ui::PromptKind::DebugMenu); clearCommandFeedback(); refreshSuggestions(); return; }
        if (kind == ui::PromptKind::RecoveryDetail) { m_recoveryPage = 0; m_searchBar.open(ui::PromptKind::RecoveryList); refreshSuggestions(); return; }
        if (kind == ui::PromptKind::ModuleCommands) { m_modulePage = 0; m_searchBar.open(ui::PromptKind::ModuleActions); refreshSuggestions(); return; }
        if (kind == ui::PromptKind::ModuleImport || kind == ui::PromptKind::ModulePreview || kind == ui::PromptKind::ModuleCatalog) { ++m_moduleFetchGeneration; m_modulePage = 0; m_searchBar.open(ui::PromptKind::ModuleList); clearCommandFeedback(); refreshSuggestions(); return; }
        if (kind == ui::PromptKind::ModuleActions) { m_modulePage = 0; m_searchBar.open(ui::PromptKind::ModuleList); refreshSuggestions(); return; }
        if (kind == ui::PromptKind::GitDiff) { m_gitPage = 0; m_searchBar.open(ui::PromptKind::GitFile); clearCommandFeedback(); refreshSuggestions(); return; }
        if (kind == ui::PromptKind::GitFile || kind == ui::PromptKind::GitCommitReview || kind == ui::PromptKind::GitRemoteReview || kind == ui::PromptKind::GitHistory) { m_gitPage = 0; m_searchBar.open(ui::PromptKind::GitChanges); clearCommandFeedback(); refreshSuggestions(); return; }
        if (kind == ui::PromptKind::GitCommitInput) { m_gitPage = 0; m_searchBar.open(ui::PromptKind::GitCommitReview); clearCommandFeedback(); refreshSuggestions(); return; }
        if (kind == ui::PromptKind::TaskSubtasks || kind == ui::PromptKind::TaskHistory) {
            showTaskView(ui::PromptKind::TaskActions);
            return;
        }
        if (kind == ui::PromptKind::TaskInput) {
            if (m_taskInputKind == TaskInputKind::Identity && m_settings.taskIdentity.empty()) {
                m_searchBar.close();
            } else if (m_taskInputKind == TaskInputKind::Subtask) {
                showTaskView(ui::PromptKind::TaskSubtasks);
            } else if (m_taskInputKind == TaskInputKind::Assignee || m_taskInputKind == TaskInputKind::Solution) {
                showTaskView(ui::PromptKind::TaskActions);
            } else {
                showTaskView(ui::PromptKind::TaskList);
            }
            return;
        }
        if (kind == ui::PromptKind::TaskActions || kind == ui::PromptKind::TaskTier) {
            showTaskView(ui::PromptKind::TaskList);
            return;
        }
        m_searchBar.close();
        return;
    }

    if (!m_searchBar.isOpen()) {
        return;
    }

    if (m_searchBar.kind() == ui::PromptKind::ProjectSearch && !m_projectSearchFuture.valid() && commandDown && !event.alt && !event.shift &&
        (event.action == input::KeyAction::Press || event.action == input::KeyAction::Repeat) &&
        (event.key == input::Key::Up || event.key == input::Key::Down)) {
        const auto size = m_projectSearchHistory.size();
        m_projectSearchHistoryPosition = std::min(m_projectSearchHistoryPosition, size);
        if ((event.key == input::Key::Up && m_projectSearchHistoryPosition == 0) ||
            (event.key == input::Key::Down && m_projectSearchHistoryPosition == size)) return;
        if (event.key == input::Key::Up && m_projectSearchHistoryPosition > 0) {
            if (m_projectSearchHistoryPosition == size) m_projectSearchDraft = m_searchBar.query();
            --m_projectSearchHistoryPosition;
        } else if (event.key == input::Key::Down && m_projectSearchHistoryPosition < size) {
            ++m_projectSearchHistoryPosition;
        }
        m_searchBar.setQuery(m_projectSearchHistoryPosition == size ? m_projectSearchDraft : m_projectSearchHistory[m_projectSearchHistoryPosition]);
        setCommandStatus("Ctrl/Command+Up or Down: search history", false);
        return;
    }


    if (m_searchBar.kind() == ui::PromptKind::AiKeyInput) {
        if ((event.action == input::KeyAction::Press || event.action == input::KeyAction::Repeat) && event.key == input::Key::Backspace) {
            if (!m_aiSecret.empty()) { m_aiSecret.back() = '\0'; m_aiSecret.pop_back(); }
            m_searchBar.setQuery(std::string(m_aiSecret.size(), '*')); return;
        }
    }
    if (m_searchBar.kind() == ui::PromptKind::Ai && m_aiReady &&
        event.action != input::KeyAction::Release) {
        const auto visible = visibleSuggestionCount();
        const auto maximum = m_aiLines.size() > visible ? m_aiLines.size() - visible : 0;
        if (event.key == input::Key::Up && m_aiScroll > 0) --m_aiScroll;
        if (event.key == input::Key::Down && m_aiScroll < maximum) ++m_aiScroll;
        if (event.key == input::Key::Up || event.key == input::Key::Down) { m_selectedSuggestion = -1; refreshSuggestions(); return; }
        if (event.key == input::Key::Tab) return;
        refreshSuggestions();
    }
    if (event.action == input::KeyAction::Press && event.key == input::Key::Enter && m_menuReselectionRequired) {
        setCommandStatus("List changed; select an option again", false);
        return;
    }
    const bool hadSelection = m_selectedSuggestion >= 0;
    std::optional<std::pair<std::string, std::string>> selectedBefore;
    if (event.action == input::KeyAction::Press && event.key == input::Key::Enter &&
        m_selectedSuggestion >= 0 && static_cast<std::size_t>(m_selectedSuggestion) < m_suggestions.size()) {
        const auto& item = m_suggestions[static_cast<std::size_t>(m_selectedSuggestion)];
        selectedBefore.emplace(item.path, item.detail);
    }
    refreshSuggestions();

    if (event.action == input::KeyAction::Press && event.key == input::Key::Enter && hadSelection) {
        const bool valid = m_selectedSuggestion >= 0 && static_cast<std::size_t>(m_selectedSuggestion) < m_suggestions.size();
        const bool changed = selectedBefore && valid &&
            (m_suggestions[static_cast<std::size_t>(m_selectedSuggestion)].path != selectedBefore->first ||
             m_suggestions[static_cast<std::size_t>(m_selectedSuggestion)].detail != selectedBefore->second);
        if ((selectedBefore && (!valid || changed)) || (!valid && isAdaptivePagedPrompt(m_searchBar.kind()))) {
            m_selectedSuggestion = -1;
            setCommandStatus("List changed; select an option again", false);
            return;
        }
    }

    if (m_searchBar.kind() == ui::PromptKind::SessionRestore &&
        event.action == input::KeyAction::Press && event.key == input::Key::Space &&
        m_selectedSuggestion >= 0 && static_cast<std::size_t>(m_selectedSuggestion) + m_pagedScroll == 2) {
        applyCommand(false); return;
    }

    if (event.action == input::KeyAction::Press && event.key == input::Key::Enter) {
        if (m_searchBar.kind() == ui::PromptKind::Find) {
            if (m_replacePending && !event.shift) {
                if (m_searchBar.query().empty() || !m_document.hasSelection()) {
                    setCommandStatus("Find a match before replacing", true);
                    return;
                }
                m_replaceQuery = m_searchBar.query();
                m_replaceFile = m_filePath;
                m_replaceRevision = m_document.revision();
                m_searchBar.open(ui::PromptKind::Replace);
                clearCommandFeedback();
                setCommandStatus("Enter: replace this match  Escape: cancel", false);
            } else {
                findInDocument(true, event.shift);
            }
            return;
        }
        if (m_searchBar.kind() == ui::PromptKind::Replace) {
            applyReplacement();
            return;
        }
        applyCommand(commandDown);
        return;
    }

    if (event.action == input::KeyAction::Press && event.key == input::Key::Tab) {
        if (m_searchBar.kind() == ui::PromptKind::AiSettings || m_searchBar.kind() == ui::PromptKind::SettingsMenu || m_searchBar.kind() == ui::PromptKind::SessionRestore || m_searchBar.kind() == ui::PromptKind::ProjectSearchResults || m_searchBar.kind() == ui::PromptKind::ProjectReplaceReview || m_searchBar.kind() == ui::PromptKind::GitRemoteReview || m_searchBar.kind() == ui::PromptKind::GitHistory || m_searchBar.kind() == ui::PromptKind::LanguageLocations || m_searchBar.kind() == ui::PromptKind::Diagnostics) { moveSuggestion(event.shift ? -1 : 1); return; }
        if (isTaskPrompt(m_searchBar.kind()) || m_searchBar.kind() == ui::PromptKind::DebugMenu || m_searchBar.kind() == ui::PromptKind::DebugInput || m_searchBar.kind() == ui::PromptKind::DebugStack || m_searchBar.kind() == ui::PromptKind::DebugThreads || m_searchBar.kind() == ui::PromptKind::DebugVariables || m_searchBar.kind() == ui::PromptKind::DebugOutput || m_searchBar.kind() == ui::PromptKind::RecoveryList || m_searchBar.kind() == ui::PromptKind::RecoveryDetail || m_searchBar.kind() == ui::PromptKind::ModuleList || m_searchBar.kind() == ui::PromptKind::ModuleActions || m_searchBar.kind() == ui::PromptKind::ModuleCommands || m_searchBar.kind() == ui::PromptKind::ModulePreview || m_searchBar.kind() == ui::PromptKind::ModuleCatalog || m_searchBar.kind() == ui::PromptKind::GitChanges || m_searchBar.kind() == ui::PromptKind::GitFile || m_searchBar.kind() == ui::PromptKind::GitDiff || m_searchBar.kind() == ui::PromptKind::GitCommitReview || m_searchBar.kind() == ui::PromptKind::GitCommitInput) {
            if (m_searchBar.kind() != ui::PromptKind::TaskInput && m_searchBar.kind() != ui::PromptKind::GitCommitInput && m_searchBar.kind() != ui::PromptKind::DebugInput) moveSuggestion(event.shift ? -1 : 1);
            return;
        }
        acceptSuggestion();
        return;
    }

    const bool repeatable = event.action == input::KeyAction::Press ||
                            event.action == input::KeyAction::Repeat;
    if (m_searchBar.kind() == ui::PromptKind::ProjectSearch && m_projectSearchFuture.valid()) return;
    if (repeatable && acceptsPromptText(m_searchBar.kind()) &&
        !(m_searchBar.kind() == ui::PromptKind::Ai && m_aiReady)) {
#if defined(__APPLE__)
        const bool wordModifier = event.alt && !commandDown;
#else
        const bool wordModifier = commandDown && !event.alt;
#endif
        if (event.key == input::Key::Left) {
            if (wordModifier) m_searchBar.moveWordLeft(event.shift);
#if defined(__APPLE__)
            else if (commandDown) m_searchBar.moveHome(event.shift);
#endif
            else m_searchBar.moveLeft(event.shift);
            return;
        }
        if (event.key == input::Key::Right) {
            if (wordModifier) m_searchBar.moveWordRight(event.shift);
#if defined(__APPLE__)
            else if (commandDown) m_searchBar.moveEnd(event.shift);
#endif
            else m_searchBar.moveRight(event.shift);
            return;
        }
        if (event.key == input::Key::Home) { m_searchBar.moveHome(event.shift); return; }
        if (event.key == input::Key::End) { m_searchBar.moveEnd(event.shift); return; }
        if ((event.key == input::Key::Backspace || event.key == input::Key::Delete) && (wordModifier || commandDown)) {
            const std::string before(m_searchBar.query());
            if (!m_searchBar.hasSelection()) {
#if defined(__APPLE__)
                if (commandDown) {
                    if (event.key == input::Key::Backspace) m_searchBar.moveHome(true); else m_searchBar.moveEnd(true);
                } else
#endif
                if (event.key == input::Key::Backspace) m_searchBar.moveWordLeft(true); else m_searchBar.moveWordRight(true);
            }
            m_searchBar.deleteSelection();
            if (m_searchBar.query() != before) onCommandQueryChanged();
            return;
        }
        if (event.key == input::Key::Delete) {
            const std::string before(m_searchBar.query());
            m_searchBar.deleteForward();
            if (m_searchBar.query() != before) onCommandQueryChanged();
            return;
        }
    }
    if (repeatable && event.key == input::Key::Up) {
        moveSuggestion(-1);
        return;
    }
    if (repeatable && event.key == input::Key::Down) {
        moveSuggestion(1);
        return;
    }
    if (repeatable && event.key == input::Key::Backspace) {
        if (!acceptsPromptText(m_searchBar.kind()) || (m_searchBar.kind() == ui::PromptKind::Ai && m_aiReady)) return;
        const std::string before(m_searchBar.query());
        m_searchBar.backspace();
        if (m_searchBar.query() != before) onCommandQueryChanged();
    }
}

void Application::handleEditorKey(const input::KeyEvent& event, bool commandDown) {
    if (event.action == input::KeyAction::Release) {
        return;
    }

    m_followCursor = true;

    if (event.key == input::Key::F8 && event.action == input::KeyAction::Press && commandDown && !event.alt && !event.shift) {
        openDiagnosticList(); return;
    }
    if (event.key == input::Key::F8 && event.action == input::KeyAction::Press && !commandDown && !event.alt) {
        navigateDiagnostic(event.shift); return;
    }
    if (event.key == input::Key::F12 && event.action == input::KeyAction::Press && !commandDown && !event.alt) {
        requestLanguageNavigation(event.shift); return;
    }
    if (event.key == input::Key::F && event.action == input::KeyAction::Press && event.alt && event.shift && !commandDown) {
        requestLanguageFormatting(); return;
    }

    if (event.alt && !commandDown && event.key == input::Key::Up) {
        m_document.moveLineUp();
        return;
    }
    if (event.alt && !commandDown && event.key == input::Key::Down) {
        m_document.moveLineDown();
        return;
    }

#if defined(__APPLE__)
    const bool wordModifier = event.alt && !commandDown;
#else
    const bool wordModifier = commandDown && !event.alt;
#endif
    if (wordModifier && (event.key == input::Key::Left || event.key == input::Key::Right)) {
        if (event.key == input::Key::Left) m_document.moveWordLeft(event.shift);
        else m_document.moveWordRight(event.shift);
        return;
    }
    if (wordModifier && !event.shift && (event.key == input::Key::Backspace || event.key == input::Key::Delete)) {
        m_document.deleteWord(event.key == input::Key::Delete);
        return;
    }
#if defined(__APPLE__)
    if (commandDown && !event.alt && !event.shift &&
        (event.key == input::Key::Backspace || event.key == input::Key::Delete)) {
        m_document.deleteToLineBoundary(event.key == input::Key::Delete);
        return;
    }
#endif
    if (commandDown) {
#if defined(__APPLE__)
        if (event.alt && event.key == input::Key::Up) m_document.moveParagraphUp(event.shift);
        else if (event.alt && event.key == input::Key::Down) m_document.moveParagraphDown(event.shift);
        else if (event.key == input::Key::Up) m_document.moveDocumentStart(event.shift);
        else if (event.key == input::Key::Down) m_document.moveDocumentEnd(event.shift);
        else
#else
        if (!event.alt && event.key == input::Key::Up) m_document.moveParagraphUp(event.shift);
        else if (!event.alt && event.key == input::Key::Down) m_document.moveParagraphDown(event.shift);
        else if (event.key == input::Key::Home) m_document.moveDocumentStart(event.shift);
        else if (event.key == input::Key::End) m_document.moveDocumentEnd(event.shift);
        else
#endif
        if (event.key == input::Key::Left) {
            m_document.moveHome(event.shift);
        } else if (event.key == input::Key::Right) {
            m_document.moveEnd(event.shift);
        }
        return;
    }

    switch (event.key) {
    case input::Key::Enter:
        m_document.newline();
        break;
    case input::Key::Tab:
        if (event.shift) {
            m_document.outdentLines(kIndentWidth);
        } else if (m_document.hasSelection()) {
            m_document.indentLines(kIndentWidth);
        } else {
            std::size_t visualColumn = 0;
            const auto& line = m_document.line(m_document.cursorLine());
            for (std::size_t i = 0; i < m_document.cursorColumn(); ++i)
                visualColumn += line[i] == U'\t' ? kIndentWidth : 1;
            m_document.insertSpaces(kIndentWidth - visualColumn % kIndentWidth);
        }
        break;
    case input::Key::Backspace:
        m_document.backspace(kIndentWidth);
        break;
    case input::Key::Delete:
        m_document.deleteForward();
        break;
    case input::Key::Left:
        m_document.moveLeft(event.shift);
        break;
    case input::Key::Right:
        m_document.moveRight(event.shift);
        break;
    case input::Key::Up:
        m_document.moveUp(event.shift);
        break;
    case input::Key::Down:
        m_document.moveDown(event.shift);
        break;
    case input::Key::Home:
        m_document.moveSmartHome(event.shift);
        break;
    case input::Key::End:
        m_document.moveEnd(event.shift);
        break;
    default:
        break;
    }
}

void Application::handleChar(char32_t codepoint) {
    if (m_settingsPanelVisible) {
        return;
    }

    if (m_compileTerminalFocused && m_compileTerminalVisible) {
        if (m_compileTerminalProcess.alive()) {
            std::string encoded;
            appendUtf8(encoded, codepoint);
            m_compileTerminalProcess.write(encoded);
            if (m_compileTerminalInput.size() < kMaxTerminalInput) {
                m_compileTerminalInput += encoded;
            }
        }
        return;
    }

    if (m_searchBar.isOpen()) {
        if (m_searchBar.isOpen() && m_searchBar.kind() == ui::PromptKind::AiKeyInput) {
            if (codepoint >= 33 && codepoint <= 126 && m_aiSecret.size() < 1024) { m_aiSecret.push_back(static_cast<char>(codepoint)); m_searchBar.setQuery(std::string(m_aiSecret.size(), '*')); }
            return;
        }
        if (m_searchBar.isOpen() && (m_searchBar.kind() == ui::PromptKind::AiSettings || m_searchBar.kind() == ui::PromptKind::SettingsMenu || m_searchBar.kind() == ui::PromptKind::SessionRestore || m_searchBar.kind() == ui::PromptKind::GitRemoteReview || m_searchBar.kind() == ui::PromptKind::GitHistory || m_searchBar.kind() == ui::PromptKind::LanguageLocations || m_searchBar.kind() == ui::PromptKind::Diagnostics)) return;
        if (m_searchBar.isOpen() && (m_searchBar.kind() == ui::PromptKind::ProjectSearchResults || m_searchBar.kind() == ui::PromptKind::ProjectReplaceReview || (m_searchBar.kind() == ui::PromptKind::ProjectSearch && m_projectSearchFuture.valid()))) return;
        if (acceptsPromptText(m_searchBar.kind()) && !(m_searchBar.kind() == ui::PromptKind::Ai && m_aiReady)) {
            const std::string before(m_searchBar.query());
            m_searchBar.append(codepoint);
            if (m_searchBar.query() != before) onCommandQueryChanged();
        }
        return;
    }

    if (m_hasDocument) {
        char32_t closing = U'\0';
        switch (codepoint) {
        case U'(': closing = U')'; break;
        case U'[': closing = U']'; break;
        case U'{': closing = U'}'; break;
        case U'\"': closing = U'\"'; break;
        case U'\'': closing = U'\''; break;
        default: break;
        }
        if (closing && m_document.surroundSelection(codepoint, closing)) {
            m_followCursor = true;
            if (m_completionOpen) refreshCompletions();
            return;
        }
        if (isJsonDocument()) {
            if (codepoint == U'"' && m_document.insertJsonQuote()) {
                m_followCursor = true;
                return;
            }
            if (codepoint == U'{' || codepoint == U'[') {
                m_document.insertJsonBlock(
                    codepoint, codepoint == U'{' ? U'}' : U']', kIndentWidth);
                m_followCursor = true;
                return;
            }
            if ((codepoint == U'}' || codepoint == U']') &&
                m_document.movePast(codepoint)) {
                m_followCursor = true;
                return;
            }
        }
        m_document.insert(codepoint);
        m_followCursor = true;
    }
    if (m_completionOpen) refreshCompletions();
}

void Application::undoDocument() {
    if (m_hasDocument && !m_searchBar.isOpen()) {
        m_document.undo();
        m_followCursor = true;
    }
}

void Application::redoDocument() {
    if (m_hasDocument && !m_searchBar.isOpen()) {
        m_document.redo();
        m_followCursor = true;
    }
}

void Application::cutSelection() {
    if (m_searchBar.isOpen()) {
        if (acceptsPromptText(m_searchBar.kind()) && m_searchBar.hasSelection()) {
            m_window.setClipboardText(std::string(m_searchBar.query().substr(m_searchBar.selectionStart(), m_searchBar.selectionEnd() - m_searchBar.selectionStart())));
            m_searchBar.deleteSelection();
            onCommandQueryChanged();
        }
        return;
    }
    if (!m_hasDocument) {
        return;
    }

    if (m_document.hasSelection()) {
        m_window.setClipboardText(m_document.selectionUtf8());
        m_document.deleteSelection();
    } else {
        m_window.setClipboardText(m_document.lineUtf8(m_document.cursorLine()) + "\n");
        m_document.deleteLine();
    }
    m_followCursor = true;
}

void Application::copySelection() {
    if (m_searchBar.isOpen()) {
        if (acceptsPromptText(m_searchBar.kind()) && m_searchBar.hasSelection())
            m_window.setClipboardText(std::string(m_searchBar.query().substr(m_searchBar.selectionStart(), m_searchBar.selectionEnd() - m_searchBar.selectionStart())));
        return;
    }
    if (!m_hasDocument) {
        return;
    }

    std::string text = m_document.selectionUtf8();
    if (text.empty()) {
        text = m_document.lineUtf8(m_document.cursorLine()) + "\n";
    }
    m_window.setClipboardText(text);
}

void Application::pasteClipboard() {
    if (m_searchBar.isOpen()) {
        const auto kind = m_searchBar.kind();
        if (kind == ui::PromptKind::AiKeyInput) {
            std::string pasted = m_window.clipboardText();
            if (m_aiSecret.size() + pasted.size() <= 1024 &&
                std::all_of(pasted.begin(), pasted.end(), [](unsigned char c) { return c >= 33 && c <= 126; }))
                m_aiSecret += pasted;
            else setCommandStatus("Key must contain only printable characters and fit within 1024 bytes", true);
            std::fill(pasted.begin(), pasted.end(), '\0');
            m_searchBar.setQuery(std::string(m_aiSecret.size(), '*'));
        } else if (acceptsPromptText(kind) && !(kind == ui::PromptKind::Ai && m_aiReady) &&
                   !(kind == ui::PromptKind::ProjectSearch && m_projectSearchFuture.valid())) {
            const std::string before(m_searchBar.query());
            const std::string pasted = m_window.clipboardText();
            const bool complete = m_searchBar.appendUtf8(pasted);
            if (m_searchBar.query() != before) onCommandQueryChanged();
            if (!complete) setCommandStatus("Paste shortened: this field is single-line and has a length limit", false);
        }
        return;
    }
    if (!m_hasDocument) {
        return;
    }

    const std::string text = m_window.clipboardText();
    if (!text.empty()) {
        m_document.pasteUtf8(text);
        m_followCursor = true;
    }
}

void Application::selectAllText() {
    if (m_searchBar.isOpen()) {
        if (acceptsPromptText(m_searchBar.kind())) m_searchBar.selectAll();
        return;
    }
    if (m_hasDocument && !m_searchBar.isOpen()) {
        m_document.selectAll();
        m_followCursor = true;
    }
}

void Application::handleMouseButton(const input::MouseButtonEvent& event) {
    if (event.action == input::KeyAction::Press) m_completionOpen = false;
    if (event.button == input::MouseButton::Left &&
        event.action == input::KeyAction::Release) {
        m_compileSelecting = false;
        m_mouseSelecting = false;
        m_commandSelecting = false;
        m_wordSelecting = false;
        m_window.endWindowDrag();
        endDrag();
        return;
    }

    if (event.action != input::KeyAction::Press ||
        event.button != input::MouseButton::Left) {
        return;
    }

    if (!m_hasDocument && !m_searchBar.isOpen() && welcomeExpandContains(event.x, event.y)) {
        m_welcomeShortcutsExpanded = !m_welcomeShortcutsExpanded;
        if (m_welcomeShortcutsExpanded) m_welcomeShortcutScroll = 0;
        return;
    }

    if (titlebarDragContains(event.x, event.y)) {
        m_window.beginWindowDrag();
        return;
    }
    if (m_window.isDraggingWindow()) {
        m_window.endWindowDrag();
    }

    if (m_settingsPanelVisible) {
        const ui::SettingsPanelLayout layout = settingsPanelLayout();
        if (layout.panel.contains(event.x, event.y)) {
            handleSettingsPanelClick(event.x, event.y);
        }
        return;
    }

    if (!m_searchBar.isOpen() && m_compileTerminalVisible &&
        m_compileTerminalWindow == nullptr) {
        const ui::CompileTerminalLayout terminal = compileTerminalLayout();
        if (terminal.closeButton.contains(event.x, event.y)) {
            closeCompileTerminal();
            return;
        }
        if (terminal.titleBar.contains(event.x, event.y)) {
            beginDrag(event.x, event.y, true);
            return;
        }
        if (terminal.panel.contains(event.x, event.y)) {
            focusCompileTerminal(true);
            if (terminal.output.contains(event.x, event.y)) {
                m_compileSelectionSnapshot = std::string(m_compileTerminalProcess.output());
                m_compileSelectionAnchor = m_compileSelectionEnd = compileOutputPosition(event.x, event.y, false);
                m_compileSelecting = true;
            }
            return;
        }
    }

    if (!m_searchBar.isOpen()) {
        focusCompileTerminal(false);
    }

    if (!m_searchBar.isOpen() && !m_settingsError.empty() &&
        settingsErrorLayout().text.contains(event.x, event.y)) {
        openSettingsJson();
        return;
    }

    if (m_hasDocument && m_showDiagnostics && !m_searchBar.isOpen()) {
        const ui::DiagnosticCardLayout cardLayout = diagnosticCardLayout();
        if (!m_activeDiagnosticKey.empty()) {
            if (cardLayout.ignoreButton.contains(event.x, event.y)) {
                m_ignoredDiagnostics.insert(m_activeDiagnosticKey);
                updateActiveDiagnostic();
                return;
            }
            if (cardLayout.card.contains(event.x, event.y)) {
                beginDrag(event.x, event.y, false);
                return;
            }
        }
    }

    if (m_searchBar.isOpen()) {
        if (placeCommandCaretAt(event.x, event.y, event.shift)) {
            m_commandSelecting = true;
            return;
        }
        refreshSuggestions();
        const int index = suggestionIndexAt(event.x, event.y);
        if (index >= 0) {
            // AI preview rows are response text, not actionable suggestions.
            // Insertion must only happen through the explicit Enter action.
            if (m_searchBar.kind() != ui::PromptKind::Ai) {
                m_menuReselectionRequired = false;
                m_selectedSuggestion = index;
                applyCommand(false);
            }
            return;
        }
        if (commandRegionContains(event.x, event.y)) return;
        if (m_searchBar.kind() == ui::PromptKind::SessionRestore) return;
        if (m_searchBar.kind() == ui::PromptKind::AiKeyInput) { std::fill(m_aiSecret.begin(), m_aiSecret.end(), '\0'); m_aiSecret.clear(); }
        m_searchBar.close();
        return;
    }

    if (!m_hasProject) {
        if (welcomeActionContains(event.x, event.y)) {
            openProjectPrompt();
        }
        return;
    }

    if (m_hasDocument) {
        if (const editor::CompileFunction* function =
                compileFunctionAt(event.x, event.y)) {
            runCompileFunction(*function);
            return;
        }
    }

    m_mouseSelecting = placeCaretAt(event.x, event.y, event.shift);
    if (m_mouseSelecting && !event.shift) {
        const double now = std::chrono::duration<double>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        if (now - m_lastClickTime < 0.4 && std::abs(event.x - m_lastClickX) < 5.0 &&
            std::abs(event.y - m_lastClickY) < 5.0) {
            m_clickCount = std::min(m_clickCount + 1, 4);
        } else {
            m_clickCount = 1;
        }
        m_lastClickTime = now;
        m_lastClickX = event.x;
        m_lastClickY = event.y;
        m_wordSelecting = m_clickCount == 2;
        if (m_wordSelecting) {
            m_document.selectWord();
            m_wordSelectionStart = m_document.selectionStart();
            m_wordSelectionEnd = m_document.selectionEnd();
        }
        if (m_clickCount >= 3) m_document.selectLine();
    }
}

void Application::handleMouseMove(const input::MouseMoveEvent& event) {
    if (m_commandSelecting && m_searchBar.isOpen()) {
        (void)placeCommandCaretAt(event.x, event.y, true, true);
        return;
    }
    if (m_compileSelecting && m_compileTerminalWindow == nullptr) {
        m_compileSelectionEnd = compileOutputPosition(event.x, event.y, false);
        return;
    }
    if (m_window.isDraggingWindow()) {
        m_window.updateWindowDrag();
        return;
    }

    if (m_settingsPanelVisible) {
        updateSettingsPanelHover(event.x, event.y);
        m_window.setCursorShape(m_settingsHovered == ui::SettingsControl::None
                                    ? CursorShape::Arrow
                                    : CursorShape::Hand);
        return;
    }

    if (m_dragTarget != DragTarget::None) {
        updateDrag(event.x, event.y);
        m_window.setCursorShape(CursorShape::Hand);
        return;
    }

    if (m_mouseSelecting && m_hasDocument && !m_searchBar.isOpen()) {
        if (m_wordSelecting && placeCaretAt(event.x, event.y, false)) {
            const auto line = m_document.cursorLine();
            const auto column = m_document.cursorColumn();
            m_document.selectWord();
            const auto start = m_document.selectionStart();
            const auto end = m_document.selectionEnd();
            const bool before = line < m_wordSelectionStart.line ||
                (line == m_wordSelectionStart.line && column < m_wordSelectionStart.column);
            if (before) {
                m_document.setCursor(m_wordSelectionEnd.line, m_wordSelectionEnd.column);
                m_document.extendSelectionTo(start.line, start.column);
            } else {
                m_document.setCursor(m_wordSelectionStart.line, m_wordSelectionStart.column);
                m_document.extendSelectionTo(end.line, end.column);
            }
        } else if (!m_wordSelecting) {
            (void)placeCaretAt(event.x, event.y, true);
        }
    }

    if (m_searchBar.isOpen()) {
        const int index = suggestionIndexAt(event.x, event.y);
        if (index >= 0 && index != m_selectedSuggestion) {
            m_menuReselectionRequired = false;
            m_selectedSuggestion = index;
        }
    }

    updateCompileTerminalHover(event.x, event.y);

    m_welcomeActionHovered =
        !m_searchBar.isOpen() && welcomeActionContains(event.x, event.y);
    m_settingsErrorHovered =
        !m_searchBar.isOpen() && !m_settingsError.empty() &&
        settingsErrorLayout().text.contains(event.x, event.y);

    m_diagnosticIgnoreHovered = false;
    if (m_showDiagnostics && !m_searchBar.isOpen() && !m_activeDiagnosticKey.empty()) {
        const ui::DiagnosticCardLayout cardLayout = diagnosticCardLayout();
        m_diagnosticIgnoreHovered =
            cardLayout.ignoreButton.contains(event.x, event.y);
    }

    updateCursorShape(event.x, event.y);
}

void Application::handleScroll(const input::ScrollEvent& event) {
    if (!std::isfinite(event.yOffset) || event.yOffset == 0.0 || m_settingsPanelVisible) {
        return;
    }

    if (m_searchBar.isOpen()) {
        if (m_menuScrollKind != m_searchBar.kind()) {
            m_menuScrollKind = m_searchBar.kind();
            m_menuScrollRemainder = 0.0;
        }
        if (m_menuScrollRemainder * event.yOffset < 0.0) m_menuScrollRemainder = 0.0;
        m_menuScrollRemainder += std::clamp(event.yOffset, -9.0, 9.0);
        const int scrollSteps = static_cast<int>(m_menuScrollRemainder);
        m_menuScrollRemainder -= static_cast<double>(scrollSteps);
        if (scrollSteps == 0) return;
        if (m_searchBar.isOpen() && m_searchBar.kind() == ui::PromptKind::Ai && m_aiReady) {
            const auto visible = visibleSuggestionCount();
            const auto maximum = m_aiLines.size() > visible ? m_aiLines.size() - visible : 0;
            const auto steps = std::abs(scrollSteps);
            if (scrollSteps > 0) m_aiScroll -= std::min(m_aiScroll, static_cast<std::size_t>(steps));
            else m_aiScroll = std::min(maximum, m_aiScroll + static_cast<std::size_t>(steps));
            m_selectedSuggestion = -1; refreshSuggestions();
        } else if (m_searchBar.isOpen() && (m_searchBar.kind() == ui::PromptKind::File || m_searchBar.kind() == ui::PromptKind::Project || m_searchBar.kind() == ui::PromptKind::OpenFiles || m_searchBar.kind() == ui::PromptKind::Function || m_searchBar.kind() == ui::PromptKind::SettingsMenu || m_searchBar.kind() == ui::PromptKind::AiSettings)) {
            const auto steps = static_cast<std::size_t>(std::abs(scrollSteps));
            m_selectedSuggestion = -1;
            if (scrollSteps > 0) {
                m_suggestionScroll -= std::min(m_suggestionScroll, steps);
                refreshSuggestions();
            } else {
                for (std::size_t step = 0; step < steps && m_suggestionHasMoreBelow; ++step) {
                    ++m_suggestionScroll;
                    refreshSuggestions();
                }
            }
        } else if (m_searchBar.isOpen() && isAdaptivePagedPrompt(m_searchBar.kind()) &&
                   (m_pagedScroll > 0 || m_suggestionHasMoreBelow)) {
            const auto steps = std::abs(scrollSteps);
            bool moved = false;
            for (int step = 0; step < steps; ++step) {
                if (scrollSteps > 0 && m_pagedScroll > 0) --m_pagedScroll;
                else if (scrollSteps < 0 && m_suggestionHasMoreBelow) ++m_pagedScroll;
                else break;
                moved = true;
                m_selectedSuggestion = -1;
                refreshSuggestions();
            }
            // A separate wheel gesture at the boundary can turn a page. Do
            // not turn it in the same gesture that revealed its control.
            if (!moved) {
                const std::string_view page = scrollSteps > 0 ? "< Previous page" : "Next page >";
                const auto it = std::find_if(m_suggestions.begin(), m_suggestions.end(), [page](const auto& suggestion) {
                    return suggestion.kind == ui::SuggestionKind::Task && suggestion.path == page;
                });
                if (it != m_suggestions.end()) {
                    m_selectedSuggestion = static_cast<int>(std::distance(m_suggestions.begin(), it));
                    applyCommand(false);
                }
            }
        } else if (m_searchBar.isOpen() && !m_suggestions.empty()) {
            const std::string_view page = scrollSteps > 0 ? "< Previous page" : "Next page >";
            const auto it = std::find_if(m_suggestions.begin(), m_suggestions.end(), [page](const auto& suggestion) {
                return suggestion.kind == ui::SuggestionKind::Task && suggestion.path == page;
            });
            if (it != m_suggestions.end()) {
                m_selectedSuggestion = static_cast<int>(std::distance(m_suggestions.begin(), it));
                applyCommand(false);
            } else moveSuggestion(scrollSteps > 0 ? -1 : 1);
        }
        return;
    }

    if (m_compileTerminalFocused && m_compileTerminalVisible && !m_compileTerminalWindow) {
        scrollCompileOutput(event.yOffset, false);
        return;
    }

    if (!m_hasDocument) {
        if (m_welcomeShortcutsExpanded) m_welcomeShortcutScroll = std::clamp(m_welcomeShortcutScroll + (event.yOffset < 0.0 ? 1 : -1), 0, 21);
        return;
    }

    const int maximum = maxFirstLine();
    if ((m_firstLine <= 0 && event.yOffset > 0.0) ||
        (m_firstLine >= maximum && event.yOffset < 0.0)) {
        m_editorScrollRemainder = 0.0;
        return;
    }
    m_editorScrollRemainder += std::clamp(event.yOffset, -1000.0, 1000.0) * 3.0;
    const int steps = static_cast<int>(m_editorScrollRemainder);
    m_editorScrollRemainder -= static_cast<double>(steps);
    const int target = m_firstLine - steps;
    const int next = std::clamp(target, 0, maximum);
    if (target != next) m_editorScrollRemainder = 0.0;
    if (next != m_firstLine) {
        m_firstLine = next;
        m_followCursor = false;
    }
}

bool Application::placeCaretAt(double x, double y, bool extendSelection) {
    if (!m_hasDocument) {
        return false;
    }

    int width [[maybe_unused]] = 0;
    int height = 0;
    uiSize(width, height);

    const float top = ui::theme::kEditorTop;
    const float bottom = static_cast<float>(height) - ui::theme::kEditorBottom;
    if (y < top || y >= bottom) {
        return false;
    }

    const int row = static_cast<int>((y - top) / ui::theme::kEditorLineHeight);
    const int line =
        std::clamp(m_firstLine + row, 0, static_cast<int>(m_document.lineCount()) - 1);
    const std::u32string& text = m_document.line(static_cast<std::size_t>(line));
    const float advance = m_charAdvance > 0.0F ? m_charAdvance : 7.0F;
    const float codeLeft = ui::theme::kEditorCodeLeft;

    std::size_t column = 0;
    float pen = codeLeft;
    if (x > codeLeft) {
        for (std::size_t index = 0; index < text.size(); ++index) {
            const float characterWidth = text[index] == U'\t' ? advance * 4.0F : advance;
            if (x < pen + characterWidth * 0.5F) {
                break;
            }
            pen += characterWidth;
            column = index + 1;
        }
    }

    if (extendSelection) {
        m_document.extendSelectionTo(static_cast<std::size_t>(line), column);
    } else {
        m_document.setCursor(static_cast<std::size_t>(line), column);
    }
    m_followCursor = true;
    return true;
}

bool Application::placeCommandCaretAt(double x, double y, bool extendSelection, bool dragging) {
    if (!m_searchBar.isOpen() || !acceptsPromptText(m_searchBar.kind()) ||
        (m_searchBar.kind() == ui::PromptKind::Ai && m_aiReady)) return false;
    int width = 0, height = 0;
    uiSize(width, height);
    const float margin = ui::theme::kSearchBarMargin;
    const float right = margin + std::max(
        std::min(ui::theme::kSearchBarWidth, std::max(0.0F, static_cast<float>(width) - margin * 2.0F)),
        std::min(static_cast<float>(width) - margin * 2.0F, 520.0F));
    const float top = ui::commandBarTop(static_cast<float>(height));
    if (!dragging && (x < margin || x > right || y < top || y > top + ui::theme::kSearchBarHeight)) return false;
    const float advance = m_charAdvance > 0.0F ? m_charAdvance : 7.0F;
    const auto query = m_searchBar.query();
    std::size_t offset = 0, cursorCharacters = 0;
    while (offset < m_searchBar.cursor()) { (void)text::nextUtf8(query, offset); ++cursorCharacters; }
    const float origin = margin + ui::theme::kSearchBarPadding - std::max(0.0F,
        margin + ui::theme::kSearchBarPadding + static_cast<float>(cursorCharacters) * advance + 4.0F - right);
    const auto target = static_cast<std::size_t>(std::max(0.0, std::floor((x - origin) / advance + 0.5)));
    offset = 0;
    for (std::size_t i = 0; i < target && offset < query.size(); ++i) (void)text::nextUtf8(query, offset);
    m_searchBar.setCursor(offset, extendSelection);
    return true;
}

bool Application::titlebarDragContains(double x, double y) const {
#if defined(__APPLE__)
    constexpr float kTitlebarHeight = 28.0F;
    constexpr float kTrafficLightsWidth = 72.0F;
    if (x <= kTrafficLightsWidth || y < 0.0F || y >= kTitlebarHeight) {
        return false;
    }
    int width [[maybe_unused]] = 0;
    int height = 0;
    uiSize(width, height);
    const float barTop = ui::commandBarTop(static_cast<float>(height));
    if (m_searchBar.visible() && x >= ui::theme::kSearchBarMargin &&
        x <= ui::theme::kSearchBarMargin + ui::theme::kSearchBarWidth &&
        y >= barTop && y <= barTop + ui::theme::kSearchBarHeight) {
        return false;
    }
    if (!m_settingsError.empty() &&
        settingsErrorLayout().text.contains(x, y)) {
        return false;
    }
    if (m_settingsPanelVisible && settingsPanelLayout().panel.contains(x, y)) {
        return false;
    }
    return true;
#else
    (void)x;
    (void)y;
    return false;
#endif
}

void Application::updateCursorShape(double x, double y) {
    if (titlebarDragContains(x, y)) {
        m_window.setCursorShape(CursorShape::Arrow);
        return;
    }

    if (!m_searchBar.visible() && m_compileTerminalVisible &&
        m_compileTerminalWindow == nullptr) {
        const ui::CompileTerminalLayout terminal = compileTerminalLayout();
        if (terminal.closeButton.contains(x, y) ||
            terminal.titleBar.contains(x, y)) {
            m_window.setCursorShape(CursorShape::Hand);
            return;
        }
        if (terminal.panel.contains(x, y)) {
            m_window.setCursorShape(terminal.input.contains(x, y)
                                        ? CursorShape::IBeam
                                        : CursorShape::Arrow);
            return;
        }
    }

    if (!m_searchBar.visible() && !m_settingsError.empty() &&
        settingsErrorLayout().text.contains(x, y)) {
        m_window.setCursorShape(CursorShape::Hand);
        return;
    }

    if (m_showDiagnostics && !m_searchBar.visible() &&
        !m_activeDiagnosticKey.empty()) {
        const ui::DiagnosticCardLayout card = diagnosticCardLayout();
        if (card.ignoreButton.contains(x, y) || card.card.contains(x, y)) {
            m_window.setCursorShape(CursorShape::Hand);
            return;
        }
    }

    if (m_searchBar.visible() && m_searchBar.isOpen()) {
        int width = 0, height = 0;
        uiSize(width, height);
        const float top = ui::commandBarTop(static_cast<float>(height));
        if (acceptsPromptText(m_searchBar.kind()) && !(m_searchBar.kind() == ui::PromptKind::Ai && m_aiReady) &&
            y >= top && y <= top + ui::theme::kSearchBarHeight && commandRegionContains(x, y)) {
            m_window.setCursorShape(CursorShape::IBeam);
            return;
        }
        const int index = suggestionIndexAt(x, y);
        m_window.setCursorShape(index >= 0 ? CursorShape::Hand : CursorShape::Arrow);
        return;
    }

    if (!m_hasProject) {
        m_window.setCursorShape((welcomeActionContains(x, y) || welcomeExpandContains(x, y)) ? CursorShape::Hand
                                                            : CursorShape::Arrow);
        return;
    }

    if (!m_hasDocument) {
        m_window.setCursorShape(welcomeExpandContains(x, y) ? CursorShape::Hand : CursorShape::Arrow);
        return;
    }

    int width [[maybe_unused]] = 0;
    int height = 0;
    uiSize(width, height);
    const bool overEditor =
        y >= ui::theme::kEditorTop &&
        y < static_cast<float>(height) - ui::theme::kEditorBottom;
    m_window.setCursorShape(overEditor ? CursorShape::IBeam : CursorShape::Arrow);
}

int Application::visibleRowCount() const {
    int width [[maybe_unused]] = 0;
    int height = 0;
    uiSize(width, height);

    const float available = static_cast<float>(height) - ui::theme::kEditorTop -
                            ui::theme::kEditorBottom;
    const float lineHeight = std::max(1.0F, ui::theme::kEditorLineHeight);
    return std::max(1, static_cast<int>(std::floor(available / lineHeight)));
}

int Application::maxFirstLine() const {
    if (!m_hasDocument) {
        return 0;
    }
    return std::max(0, static_cast<int>(m_document.lineCount()) - 1);
}

float Application::suggestionListTop() const {
    int width [[maybe_unused]] = 0;
    int height = 0;
    uiSize(width, height);
    return ui::commandBarTop(static_cast<float>(height)) +
           ui::theme::kSearchBarHeight + ui::commandSuggestionGap(static_cast<float>(height)) +
           static_cast<float>(commandStatusRows()) * ui::theme::kSuggestionLineHeight;
}

std::size_t Application::commandStatusColumns() const {
    int width = 0, height = 0;
    uiSize(width, height);
    const float available = std::max(0.0F, std::min(static_cast<float>(width) - 2.0F * ui::theme::kSearchBarMargin, 520.0F) - 16.0F);
    const float advance = m_charAdvance > 0.0F ? m_charAdvance : 7.0F;
    return static_cast<std::size_t>(std::max(1, static_cast<int>(std::floor(available / advance))));
}

std::size_t Application::commandStatusRows() const {
    std::size_t requested = 0;
    if (m_commandStatus.empty()) {
        requested = m_searchBar.kind() == ui::PromptKind::SettingsMenu || m_searchBar.kind() == ui::PromptKind::AiSettings ||
                    m_searchBar.kind() == ui::PromptKind::SessionRestore ? 1U : 0U;
    } else {
        const auto status = (m_commandStatusIsError ? "Error: " : "") + m_commandStatus;
        requested = std::clamp<std::size_t>(text::wrapLines(status, commandStatusColumns()).size(), 1, 3);
    }

    int width = 0, height = 0;
    uiSize(width, height);
    const float listStart = ui::commandBarTop(static_cast<float>(height)) +
                            ui::theme::kSearchBarHeight + ui::commandSuggestionGap(static_cast<float>(height));
    const int availableRows = std::max(0, static_cast<int>(std::floor(
        (static_cast<float>(height) - listStart - ui::theme::kSearchBarMargin) /
        ui::theme::kSuggestionLineHeight)));
    // Leave room for a choice, and for page controls whenever the window permits.
    const int reservedChoices = availableRows >= 4 ? 3 : std::min(1, availableRows);
    return std::min(requested, static_cast<std::size_t>(availableRows - reservedChoices));
}

std::size_t Application::visibleSuggestionCount() const {
    int width = 0, height = 0;
    uiSize(width, height);
    const auto rows = static_cast<int>(std::floor((static_cast<float>(height) - suggestionListTop() - ui::theme::kSearchBarMargin) /
                                                  ui::theme::kSuggestionLineHeight));
    return static_cast<std::size_t>(std::clamp(rows, 1, ui::theme::kMaxSuggestions));
}

std::size_t Application::pagedContentCount() const {
    const auto visible = visibleSuggestionCount();
    return std::min<std::size_t>(6, visible > 2 ? visible - 2 : 1);
}

int Application::suggestionIndexAt(double x, double y) const {
    if (m_searchBar.kind() == ui::PromptKind::Ai && m_aiReady) return -1;
    if (!m_searchBar.isOpen() || m_suggestions.empty()) {
        return -1;
    }

    int width = 0;
    int height = 0;
    uiSize(width, height);

    const float margin = ui::theme::kSearchBarMargin;
    const float barWidth =
        std::min(ui::theme::kSearchBarWidth,
                 std::max(0.0F, static_cast<float>(width) - margin * 2.0F));
    const float suggestionWidth = std::max(
        barWidth, std::min(static_cast<float>(width) - margin * 2.0F, 520.0F));

    const float suggestionTop = suggestionListTop();
    const std::size_t count =
        std::min(m_suggestions.size(),
                 static_cast<std::size_t>(ui::theme::kMaxSuggestions));

    if (x < margin || x > margin + suggestionWidth || y < suggestionTop) {
        return -1;
    }

    const int row = static_cast<int>(
        (y - suggestionTop) / ui::theme::kSuggestionLineHeight);
    if (row < 0 || row >= static_cast<int>(count)) {
        return -1;
    }
    if (m_suggestions[static_cast<std::size_t>(row)].kind == ui::SuggestionKind::ReadOnlyText) return -1;
    return row;
}

bool Application::commandRegionContains(double x, double y) const {
    if (!m_searchBar.isOpen()) {
        return false;
    }

    int width = 0;
    int height = 0;
    uiSize(width, height);

    const float margin = ui::theme::kSearchBarMargin;
    const float barWidth =
        std::min(ui::theme::kSearchBarWidth,
                 std::max(0.0F, static_cast<float>(width) - margin * 2.0F));
    const float suggestionWidth = std::max(
        barWidth, std::min(static_cast<float>(width) - margin * 2.0F, 520.0F));
    const float suggestionTop = suggestionListTop();
    const std::size_t count =
        std::min(m_suggestions.size(),
                 static_cast<std::size_t>(ui::theme::kMaxSuggestions));
    const float bottom =
        suggestionTop + static_cast<float>(count) * ui::theme::kSuggestionLineHeight;
    const float barTop = ui::commandBarTop(static_cast<float>(height));

    return x >= margin && x <= margin + suggestionWidth && y >= barTop &&
           y <= bottom;
}

bool Application::welcomeActionContains(double x, double y) const {
    if (m_hasProject) {
        return false;
    }

    int width [[maybe_unused]] = 0;
    int height = 0;
    uiSize(width, height);
    const ui::WelcomeLayout layout = ui::welcomeLayout(
        static_cast<float>(width), static_cast<float>(height), 1.0F);

    return x >= layout.actionX0 && x <= layout.actionX1 &&
           y >= layout.actionY0 && y <= layout.actionY1;
}

bool Application::welcomeExpandContains(double x, double y) const {
    if (m_hasDocument || m_searchBar.visible()) return false;
    int width = 0, height = 0; uiSize(width, height);
    const auto layout = ui::welcomeLayout(static_cast<float>(width), static_cast<float>(height), 1.0F);
    float top = layout.hintTop;
    if (m_hasProject) {
        const float line = ui::theme::kEditorLineHeight;
        const float blockHeight = line * 3.0F + 3.0F * 24.0F;
        const float centerY = std::max(line * 2.0F, (static_cast<float>(height) - blockHeight) * 0.5F + line * 2.0F);
        top = centerY + line * 1.15F;
    }
    const float divider = top + 48.0F;
    return x >= 0 && x < width && y >= divider && y < divider + 24.0F;
}

void Application::openFilePrompt() {
    if (m_searchBar.visible() && m_searchBar.isOpen() &&
        m_searchBar.kind() == ui::PromptKind::File) {
        m_searchBar.close();
        return;
    }

    focusCompileTerminal(false);
    m_searchBar.open(ui::PromptKind::File);
    clearCommandFeedback();
    if (!m_hasProject) {
        setCommandStatus("Open a project first", true);
    } else if (m_projectIndexFuture.valid()) {
        setCommandStatus("Indexing project files…", false);
    }
    refreshSuggestions();
}

void Application::captureTaskContext(workspace::Task& task) const {
    task.files.clear();
    if (m_hasDocument) task.files.push_back({m_filePath, m_document.cursorLine(), m_document.cursorColumn(), m_firstLine});
    for (const auto& file : m_inactiveDocuments)
        task.files.push_back({file.path, file.document.cursorLine(), file.document.cursorColumn(), file.firstLine});
}

bool Application::commitTaskBoard(workspace::TaskBoard board) {
    std::string error;
    if (!workspace::saveTasks(m_project, board, error)) {
        setCommandStatus(error, true);
        setStatus(error, true);
        return false;
    }
    m_taskBoard = std::move(board);
    return true;
}

bool Application::saveActiveTaskContext() {
    if (m_activeTaskId.empty()) return true;
    if (!saveDocument()) return false;
    auto board = m_taskBoard;
    auto* task = workspace::findTask(board, m_activeTaskId);
    if (!task) { setStatus("Active task no longer exists", true); return false; }
    captureTaskContext(*task);
    return commitTaskBoard(std::move(board));
}

bool Application::activateTask(std::string_view id) {
    if (m_debugAdapter.alive()) { setCommandStatus("Stop the debugger before switching tasks", true); return false; }
    if (!saveDocument()) return false;
    auto board = m_taskBoard;
    const std::string targetId(id);
    if (!m_activeTaskId.empty()) {
        auto* previous = workspace::findTask(board, m_activeTaskId);
        if (!previous) { setCommandStatus("Active task no longer exists", true); return false; }
        captureTaskContext(*previous);
        if (m_activeTaskId != targetId) (void)workspace::setTaskWorking(*previous, false, m_settings.taskIdentity);
    }
    auto* target = workspace::findTask(board, targetId);
    if (!target) { setCommandStatus("Task no longer exists", true); return false; }
    (void)workspace::setTaskWorking(*target, true, m_settings.taskIdentity);
    const auto files = target->files;
    const std::string title = target->title;
    if (!commitTaskBoard(std::move(board))) return false;
    m_activeTaskId = targetId;
    m_debugBreakpoints.clear();
    if (const auto* active = workspace::findTask(m_taskBoard, targetId); active && !active->debugSource.empty())
        m_debugBreakpoints[active->debugSource] = active->debugBreakpoints;
    m_inactiveDocuments.clear();
    m_hasDocument = false;
    m_filePath.clear();
    m_document = editor::Document{};
    m_firstLine = 0;
    closeCompileTerminal();
    resetDiagnostics();
    m_syntaxValid = false;
    m_compileScriptValid = false;
    std::size_t missing = 0;
    for (auto it = files.rbegin(); it != files.rend(); ++it) {
        if (!m_project.exists(it->path) || m_project.isDirectory(it->path)) { ++missing; continue; }
        openFile(it->path, false);
        if (!m_hasDocument || m_filePath != it->path) { ++missing; continue; }
        m_document.setCursor(it->line, it->column);
        m_firstLine = it->firstLine;
        m_followCursor = false;
    }
    m_searchBar.close();
    setStatus("Task: " + title + (missing ? " (some files could not be opened)" : ""), missing != 0);
    return true;
}

bool Application::checkpointRecovery() {
    if (!m_hasProject || !m_hasDocument || m_settingsRoot.empty()) return false;
    if (m_recoveryFile.empty()) m_recoveryFile = std::filesystem::path(m_settingsRoot).parent_path() / "Recovery" / (workspace::newTaskId() + ".json");
    workspace::recovery::Snapshot snapshot;
    snapshot.project = m_project.root().generic_string(); snapshot.file = m_filePath;
    snapshot.baseline = m_diskContents;
    std::string serializationError;
    if (!m_document.serializeForFile(m_diskContents, snapshot.contents, serializationError)) {
        setStatus("Recovery could not be written: " + serializationError, true); return false;
    }
    snapshot.line = m_document.cursorLine(); snapshot.column = m_document.cursorColumn(); snapshot.firstLine = m_firstLine;
    for (const auto& file : m_inactiveDocuments)
        snapshot.openFiles.push_back({file.path, file.document.cursorLine(), file.document.cursorColumn(), file.firstLine});
    std::string error;
    // Confirm the on-disk snapshot before skipping a write: it may have been
    // removed or edited outside Lucid since the previous checkpoint.
    if (m_cachedRecoverySnapshot && m_cachedRecoveryPath == m_recoveryFile && *m_cachedRecoverySnapshot == snapshot) {
        workspace::recovery::Snapshot onDisk;
        if (workspace::recovery::read(m_recoveryFile, onDisk, error) && onDisk == snapshot) return true;
    }
    if (!workspace::recovery::write(m_recoveryFile, snapshot, error)) {
        m_cachedRecoverySnapshot.reset();
        setStatus("Recovery: " + error, true);
        return false;
    }
    if (snapshot.contents.size() + snapshot.baseline.size() <= 1024U * 1024U) {
        m_cachedRecoveryPath = m_recoveryFile;
        m_cachedRecoverySnapshot = std::move(snapshot);
    } else m_cachedRecoverySnapshot.reset();
    return true;
}

bool Application::saveSession(std::string& error) {
    if (std::getenv("LUCID_SESSION_DISABLED")) { error.clear(); return true; }
    if (m_pendingSession) { error.clear(); return true; }
    const auto settings = settingsDirectory();
    if (settings.empty()) { error = "Session storage unavailable"; return false; }
    workspace::session::State state; state.recentProjects = m_recentProjects;
    if (m_hasProject) {
        state.project = m_project.root().generic_string();
        if (m_hasDocument) state.files.push_back({m_filePath, m_document.cursorLine(), m_document.cursorColumn(), m_firstLine});
        for (const auto& file : m_inactiveDocuments) {
            if (state.files.size() >= workspace::session::kMaxSessionFiles) {
                error = "Too many open files to save the complete session";
                return false;
            }
            state.files.push_back({file.path, file.document.cursorLine(), file.document.cursorColumn(), file.firstLine});
        }
        if (!workspace::session::mergeUnrestored(state, m_unrestoredProject, m_unrestoredFiles)) {
            error = "Too many open files to preserve the previous session";
            return false;
        }
    }
    return workspace::session::write(settings.parent_path() / "Session", state, error);
}

void Application::restoreSession(const workspace::session::State& state) {
    std::error_code ec;
    const std::filesystem::path root(state.project);
    const auto offerRetry = [this, &state](const char* message) {
        m_pendingSession = state;
        m_searchBar.open(ui::PromptKind::SessionRestore);
        clearCommandFeedback();
        setCommandStatus(message, true);
        refreshSuggestions();
    };
    if (state.project.empty() || !root.is_absolute() || !std::filesystem::is_directory(root, ec) || ec) {
        offerRetry("Previous project is unavailable; reconnect it or start fresh");
        return;
    }
    m_restoringSession = true;
    const bool opened = openProject(root.string());
    m_restoringSession = false;
    if (!opened) {
        offerRetry("Could not reopen the project; try again or start fresh");
        return;
    }
    m_unrestoredProject = m_project.root().generic_string();
    m_unrestoredFiles.clear();
    if (m_searchBar.kind() == ui::PromptKind::RecoveryList && m_searchBar.isOpen()) {
        // Recovery takes precedence in the UI. Keep the remaining session
        // contexts until they are opened or can be restored on a later launch.
        m_unrestoredFiles = state.files;
        setCommandStatus("Review recovery; your previous open files are preserved", false);
        return;
    }
    std::size_t skipped = 0;
    for (auto it = state.files.rbegin(); it != state.files.rend(); ++it) {
        std::string data, error;
        if (!m_project.exists(it->path) || !m_project.readFile(it->path, data, error) ||
            std::any_of(data.begin(), data.end(), [](unsigned char c) { return c == 0 || (c < 0x20 && c != '\n' && c != '\r' && c != '\t' && c != '\f'); }) ||
            !editor::Document::canEditLosslessly(data, error)) { m_unrestoredFiles.push_back(*it); ++skipped; continue; }
        openFile(it->path, false);
        if (!m_hasDocument || m_filePath != it->path) { m_unrestoredFiles.push_back(*it); ++skipped; continue; }
        m_document.setCursor(it->line, it->column); m_firstLine = std::max(0, it->firstLine); m_followCursor = false;
    }
    setStatus(skipped ? "Session restored; " + std::to_string(skipped) + " files unavailable" : "Session restored", skipped != 0);
}
void Application::openRecovery(bool includeHistory) {
    if (!m_hasProject || m_settingsRoot.empty() || m_settingsPanelVisible || m_compileTerminalFocused) { setStatus("Open a project to review recovery snapshots", true); return; }
    const bool currentDirty = m_hasDocument && m_document.dirty();
    const bool currentSaved = !currentDirty || checkpointRecovery();
    const std::string recoveryError = currentSaved ? std::string{} : m_statusText;
    const auto root = std::filesystem::path(m_settingsRoot).parent_path() / "Recovery";
    m_recoveryFiles.clear(); std::error_code ec;
    if (std::filesystem::exists(root, ec)) {
        std::filesystem::directory_iterator it(root, ec), end;
        for (; !ec && it != end; it.increment(ec)) {
            if ((it->path() == m_recoveryFile && (!currentDirty || !currentSaved)) ||
                it->path().extension() != ".json" || it->is_symlink(ec)) continue;
            workspace::recovery::Snapshot snapshot; std::string error;
            if (workspace::recovery::read(it->path(), snapshot, error) && snapshot.project == m_project.root().generic_string()) m_recoveryFiles.push_back(it->path());
        }
    }
    if (ec) { setStatus("Could not list recovery snapshots", true); return; }
    if (includeHistory) {
        std::string error;
        if (!workspace::recovery::listHistory(root, m_project.root().generic_string(), m_recoveryFiles, error)) { setStatus(error, true); return; }
    }
    std::sort(m_recoveryFiles.begin(), m_recoveryFiles.end());
    m_recoverySummaries.clear();
    std::vector<std::filesystem::path> readable;
    for (const auto& path : m_recoveryFiles) {
        workspace::recovery::Snapshot snapshot; std::string error;
        if (!workspace::recovery::read(path, snapshot, error) || snapshot.project != m_project.root().generic_string()) continue;
        readable.push_back(path);
        m_recoverySummaries.push_back({snapshot.file, snapshot.contents == snapshot.baseline,
            path.parent_path().parent_path().filename() == "History", snapshot.openFiles.size() + 1});
    }
    m_recoveryFiles = std::move(readable); m_recoveryPage = 0;
    m_searchBar.open(ui::PromptKind::RecoveryList); clearCommandFeedback();
    if (!currentSaved) setCommandStatus("Current edits could not be backed up; they remain open. " + recoveryError, true);
    else if (m_recoveryFiles.empty()) setCommandStatus("No recovery snapshots for this project", false);
    refreshSuggestions();
}
void Application::refreshRecoverySuggestions() {
    struct Entry { std::string label, detail; int choice; }; std::vector<Entry> entries;
    const auto add = [&entries](std::string label, std::string detail, int choice) { entries.push_back({std::move(label), std::move(detail), choice}); };
    if (m_searchBar.kind() == ui::PromptKind::RecoveryList) {
        for (std::size_t i = 0; i < m_recoveryFiles.size(); ++i) {
            if (i < m_recoverySummaries.size()) {
                const auto& summary = m_recoverySummaries[i];
                const bool history = summary.history;
                std::string detail = history ? "Previous saved version" : (summary.saved ? "Saved content" : "Unsaved changes");
                std::error_code timeError;
                const auto written = std::filesystem::last_write_time(m_recoveryFiles[i], timeError);
                if (!timeError) {
                    const auto seconds = std::max<long long>(0, std::chrono::duration_cast<std::chrono::seconds>(std::filesystem::file_time_type::clock::now() - written).count());
                    detail += seconds < 60 ? "; just now" : seconds < 3600 ? "; " + std::to_string(seconds / 60) + " min ago" : seconds < 86400 ? "; " + std::to_string(seconds / 3600) + " hr ago" : "; " + std::to_string(seconds / 86400) + " days ago";
                }
                if (!history) detail += "; " + std::to_string(summary.openCount) + " open files";
                add(summary.file, std::move(detail), static_cast<int>(i));
            }
        }
    } else {
        add("Restore this snapshot", "Opens a copy; does not overwrite disk", 0);
        add("Restore as a new file", "Keeps the current file unchanged", 3);
        const bool liveSnapshot = m_hasDocument && m_document.dirty() && m_recoverySelected < m_recoveryFiles.size() &&
                                  m_recoveryFiles[m_recoverySelected] == m_recoveryFile;
        add("Delete this snapshot", liveSnapshot ? "Unavailable while it protects unsaved edits" : "", 1);
        add("File: " + m_recoveryPreview.file, "", 2);
        add("Cursor: " + std::to_string(m_recoveryPreview.line + 1) + ":" + std::to_string(m_recoveryPreview.column + 1), "Scroll line " + std::to_string(m_recoveryPreview.firstLine + 1), 2);
        add("Text: " + std::to_string(m_recoveryPreview.contents.size()) + " bytes", !m_recoveryPreview.contents.empty() && m_recoveryPreview.contents.back() == '\n' ? "Ends in newline" : "No trailing newline", 2);
        add("Open files to restore: " + std::to_string(m_recoveryPreview.openFiles.size() + 1), "", 2);
        for (const auto& file : m_recoveryPreview.openFiles) add("Open: " + file.path, "Cursor " + std::to_string(file.line + 1) + ":" + std::to_string(file.column + 1), 2);
        std::istringstream lines(m_recoveryPreview.contents); std::string line; std::size_t count = 0;
        while (std::getline(lines, line) && count++ < 10000) add(std::to_string(count) + ": " + line, "Snapshot content", 2);
        if (count >= 10000) add("Preview limited to 10000 lines", "", 2);
    }
    // Reserve two rows for previous/next controls at larger text sizes.
    const auto pageSize = pagedContentCount();
    const auto pages = std::max<std::size_t>(1, (entries.size() + pageSize - 1) / pageSize);
    m_recoveryPage = std::min(m_recoveryPage, pages - 1); m_recoveryChoices.clear();
    const auto append = [this](const Entry& e) {
        m_suggestionPaths.push_back(e.label); m_suggestionDetails.push_back(e.detail);
        m_suggestions.push_back({m_suggestionPaths.back(), ui::SuggestionKind::Task, m_suggestionDetails.back()}); m_recoveryChoices.push_back(e.choice);
    };
    for (std::size_t i = m_recoveryPage * pageSize; i < std::min(entries.size(), (m_recoveryPage + 1) * pageSize); ++i) append(entries[i]);
    if (m_recoveryPage) append({"< Previous page", "", -1});
    if (m_recoveryPage + 1 < pages) append({"Next page >", "", -2});
    if (m_selectedSuggestion >= static_cast<int>(m_recoveryChoices.size())) m_selectedSuggestion = -1;
}
void Application::applyRecovery() {
    refreshSuggestions(); if (m_recoveryChoices.empty()) return;
    const auto index = m_selectedSuggestion >= 0 && m_selectedSuggestion < static_cast<int>(m_recoveryChoices.size()) ? static_cast<std::size_t>(m_selectedSuggestion) : 0U;
    const int choice = m_recoveryChoices[index];
    if (choice < 0) { if (choice == -1 && m_recoveryPage) --m_recoveryPage; else if (choice == -2) ++m_recoveryPage; m_pagedScroll = 0; m_selectedSuggestion = -1; m_menuReselectionRequired = true; refreshSuggestions(); return; }
    if (m_searchBar.kind() == ui::PromptKind::RecoveryList) {
        m_recoverySelected = static_cast<std::size_t>(choice); std::string error;
        if (m_recoverySelected >= m_recoveryFiles.size() || !workspace::recovery::read(m_recoveryFiles[m_recoverySelected], m_recoveryPreview, error)) { setCommandStatus(error, true); return; }
        m_recoveryPage = 0; m_searchBar.open(ui::PromptKind::RecoveryDetail); clearCommandFeedback(); refreshSuggestions(); return;
    }
    if (choice == 2 || m_recoverySelected >= m_recoveryFiles.size()) return;
    if (choice == 1) {
        if (m_hasDocument && m_document.dirty() && m_recoveryFiles[m_recoverySelected] == m_recoveryFile) {
            setCommandStatus("This snapshot protects your current unsaved edits and cannot be deleted yet", true);
            return;
        }
        if (!platform::confirmDestructiveAction("Delete recovery snapshot", "Permanently delete this recovery snapshot?", "Delete")) return;
        std::error_code ec; std::filesystem::remove(m_recoveryFiles[m_recoverySelected], ec);
        if (ec) { setCommandStatus("Could not delete snapshot: " + ec.message(), true); return; }
        openRecovery(); return;
    }
    workspace::recovery::Snapshot snapshot; std::string error;
    if (!workspace::recovery::read(m_recoveryFiles[m_recoverySelected], snapshot, error) || snapshot != m_recoveryPreview || snapshot.project != m_project.root().generic_string()) { setCommandStatus("Snapshot changed or is unavailable; reopen recovery", true); return; }
    std::string resolveError;
    if (m_project.resolve(snapshot.file, resolveError).empty() || m_project.isDirectory(snapshot.file)) { setCommandStatus("Invalid recovery file path", true); return; }
    if (choice == 3) {
        const std::filesystem::path original(snapshot.file);
        const auto name = original.stem().string() + ".recovered-" + workspace::newTaskId() + original.extension().string();
        const auto copy = (original.parent_path() / name).generic_string();
        if (!m_project.writeFile(copy, snapshot.contents, error, nullptr, true)) {
            setCommandStatus("Could not write recovery copy: " + error + "; the snapshot is unchanged.", true);
            return;
        }
        scheduleProjectIndex();
        if (snapshot.contents.size() > workspace::Project::maxFileSize) {
            m_searchBar.close();
            setStatus("Recovery copy saved as " + copy + "; it exceeds Lucid's file-opening limit", true);
        } else if (m_hasDocument && m_document.dirty()) {
            m_searchBar.close();
            setStatus("Recovery copy saved as " + copy + "; current edits remain open", false);
        } else {
            openFile(copy);
            if (m_hasDocument && m_filePath == copy) setStatus("Recovered as " + copy, false);
        }
        return;
    }
    const bool binaryRecovery = std::any_of(snapshot.contents.begin(), snapshot.contents.end(), [](unsigned char c) {
        return c == 0 || (c < 0x20 && c != '\n' && c != '\r' && c != '\t' && c != '\f');
    });
    if (binaryRecovery || !editor::Document::canEditLosslessly(snapshot.contents, error) ||
        !editor::Document::canEditLosslessly(snapshot.baseline, error)) {
        setCommandStatus("Recovery text cannot be opened safely; save a copy to preserve it", true);
        return;
    }
    if (!saveDocument()) return;
    for (auto it = snapshot.openFiles.rbegin(); it != snapshot.openFiles.rend(); ++it) {
        if (!m_project.exists(it->path) || m_project.isDirectory(it->path)) continue;
        openFile(it->path, false);
        if (m_hasDocument && m_filePath == it->path) { m_document.setCursor(it->line, it->column); m_firstLine = it->firstLine; }
    }
    const bool fileExists = m_project.exists(snapshot.file);
    std::string currentDisk;
    if (fileExists && !m_project.readFile(snapshot.file, currentDisk, error)) { setCommandStatus(error, true); return; }
    if (fileExists) {
        openFile(snapshot.file, false);
        if (!m_hasDocument || m_filePath != snapshot.file) return;
    } else {
        if (m_hasDocument) m_inactiveDocuments.insert(m_inactiveDocuments.begin(), OpenDocument{m_filePath, std::move(m_document), m_firstLine, m_diskContents});
        m_hasDocument = true; m_filePath = snapshot.file; m_diskContents.clear();
    }
    const bool sameAsDisk = fileExists && currentDisk == snapshot.contents;
    m_document = editor::Document::fromUtf8(snapshot.contents);
    m_accessibilityKey.clear();
    if (!sameAsDisk) m_document.markRecovered();
    m_diskContents = sameAsDisk ? snapshot.contents : snapshot.baseline;
    m_document.setCursor(snapshot.line, snapshot.column); m_firstLine = snapshot.firstLine; m_followCursor = false;
    m_searchBar.close(); m_syntaxValid = false; m_compileScriptValid = false; resetDiagnostics();
    setStatus(fileExists ? "Recovery opened; review changes before saving" : "Recovery opened; original file is missing, copy the recovered text", !fileExists);
}

void Application::openModules() {
    if (m_settingsPanelVisible || m_compileTerminalFocused) return;
    if (m_settingsRoot.empty()) { setStatus("Module storage unavailable", true); return; }
    m_moduleRoot = std::filesystem::path(m_settingsRoot).parent_path() / "Modules";
    std::string error;
    if (!modules::list(m_moduleRoot, m_modules, error)) { setStatus(error, true); return; }
    m_modulePage = 0; m_searchBar.open(ui::PromptKind::ModuleList); clearCommandFeedback();
    if (m_modules.empty()) setCommandStatus("No modules installed; add a module to get started", false);
    refreshSuggestions();
}
void Application::refreshModuleSuggestions() {
    struct Entry { std::string label, detail; int choice; };
    std::vector<Entry> entries;
    auto add = [&entries](std::string name, std::string detail, int choice) { entries.push_back({std::move(name), std::move(detail), choice}); };
    const auto kind = m_searchBar.kind();
    if (kind == ui::PromptKind::ModuleList) {
        std::string query(m_searchBar.query());
        std::transform(query.begin(), query.end(), query.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        for (std::size_t i = 0; i < m_modules.size(); ++i) {
            const auto& m = m_modules[i];
            std::string searchable = m.name + " " + m.id + " " + m.description;
            std::transform(searchable.begin(), searchable.end(), searchable.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (searchable.find(query) == std::string::npos) continue;
            add(m.name, !m.error.empty() ? m.error : (m.enabled ? "Enabled" : "Disabled"), static_cast<int>(i));
        }
        add("Add module", "Preview a local manifest or GitHub package", -3);
        add("Browse GitHub modules", "Download the Lucid module catalog", -4);
    } else if (kind == ui::PromptKind::ModuleCatalog) {
        std::string query(m_searchBar.query());
        std::transform(query.begin(), query.end(), query.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        for (std::size_t i = 0; i < m_moduleCatalog.size(); ++i) {
            const auto& entry = m_moduleCatalog[i];
            std::string searchable = entry.name + " " + entry.id + " " + entry.description + " " + entry.package;
            std::transform(searchable.begin(), searchable.end(), searchable.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (searchable.find(query) == std::string::npos) continue;
            const bool installed = std::any_of(m_modules.begin(), m_modules.end(), [&](const auto& m) { return m.id == entry.id; });
            add(entry.name.empty() ? entry.id : entry.name,
                (entry.description.empty() ? entry.package : entry.description) + "  " + entry.version + (installed ? "  Installed" : ""), static_cast<int>(i));
        }
        if (!query.empty() && entries.empty() && !m_moduleFetchFuture.valid() && !m_commandStatusIsError) add("No matching modules", "Try a different search", -6);
        if (query.empty()) add("Refresh catalog", "Check GitHub for modules", -5);
    } else if (kind == ui::PromptKind::ModulePreview) {
        const auto& m = m_modulePreview;
        add(m.name + "  " + m.version, m.description.empty() ? m.id : m.description, 101);
        add("Source", m_modulePreviewSource, 101);
        add("Project files", m.project ? "Requested - unavailable" : "No access", 101);
        add("Network", m.network ? "Requested - unavailable" : "No access", 101);
        add("Processes", m.processes ? "Requested - unavailable" : "No access", 101);
        add("Settings", m.settings ? "Requested - unavailable" : "No access", 101);
        for (const auto& command : m.commands) add(command.title, command.insert, 101);
        add("Install module", m.error.empty() ? "Installs disabled; enable it when ready" : m.error, 100);
    } else if (m_moduleSelected < m_modules.size()) {
        const auto& m = m_modules[m_moduleSelected];
        if (kind == ui::PromptKind::ModuleActions) {
            add(m.enabled ? "Disable module" : "Enable module", m.error, 0);
            if (m.enabled) add("Run a text command", "", 1);
            add("Remove module", "Deletes its module folder", 2);
            add("Project files", m.project ? "Requested - unavailable" : "No access", 3);
            add("Network", m.network ? "Requested - unavailable" : "No access", 3);
            add("Processes", m.processes ? "Requested - unavailable" : "No access", 3);
            add("Settings", m.settings ? "Requested - unavailable" : "No access", 3);
        } else if (m.enabled) {
            for (std::size_t i = 0; i < m.commands.size(); ++i) add(m.commands[i].title, "Insert text", static_cast<int>(i));
        }
    }
    const auto pageSize = pagedContentCount();
    const auto pages = std::max<std::size_t>(1, (entries.size() + pageSize - 1) / pageSize);
    m_modulePage = std::min(m_modulePage, pages - 1); m_moduleChoices.clear();
    auto append = [this](const Entry& e) {
        m_suggestionPaths.push_back(e.label); m_suggestionDetails.push_back(e.detail);
        m_suggestions.push_back({m_suggestionPaths.back(), ui::SuggestionKind::Task, m_suggestionDetails.back()});
        m_moduleChoices.push_back(e.choice);
    };
    const auto start = m_modulePage * pageSize;
    for (std::size_t i = start; i < std::min(entries.size(), start + pageSize); ++i) append(entries[i]);
    if (m_modulePage) append({"< Previous page", "", -1});
    if (m_modulePage + 1 < pages) append({"Next page >", "", -2});
    if (m_selectedSuggestion >= static_cast<int>(m_moduleChoices.size())) m_selectedSuggestion = -1;
}
void Application::applyModuleCommand() {
    if (m_searchBar.kind() == ui::PromptKind::ModuleImport) {
        const std::string source(m_searchBar.query());
        if (source.starts_with("ghcr.io/")) {
            if (m_moduleFetchFuture.valid()) { setCommandStatus("A module download is still running", false); return; }
            const auto reference = source.substr(8);
            const auto separator = reference.rfind(':');
            const auto slash = reference.find('/');
            if (slash == std::string::npos || separator == std::string::npos || separator <= slash + 1 || separator + 1 == reference.size()) {
                setCommandStatus("Use ghcr.io/owner/package:version", true); return;
            }
            const modules::RegistryEntry entry{reference.substr(slash + 1, separator - slash - 1), reference.substr(0, separator), reference.substr(separator + 1), {}, {}};
            const auto generation = ++m_moduleFetchGeneration;
            m_moduleCatalog.clear(); m_modulePage = 0;
            m_searchBar.open(ui::PromptKind::ModuleCatalog); clearCommandFeedback(); refreshSuggestions();
            m_moduleFetchFuture = std::async(std::launch::async, [entry, generation] {
                ModuleFetchResult result; result.id = entry.id; result.generation = generation;
                result.source = "ghcr.io/" + entry.package + ":" + entry.version;
                try { (void)modules::fetchPackage(entry.package, entry.version, "application/vnd.lucid.module.v1+json", result.manifest, result.error); }
                catch (const std::exception& e) { result.error = std::string("Module download failed: ") + e.what(); }
                return result;
            });
            setCommandStatus("Loading module preview…", false); return;
        }
        const std::filesystem::path path{source};
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec || size > 65536 || !std::filesystem::is_regular_file(path, ec)) {
            setCommandStatus("Choose a readable module manifest up to 64 KB", true); return;
        }
        std::ifstream input(path, std::ios::binary);
        if (!input) { setCommandStatus("Could not read module manifest", true); return; }
        std::string manifest(65537, '\0');
        input.read(manifest.data(), static_cast<std::streamsize>(manifest.size()));
        const auto bytes = input.gcount();
        if (input.bad() || bytes > 65536) { setCommandStatus("Could not read module manifest or it exceeds 64 KB", true); return; }
        manifest.resize(static_cast<std::size_t>(bytes));
        const auto id = (path.filename() == "module.json" ? path.parent_path().filename() : path.stem()).string();
        auto candidate = modules::preview(id, manifest);
        if (!candidate.valid) { setCommandStatus(candidate.error, true); return; }
        m_modulePreview = std::move(candidate);
        m_modulePreviewManifest = std::move(manifest);
        m_modulePreviewSource = path.string();
        m_modulePage = 0; m_searchBar.open(ui::PromptKind::ModulePreview); clearCommandFeedback(); refreshSuggestions();
        if (!m_modulePreview.error.empty()) setCommandStatus(m_modulePreview.error, true);
        return;
    }
    refreshSuggestions(); if (m_moduleChoices.empty()) return;
    const auto index = m_selectedSuggestion >= 0 && m_selectedSuggestion < static_cast<int>(m_moduleChoices.size()) ? static_cast<std::size_t>(m_selectedSuggestion) : 0U;
    const int choice = m_moduleChoices[index];
    if (m_searchBar.kind() == ui::PromptKind::ModuleList && choice == -3) {
        m_searchBar.open(ui::PromptKind::ModuleImport); clearCommandFeedback();
        setCommandStatus("Enter a module.json path or ghcr.io/owner/package:version", false); return;
    }
    if ((m_searchBar.kind() == ui::PromptKind::ModuleList && choice == -4) ||
        (m_searchBar.kind() == ui::PromptKind::ModuleCatalog && choice == -5)) {
        refreshModuleCatalog(); return;
    }
    if (choice < 0) { if (choice == -1 && m_modulePage) --m_modulePage; else if (choice == -2) ++m_modulePage; m_pagedScroll = 0; m_selectedSuggestion = -1; m_menuReselectionRequired = true; refreshSuggestions(); return; }
    if (m_searchBar.kind() == ui::PromptKind::ModulePreview) {
        if (choice != 100) return;
        std::string error;
        if (!modules::install(m_moduleRoot, m_modulePreview.id, m_modulePreviewManifest, error)) { setCommandStatus(error, true); return; }
        if (!modules::list(m_moduleRoot, m_modules, error)) {
            setCommandStatus("Module installed, but the list could not be refreshed: " + error, true); return;
        }
        m_modulePreviewManifest.clear(); m_modulePage = 0;
        m_searchBar.open(ui::PromptKind::ModuleList); clearCommandFeedback(); refreshSuggestions();
        setCommandStatus("Module installed disabled; select it to enable", false); return;
    }
    if (m_searchBar.kind() == ui::PromptKind::ModuleCatalog) {
        if (m_moduleFetchFuture.valid()) { setCommandStatus("A module download is still running", false); return; }
        if (static_cast<std::size_t>(choice) >= m_moduleCatalog.size()) return;
        const auto entry = m_moduleCatalog[static_cast<std::size_t>(choice)];
        const auto generation = ++m_moduleFetchGeneration;
        m_moduleFetchFuture = std::async(std::launch::async, [entry, generation] {
            ModuleFetchResult result; result.id = entry.id; result.generation = generation;
            result.source = "ghcr.io/" + entry.package + ":" + entry.version;
            try { (void)modules::fetchPackage(entry.package, entry.version, "application/vnd.lucid.module.v1+json", result.manifest, result.error); }
            catch (const std::exception& e) { result.error = std::string("Module download failed: ") + e.what(); }
            return result;
        });
        setCommandStatus("Loading module preview…", false); return;
    }
    if (m_searchBar.kind() == ui::PromptKind::ModuleList) {
        m_moduleSelected = static_cast<std::size_t>(choice); m_modulePage = 0;
        m_searchBar.open(ui::PromptKind::ModuleActions); clearCommandFeedback(); refreshSuggestions(); return;
    }
    if (m_moduleSelected >= m_modules.size()) return;
    const auto current = m_modules[m_moduleSelected];
    if (m_searchBar.kind() == ui::PromptKind::ModuleCommands) {
        // Re-read the manifest and enabled state at the time of use.
        std::vector<modules::Module> fresh; std::string error;
        if (!modules::list(m_moduleRoot, fresh, error)) { setCommandStatus(error, true); return; }
        const auto found = std::find_if(fresh.begin(), fresh.end(), [&current](const auto& m) { return m.id == current.id; });
        if (found == fresh.end() || !found->enabled || static_cast<std::size_t>(choice) >= found->commands.size() ||
            found->commands[static_cast<std::size_t>(choice)].title != current.commands[static_cast<std::size_t>(choice)].title ||
            found->commands[static_cast<std::size_t>(choice)].insert != current.commands[static_cast<std::size_t>(choice)].insert) {
            setCommandStatus("Module changed or was disabled; reopen Modules", true); return;
        }
        if (!m_hasDocument) { setCommandStatus("Open a file before inserting text", true); return; }
        m_document.pasteUtf8(found->commands[static_cast<std::size_t>(choice)].insert);
        m_searchBar.close(); m_followCursor = true; setStatus("Inserted text from " + current.name, false); return;
    }
    if (choice == 1) { m_modulePage = 0; m_searchBar.open(ui::PromptKind::ModuleCommands); clearCommandFeedback(); refreshSuggestions(); return; }
    if (choice == 3) return;
    std::string error;
    if (choice == 2 && !platform::confirmDestructiveAction("Remove module", "Remove " + current.name + " and its module folder?", "Remove")) return;
    const bool ok = choice == 2 ? modules::remove(m_moduleRoot, current.id, error) : modules::setEnabled(m_moduleRoot, current.id, !current.enabled, error);
    if (!ok) { setCommandStatus(error, true); return; }
    if (!modules::list(m_moduleRoot, m_modules, error)) {
        setCommandStatus("Module changed, but the list could not be refreshed: " + error, true); return;
    }
    m_modulePage = 0; m_searchBar.open(ui::PromptKind::ModuleList); clearCommandFeedback(); refreshSuggestions();
    setCommandStatus(current.name + (choice == 2 ? " removed" : current.enabled ? " disabled" : " enabled"), false);
}

void Application::refreshModuleCatalog() {
    if (m_moduleFetchFuture.valid()) { setCommandStatus("A module download is still running", false); return; }
    m_modulePage = 0;
    if (m_searchBar.kind() != ui::PromptKind::ModuleCatalog) m_searchBar.open(ui::PromptKind::ModuleCatalog);
    clearCommandFeedback(); refreshSuggestions();
    const auto generation = ++m_moduleFetchGeneration;
    m_moduleFetchFuture = std::async(std::launch::async, [generation] {
        ModuleFetchResult result; result.catalog = true; result.generation = generation;
        std::string catalog;
        try {
            if (modules::fetchPackage("initializeddd/lucid-module-catalog", "latest", "application/vnd.lucid.catalog.v1+json", catalog, result.error))
                (void)modules::parseCatalog(catalog, result.entries, result.error);
        } catch (const std::exception& e) { result.error = std::string("Module catalog failed: ") + e.what(); }
        return result;
    });
    setCommandStatus("Loading GitHub module catalog…", false);
}

void Application::pollModules() {
    if (!m_moduleFetchFuture.valid() || m_moduleFetchFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    ModuleFetchResult result;
    try { result = m_moduleFetchFuture.get(); }
    catch (const std::exception& e) { setStatus(std::string("Module download failed: ") + e.what(), true); return; }
    if (result.generation != m_moduleFetchGeneration || !m_searchBar.isOpen() || m_searchBar.kind() != ui::PromptKind::ModuleCatalog) return;
    if (!result.error.empty()) {
        setCommandStatus(result.catalog && !m_moduleCatalog.empty() ? result.error + "; showing previous catalog" : result.error, true);
        refreshSuggestions(); return;
    }
    if (result.catalog) {
        m_moduleCatalog = std::move(result.entries); m_modulePage = 0; m_selectedSuggestion = -1; clearCommandFeedback(); refreshSuggestions();
        setCommandStatus(m_moduleCatalog.empty() ? "No modules in the catalog" : "Select a module to preview", false);
        return;
    }
    auto candidate = modules::preview(result.id, result.manifest);
    if (!candidate.valid) { setCommandStatus(candidate.error, true); return; }
    m_modulePreview = std::move(candidate); m_modulePreviewManifest = std::move(result.manifest);
    m_modulePreviewSource = std::move(result.source);
    m_modulePage = 0; m_searchBar.open(ui::PromptKind::ModulePreview); clearCommandFeedback(); refreshSuggestions();
    if (!m_modulePreview.error.empty()) setCommandStatus(m_modulePreview.error, true);
}

void Application::openDebugger() {
    if (!m_hasProject || m_settingsPanelVisible || m_compileTerminalFocused) { setStatus("Open a project to use the debugger", true); return; }
    m_debugPage = 0; m_searchBar.open(ui::PromptKind::DebugMenu); clearCommandFeedback();
    const bool hasDebugMessage = m_debugAdapter.alive() || m_debugSession.state() == debug::State::Failed;
    setCommandStatus(hasDebugMessage ? m_debugSession.message() : "Select a source file and set breakpoints",
                     hasDebugMessage && m_debugSession.messageIsError());
    refreshSuggestions();
}
void Application::toggleBreakpoint() {
    if (!m_hasProject || !m_hasDocument || m_settingsPanelVisible || m_compileTerminalFocused) { setStatus("Open a source file to set a breakpoint", true); return; }
    auto lines = m_debugBreakpoints[m_filePath]; const int line = static_cast<int>(m_document.cursorLine()) + 1;
    const auto found = std::find(lines.begin(), lines.end(), line);
    const bool added = found == lines.end(); if (added) lines.push_back(line); else lines.erase(found);
    std::sort(lines.begin(), lines.end());
    if (!m_activeTaskId.empty()) {
        auto board = m_taskBoard; auto* task = workspace::findTask(board, m_activeTaskId);
        if (!task || m_settings.taskIdentity.empty()) { setStatus("Active task is unavailable", true); return; }
        task->debugSource = m_filePath; task->debugBreakpoints = lines;
        workspace::recordTaskActivity(*task, m_settings.taskIdentity, added ? "added breakpoint" : "removed breakpoint", m_filePath + ":" + std::to_string(line));
        if (!commitTaskBoard(std::move(board))) return;
    }
    m_debugBreakpoints[m_filePath] = lines;
    if (m_debugAdapter.alive()) { std::string error; const auto source = m_project.resolve(m_filePath, error); if (!error.empty() || !m_debugSession.setBreakpoints(source.string(), lines, error)) { setStatus(error, true); return; } }
    setStatus(std::string(added ? "Breakpoint set at line " : "Breakpoint removed from line ") + std::to_string(line), false);
}
void Application::stopDebugger() {
    if (!m_debugAdapter.alive()) return;
    m_debugSession.disconnect(); std::string error;
    const auto bytes = m_debugSession.takeOutbound();
    if (!m_debugAdapter.send(bytes, error)) { m_debugAdapter.close(); m_debugSession.transportClosed(error, false); setStatus(error, true); return; }
    m_debugClosing = true;
    m_debugCloseDeadline = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count() + 2.0;
    setStatus("Stopping debug session", false);
}
void Application::pollDebugger() {
    if (!m_debugAdapter.alive()) return;
    const auto updateOpenDebugView = [this](const std::string& message, bool error) {
        if (!m_searchBar.isOpen()) return;
        const auto kind = m_searchBar.kind();
        if (kind == ui::PromptKind::DebugMenu || kind == ui::PromptKind::DebugStack || kind == ui::PromptKind::DebugThreads ||
            kind == ui::PromptKind::DebugVariables || kind == ui::PromptKind::DebugOutput) {
            setCommandStatus(message, error); refreshSuggestions();
        }
    };
    std::string error, output; const auto outgoing = m_debugSession.takeOutbound();
    if ((!outgoing.empty() && !m_debugAdapter.send(outgoing, error)) || !m_debugAdapter.poll(output, error)) {
        m_debugAdapter.close();
        if (!m_debugClosing && m_debugSession.state() != debug::State::Ended) {
            const bool starting = m_debugSession.state() == debug::State::Initializing || m_debugSession.state() == debug::State::Launching;
            const std::string message = starting ? "Debugger exited while starting; check that lldb-dap is installed and the program can launch" : "Debugger: " + error;
            m_debugSession.transportClosed(message, false); setStatus(message, true);
        } else { m_debugSession.transportClosed("Debug session ended", true); setStatus("Debug session ended", false); }
        m_debugClosing = false; updateOpenDebugView(m_debugSession.message(), m_debugSession.state() == debug::State::Failed); return;
    }
    if (!output.empty() && !m_debugSession.receive(output, error)) { m_debugAdapter.close(); m_debugClosing = false; setStatus("Debugger: " + error, true); updateOpenDebugView("Debugger: " + error, true); return; }
    const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (!m_debugClosing && m_debugStartDeadline > 0.0 && now >= m_debugStartDeadline &&
        (m_debugSession.state() == debug::State::Initializing || m_debugSession.state() == debug::State::Launching)) {
        m_debugAdapter.close(); m_debugStartDeadline = 0.0;
        const std::string message = "Debugger did not start within 60 seconds; check the adapter and launch configuration";
        m_debugSession.transportClosed(message, false); setStatus(message, true); updateOpenDebugView(message, true); return;
    }
    if (m_debugClosing && (m_debugSession.state() == debug::State::Ended || now >= m_debugCloseDeadline)) {
        m_debugAdapter.close(); m_debugClosing = false; m_debugSession.transportClosed("Debug session ended", true); setStatus("Debug session ended", false); updateOpenDebugView("Debug session ended", false); return;
    }
    if (m_debugSession.state() != m_debugLastState || m_debugLastWarning != m_debugSession.breakpointWarning() || m_debugLastMessage != m_debugSession.message()) {
        m_debugLastState = m_debugSession.state(); m_debugLastWarning = m_debugSession.breakpointWarning(); m_debugLastMessage = m_debugSession.message();
        setStatus(m_debugSession.breakpointWarning().empty() ? m_debugSession.message() : m_debugSession.breakpointWarning(), m_debugSession.messageIsError() || !m_debugSession.breakpointWarning().empty());
        if (m_debugLastState == debug::State::Failed) m_debugAdapter.close();
    }
    if (m_searchBar.isOpen() && m_searchBar.kind() == ui::PromptKind::DebugThreads && !m_debugSession.threadsPending())
        setCommandStatus(m_debugSession.messageIsError() ? m_debugSession.message() : m_debugSession.threads().empty() ? "No threads reported" : "Select a thread", m_debugSession.messageIsError());
    if (m_searchBar.isOpen() && (m_searchBar.kind() == ui::PromptKind::DebugMenu || m_searchBar.kind() == ui::PromptKind::DebugStack || m_searchBar.kind() == ui::PromptKind::DebugThreads || m_searchBar.kind() == ui::PromptKind::DebugVariables || m_searchBar.kind() == ui::PromptKind::DebugOutput)) refreshSuggestions();
}
void Application::refreshDebugSuggestions() {
    m_debugChoices.clear(); const auto kind = m_searchBar.kind(); if (kind == ui::PromptKind::DebugInput) return;
    struct Entry { std::string label, detail; int choice; }; std::vector<Entry> entries;
    const auto add = [&entries](std::string label, std::string detail, int choice) { entries.push_back({std::move(label), std::move(detail), choice}); };
    if (kind == ui::PromptKind::DebugMenu) {
        if (!m_debugAdapter.alive() || m_debugSession.state() == debug::State::Ended || m_debugSession.state() == debug::State::Failed) add("Start debugging", "Choose an executable", -10);
        if (m_hasDocument) add("Toggle breakpoint at cursor", "", -11);
        if (m_debugAdapter.alive() && m_debugSession.state() == debug::State::Stopped) {
            add("Continue", "", -12); add("Step over", "", -13); add("Step into", "", -14); add("Step out", "", -15);
            add("Call stack", std::to_string(m_debugSession.stack().size()), -16);
            add("Variables", std::to_string(m_debugSession.variables().size()), -17);
            add("Explain paused state with AI", m_settings.aiDebugProvider == "local" ? "Local model" : "OpenRouter sends stack and variables online", -21);
            add("Threads", "Choose a paused thread", -24);
        }
        if (m_debugAdapter.alive() || !m_debugSession.output().empty()) add("Debug output", "", -18);
        if (m_debugAdapter.alive() && m_debugSession.state() != debug::State::Ended && m_debugSession.state() != debug::State::Failed) add("Stop debugger", "Terminates the debuggee", -19);
    } else if (kind == ui::PromptKind::DebugStack) {
        for (std::size_t i = 0; i < m_debugSession.stack().size(); ++i) { const auto& f = m_debugSession.stack()[i]; add(f.name, f.path + ":" + std::to_string(f.line), static_cast<int>(i)); }
    } else if (kind == ui::PromptKind::DebugThreads) {
        for (std::size_t i = 0; i < m_debugSession.threads().size(); ++i) {
            const auto& thread = m_debugSession.threads()[i];
            add(thread.name.empty() ? "Thread " + std::to_string(thread.id) : thread.name,
                thread.id == m_debugSession.selectedThread() ? "Selected" : "Select thread", static_cast<int>(i));
        }
    } else if (kind == ui::PromptKind::DebugVariables) {
        if (m_debugSession.expandedVariables()) add("Back to frame variables", "", -23);
        for (std::size_t i = 0; i < m_debugSession.variables().size(); ++i) {
            const auto& v = m_debugSession.variables()[i];
            add(std::string(v.changed ? "* " : "") + v.name + " = " + v.value, v.reference > 0 ? "Expand " + v.type : v.type, static_cast<int>(i));
        }
    } else if (kind == ui::PromptKind::DebugOutput) {
        std::istringstream input(m_debugSession.output()); std::string line; while (std::getline(input, line)) add(line, "", -20);
    }
    const auto pageSize = pagedContentCount(); const auto pages = std::max<std::size_t>(1, (entries.size() + pageSize - 1) / pageSize);
    m_debugPage = std::min(m_debugPage, pages - 1);
    const auto append = [this](const Entry& e) { m_suggestionPaths.push_back(e.label); m_suggestionDetails.push_back(e.detail); m_suggestions.push_back({m_suggestionPaths.back(), ui::SuggestionKind::Task, m_suggestionDetails.back()}); m_debugChoices.push_back(e.choice); };
    for (std::size_t i = m_debugPage * pageSize; i < std::min(entries.size(), (m_debugPage + 1) * pageSize); ++i) append(entries[i]);
    if (m_debugPage) append({"< Previous page", "", -1});
    if (m_debugPage + 1 < pages) append({"Next page >", "", -2});
    if (m_selectedSuggestion >= static_cast<int>(m_debugChoices.size())) m_selectedSuggestion = -1;
}
void Application::applyDebugCommand() {
    const auto kind = m_searchBar.kind();
    if (kind == ui::PromptKind::DebugInput) {
        if (!m_hasDocument || !m_hasProject) { setCommandStatus("Open the source file first", true); return; }
        if (!m_debugAwaitArguments && !m_debugAwaitWorkingDirectory && !m_debugAwaitEnvironment) {
            const std::string program(m_searchBar.query()); std::string error; std::error_code fileError;
            const auto binary = m_project.resolve(program, error);
            if (!error.empty() || !std::filesystem::is_regular_file(binary, fileError) || fileError) { setCommandStatus("Enter an existing executable path inside the project", true); return; }
            m_debugPendingProgram = program; m_debugPendingRoot = m_project.root().generic_string(); m_debugPendingSource = m_filePath;
            m_debugAwaitArguments = true; m_searchBar.setQuery("[]");
            setCommandStatus("Arguments as a JSON array of strings, e.g. [\"--verbose\"]; Enter continues", false); return;
        }
        if (m_project.root().generic_string() != m_debugPendingRoot || m_filePath != m_debugPendingSource) {
            m_debugAwaitArguments = false; m_debugAwaitWorkingDirectory = false; m_debugAwaitEnvironment = false; setCommandStatus("Project or source changed; start again", true); return;
        }
        if (m_debugAwaitArguments) {
            editor::JsonError parseError; const auto parsed = editor::parseJson(m_searchBar.query(), parseError);
            if (!parsed || parsed->type != editor::JsonType::Array || parsed->children.size() > 128) {
                setCommandStatus("Enter arguments as a JSON array of strings", true); return;
            }
            std::vector<std::string> arguments;
            for (const auto& value : parsed->children) {
                if (value.type != editor::JsonType::String || value.stringValue.size() > 4096 || value.stringValue.find('\0') != std::string::npos) {
                    setCommandStatus("Each argument must be text without NUL, up to 4096 bytes", true); return;
                }
                arguments.push_back(value.stringValue);
            }
            m_debugPendingArguments = std::move(arguments); m_debugAwaitArguments = false; m_debugAwaitWorkingDirectory = true;
            m_searchBar.setQuery("."); setCommandStatus("Working directory relative to project; . uses the project root", false); return;
        }
        if (m_debugAwaitWorkingDirectory) {
            m_debugPendingDirectory = m_searchBar.query();
        }
        const std::string directory = m_debugPendingDirectory; std::string directoryError;
        const auto cwd = directory.empty() || directory == "." ? m_project.root() : m_project.resolve(directory, directoryError);
        std::error_code directoryStatus;
        if (!directoryError.empty() || !std::filesystem::is_directory(cwd, directoryStatus) || directoryStatus) {
            setCommandStatus("Enter an existing working directory inside the project", true); return;
        }
        if (m_debugAwaitWorkingDirectory) {
            m_debugAwaitWorkingDirectory = false; m_debugAwaitEnvironment = true;
            m_searchBar.setQuery("{}"); setCommandStatus("Environment as a JSON object of strings; {} inherits defaults", false); return;
        }
        editor::JsonError environmentError; const auto environmentJson = editor::parseJson(m_searchBar.query(), environmentError);
        if (!environmentJson || environmentJson->type != editor::JsonType::Object || environmentJson->children.size() > 128) {
            setCommandStatus("Enter environment variables as a JSON object of strings", true); return;
        }
        std::map<std::string, std::string> environment;
        for (const auto& entry : environmentJson->children) {
            if (entry.key.empty() || entry.key.size() > 256 || entry.key.find_first_of("=\0", 0, 2) != std::string::npos ||
                entry.type != editor::JsonType::String || entry.stringValue.size() > 4096 || entry.stringValue.find('\0') != std::string::npos ||
                !environment.emplace(entry.key, entry.stringValue).second) {
                setCommandStatus("Environment names must be unique text without = or NUL; values must be text", true); return;
            }
        }
        const std::string program = m_debugPendingProgram; std::string error;
        const auto binary = m_project.resolve(program, error); std::error_code fileError;
        if (!error.empty() || !std::filesystem::is_regular_file(binary, fileError) || fileError) { setCommandStatus("Enter an existing executable path inside the project", true); return; }
        const auto source = m_project.resolve(m_filePath, error);
        if (!error.empty() || !saveDocument()) { setCommandStatus(error.empty() ? "Could not save source" : error, true); return; }
        if (!m_debugAdapter.startDefault(error)) { setCommandStatus(error, true); return; }
        if (!m_activeTaskId.empty()) {
            auto board = m_taskBoard; auto* task = workspace::findTask(board, m_activeTaskId);
            if (!task || m_settings.taskIdentity.empty()) { m_debugAdapter.close(); setCommandStatus("Active task is unavailable", true); return; }
            (void)workspace::setTaskDebugFunction(*task, program, m_settings.taskIdentity);
            task->debugSource = m_filePath; task->debugBreakpoints = m_debugBreakpoints[m_filePath];
            workspace::recordTaskActivity(*task, m_settings.taskIdentity, "started debugging", program);
            if (!commitTaskBoard(std::move(board))) { m_debugAdapter.close(); return; }
        }
        m_debugSource = m_filePath; m_debugClosing = false; m_debugLastState = debug::State::Idle; m_debugLastWarning.clear(); m_debugLastMessage.clear();
        std::map<std::string, std::vector<int>> breakpoints;
        for (const auto& [path, lines] : m_debugBreakpoints) {
            std::string pathError; const auto resolved = m_project.resolve(path, pathError);
            if (pathError.empty()) breakpoints.emplace(resolved.string(), lines);
        }
        m_debugSession.start(binary.string(), cwd.string(), std::move(breakpoints), std::move(m_debugPendingArguments), std::move(environment));
        m_debugAwaitArguments = false; m_debugAwaitWorkingDirectory = false; m_debugAwaitEnvironment = false;
        m_debugStartDeadline = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count() + 60.0;
        m_searchBar.close(); setStatus("Starting debugger", false); return;
    }
    refreshSuggestions(); if (m_debugChoices.empty()) return;
    const auto selected = m_selectedSuggestion >= 0 && m_selectedSuggestion < static_cast<int>(m_debugChoices.size()) ? static_cast<std::size_t>(m_selectedSuggestion) : 0U;
    const int choice = m_debugChoices[selected];
    if (choice == -1 || choice == -2) { if (choice == -1 && m_debugPage) --m_debugPage; else if (choice == -2) ++m_debugPage; m_pagedScroll = 0; m_selectedSuggestion = -1; m_menuReselectionRequired = true; refreshSuggestions(); return; }
    if (kind == ui::PromptKind::DebugStack && choice >= 0 && static_cast<std::size_t>(choice) < m_debugSession.stack().size()) {
        const auto frame = m_debugSession.stack()[static_cast<std::size_t>(choice)];
        std::string selectError;
        if (!m_debugSession.selectFrame(static_cast<std::size_t>(choice), selectError)) { setCommandStatus(selectError, true); return; }
        const auto relative = std::filesystem::path(frame.path).lexically_relative(m_project.root());
        if (relative.empty() || *relative.begin() == ".." || !m_project.exists(relative.generic_string())) { setCommandStatus("Source is outside the project or unavailable", true); return; }
        openFile(relative.generic_string()); if (m_hasDocument && m_filePath == relative.generic_string()) { m_document.setCursor(static_cast<std::size_t>(std::max(0, frame.line - 1)), static_cast<std::size_t>(std::max(0, frame.column - 1))); m_followCursor = true; }
        return;
    }
    if (kind == ui::PromptKind::DebugThreads && choice >= 0) {
        std::string error;
        if (!m_debugSession.selectThread(static_cast<std::size_t>(choice), error)) { setCommandStatus(error, true); return; }
        m_debugPage = 0; m_searchBar.open(ui::PromptKind::DebugStack); clearCommandFeedback();
        setCommandStatus("Loading selected thread's stack…", false); refreshSuggestions(); return;
    }
    if (kind == ui::PromptKind::DebugVariables) {
        std::string error;
        const bool ok = choice == -23 ? m_debugSession.selectFrame(m_debugSession.selectedFrame(), error) :
                        choice >= 0 && m_debugSession.expandVariable(static_cast<std::size_t>(choice), error);
        if (!ok && !error.empty()) setCommandStatus(error, true); else if (ok) { m_debugPage = 0; setCommandStatus("Loading variables...", false); refreshSuggestions(); }
        return;
    }
    if (kind != ui::PromptKind::DebugMenu) return;
    if (choice == -10) {
        if (!m_hasDocument) { setCommandStatus("Open a source file first", true); return; }
        m_debugAwaitArguments = false; m_debugAwaitWorkingDirectory = false; m_debugAwaitEnvironment = false;
        m_searchBar.open(ui::PromptKind::DebugInput); clearCommandFeedback();
        if (auto* task = workspace::findTask(m_taskBoard, m_activeTaskId); task && !task->debugFunction.empty()) m_searchBar.setQuery(task->debugFunction);
        setCommandStatus("Program must already be built with debug symbols", false); return;
    }
    if (choice == -21 || choice == -22) {
        const bool remote = m_settings.aiDebugProvider == "openrouter";
        std::string context = "Reason: " + m_debugSession.message();
        const auto appendBounded = [&context](std::string_view part, std::size_t limit) {
            if (context.size() >= limit) return;
            auto count = std::min(part.size(), limit - context.size());
            while (count > 0 && count < part.size() && (static_cast<unsigned char>(part[count]) & 0xc0U) == 0x80U) --count;
            context.append(part.substr(0, count));
        };
        for (const auto& frame : m_debugSession.stack()) { appendBounded("\n" + frame.name + " " + frame.path + ":" + std::to_string(frame.line), 600); if (context.size() >= 600) break; }
        for (const auto& variable : m_debugSession.variables()) { appendBounded("\n" + variable.name + " = " + variable.value, 1200); if (context.size() >= 1200) break; }
        openAiPrompt();
        if (m_searchBar.isOpen() && m_searchBar.kind() == ui::PromptKind::Ai && !m_aiFuture.valid()) {
            m_aiDebugExplanation = true; m_aiDebugOpenRouter = remote; m_aiDebugContext = std::move(context);
            m_searchBar.setQuery("Explain this paused program state in at most three sentences. Identify uncertainty.");
            setCommandStatus(remote ? "OpenRouter receives stack and variable values; Enter sends, Esc cancels" : "Local model sees stack and variable values; Enter explains, Esc cancels", false);
        }
        return;
    }
    if (choice == -11) { toggleBreakpoint(); m_searchBar.close(); return; }
    if (choice == -24) {
        std::string error; if (!m_debugSession.requestThreads(error)) { setCommandStatus(error, true); return; }
        m_debugPage = 0; m_searchBar.open(ui::PromptKind::DebugThreads); clearCommandFeedback();
        setCommandStatus("Loading threads…", false); refreshSuggestions(); return;
    }
    if (choice == -19) { stopDebugger(); m_searchBar.close(); return; }
    if (choice >= -15 && choice <= -12) {
        const char* command = choice == -12 ? "continue" : choice == -13 ? "next" : choice == -14 ? "stepIn" : "stepOut";
        std::string error; if (!m_debugSession.resume(command, error)) { setCommandStatus(error, true); return; }
        m_searchBar.close(); setStatus("Running", false); return;
    }
    if (choice == -16 || choice == -17 || choice == -18) {
        m_debugPage = 0; m_searchBar.open(choice == -16 ? ui::PromptKind::DebugStack : choice == -17 ? ui::PromptKind::DebugVariables : ui::PromptKind::DebugOutput); clearCommandFeedback(); refreshSuggestions();
    }
}

void Application::openGitReview() {
    if (!m_hasProject || m_settingsPanelVisible || m_compileTerminalFocused) {
        setStatus("Open a project to review Git changes", true); return;
    }
    if (m_hasDocument && !saveDocument()) return;
    std::string error;
    if (!workspace::gitChanges(m_project.root(), m_gitChanges, error)) {
        setStatus("Git review: " + error, true); return;
    }
    m_gitPage = 0; m_searchBar.open(ui::PromptKind::GitChanges); clearCommandFeedback();
    if (m_gitChanges.empty()) setCommandStatus("No changed files", false);
    refreshSuggestions();
}

void Application::pollGitRemote() {
    if (!m_gitRemoteFuture.valid() || m_gitRemoteFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    GitRemoteResult result;
    try { result = m_gitRemoteFuture.get(); } catch (const std::exception& e) { result.error = e.what(); }
    const bool sameProject = m_hasProject && m_project.root().generic_string() == m_gitRemoteRoot;
    std::string message = result.ok ? (m_gitRemoteAction == workspace::GitRemoteAction::Fetch ? "Fetch completed" : m_gitRemoteAction == workspace::GitRemoteAction::Pull ? "Pull completed" : "Push completed") : "Git operation failed: " + result.error;
    bool warning = !result.ok;
    // A failed pull can still update the working tree (for example, a merge
    // that stops with conflicts), so reconcile the editor in both cases.
    if (sameProject && m_gitRemoteAction == workspace::GitRemoteAction::Pull) {
        scheduleProjectIndex();
        if (m_hasDocument) {
            std::string disk, error;
            if (!m_project.readFile(m_filePath, disk, error)) {
                m_document.markRecovered(); message += "; open file is unavailable—its text remains open"; warning = true;
            } else if (disk != m_diskContents) {
                if (m_document.dirty()) { message += "; open file changed on disk while you were editing"; warning = true; }
                else if (!editor::Document::canEditLosslessly(disk, error)) { m_document.markRecovered(); message += "; updated file cannot be reloaded safely"; warning = true; }
                else {
                    const auto line = m_document.cursorLine(), column = m_document.cursorColumn();
                    m_document = editor::Document::fromUtf8(disk); m_document.setCursor(line, column); m_diskContents = disk;
                    m_syntaxValid = false; m_compileScriptValid = false; resetDiagnostics(); m_followCursor = true;
                }
            }
        }
    }
    if (sameProject && m_searchBar.isOpen() && m_searchBar.kind() == ui::PromptKind::GitRemoteReview) {
        m_gitPage = 0; std::string statusError;
        if (!workspace::gitChanges(m_project.root(), m_gitChanges, statusError) && result.ok) { message += "; could not refresh Git status: " + statusError; warning = true; }
        m_searchBar.open(ui::PromptKind::GitChanges); clearCommandFeedback(); refreshSuggestions(); setCommandStatus(message, warning);
    }
    setStatus(message, warning);
}

void Application::refreshGitSuggestions() {
    m_gitChoices.clear();
    const auto kind = m_searchBar.kind();
    if (kind == ui::PromptKind::GitCommitInput) return;
    struct Entry { std::string label, detail; int choice; };
    std::vector<Entry> entries;
    auto add = [&entries](std::string label, std::string detail, int choice) { entries.push_back({std::move(label), std::move(detail), choice}); };
    if (kind == ui::PromptKind::GitChanges) {
        add("Review staged changes and commit", "Local commit", -10);
        add("Fetch from upstream", "Download remote updates", -30);
        add("Pull from upstream", "Clean working tree; fast-forward only", -31);
        add("Push to upstream", "Current commit; no force push", -32);
        add("Recent commits", "View history", -35);
        for (std::size_t i = 0; i < m_gitChanges.size(); ++i) {
            const auto& item = m_gitChanges[i]; add(item.path, item.conflict ? "Merge conflict" : "Git " + item.status, static_cast<int>(i));
        }
    } else if (kind == ui::PromptKind::GitFile && m_gitSelectedFile < m_gitChanges.size()) {
        const auto& item = m_gitChanges[m_gitSelectedFile];
        add("View diff", item.path, -11);
        if (item.conflict || item.status == "??" || (item.status.size() == 2 && item.status[1] != ' ' && item.status[1] != '?'))
            add(item.conflict ? "Mark resolved and stage" : "Stage file", "Stages current file contents", -12);
        if (!item.conflict && item.status.size() == 2 && item.status[0] != ' ' && item.status[0] != '?') add("Unstage file", "Keeps working file", -13);
        if (m_project.exists(item.path) && !m_project.isDirectory(item.path)) add("Open file", "", -14);
    } else if (kind == ui::PromptKind::GitDiff) {
        for (std::size_t i = 0; i < m_gitReview.lines.size(); ++i) {
            const auto& line = m_gitReview.lines[i]; add(line.text, line.line > 0 ? "Line " + std::to_string(line.line) : "", static_cast<int>(i));
        }
    } else if (kind == ui::PromptKind::GitCommitReview) {
        add("Write commit message", "Enter creates a local commit", -15);
        for (const auto& path : m_gitCommitPreview.paths) add(path, "Staged", -16);
    } else if (kind == ui::PromptKind::GitRemoteReview) {
        const char* name = m_gitRemoteAction == workspace::GitRemoteAction::Fetch ? "Fetch" : m_gitRemoteAction == workspace::GitRemoteAction::Pull ? "Pull" : "Push";
        add(std::string("Run ") + name, "Enter starts the operation", -33);
        add("Branch: " + m_gitUpstream.branch, "", -34);
        add("Remote: " + m_gitUpstream.remote, "", -34);
        add("Upstream: " + m_gitUpstream.mergeRef, "", -34);
        add("Commit: " + m_gitUpstream.head.substr(0, 12), "", -34);
    } else if (kind == ui::PromptKind::GitHistory) {
        for (const auto& item : m_gitHistory) add(item.hash + "  " + item.subject, item.author + " · " + item.date, -34);
    }
    const auto pageSize = pagedContentCount();
    const auto pages = std::max<std::size_t>(1, (entries.size() + pageSize - 1) / pageSize);
    m_gitPage = std::min(m_gitPage, pages - 1);
    auto append = [this](const Entry& item) {
        m_suggestionPaths.push_back(item.label); m_suggestionDetails.push_back(item.detail);
        m_suggestions.push_back({m_suggestionPaths.back(), ui::SuggestionKind::Task, m_suggestionDetails.back()}); m_gitChoices.push_back(item.choice);
    };
    for (std::size_t i = m_gitPage * pageSize; i < std::min(entries.size(), (m_gitPage + 1) * pageSize); ++i) append(entries[i]);
    if (m_gitPage) append({"< Previous page", "", -1});
    if (m_gitPage + 1 < pages) append({"Next page >", "", -2});
    if (m_selectedSuggestion >= static_cast<int>(m_gitChoices.size())) m_selectedSuggestion = -1;
}

void Application::applyGitReview() {
    const auto kind = m_searchBar.kind();
    if (kind == ui::PromptKind::GitCommitInput) {
        const std::string message(m_searchBar.query()); std::string output, error;
        if (!workspace::gitCommit(m_project.root(), m_gitCommitPreview, message, output, error)) { setCommandStatus(error, true); return; }
        openGitReview(); setCommandStatus("Commit created locally", false); setStatus("Commit created locally", false); return;
    }
    refreshSuggestions(); if (m_gitChoices.empty()) return;
    const auto selected = m_selectedSuggestion >= 0 && m_selectedSuggestion < static_cast<int>(m_gitChoices.size()) ? m_selectedSuggestion : 0;
    const int choice = m_gitChoices[static_cast<std::size_t>(selected)];
    if (kind == ui::PromptKind::GitRemoteReview && choice == -33) {
        if (m_selectedSuggestion < 0) { setCommandStatus("Select Run to confirm", false); return; }
        if (m_gitRemoteFuture.valid()) { setCommandStatus("A Git operation is already running", true); return; }
        if (m_hasDocument && !saveDocument()) { setCommandStatus("Save the open file before continuing", true); return; }
        const auto root = m_project.root(); const auto upstream = m_gitUpstream; const auto action = m_gitRemoteAction;
        m_gitRemoteRoot = root.generic_string();
        m_gitRemoteFuture = std::async(std::launch::async, [root, upstream, action] {
            GitRemoteResult result;
            try { result.ok = workspace::gitRemote(root, upstream, action, result.output, result.error); }
            catch (const std::exception& e) { result.error = e.what(); }
            return result;
        });
        setCommandStatus("Git operation running; authentication must already be configured", false); return;
    }
    if (kind == ui::PromptKind::GitRemoteReview) return;
    if (choice == -1 || choice == -2) { if (choice == -1 && m_gitPage) --m_gitPage; else if (choice == -2) ++m_gitPage; m_pagedScroll = 0; m_selectedSuggestion = -1; m_menuReselectionRequired = true; refreshSuggestions(); return; }
    if (kind == ui::PromptKind::GitChanges) {
        if (choice == -35) {
            std::string error; if (!workspace::gitHistory(m_project.root(), m_gitHistory, error)) { setCommandStatus(error, true); return; }
            m_gitPage = 0; m_searchBar.open(ui::PromptKind::GitHistory); clearCommandFeedback(); refreshSuggestions(); return;
        }
        if (choice >= -32 && choice <= -30) {
            if (m_gitRemoteFuture.valid()) { setCommandStatus("A Git operation is already running", true); return; }
            std::string error;
            if (!workspace::gitUpstream(m_project.root(), m_gitUpstream, error)) { setCommandStatus(error, true); return; }
            m_gitRemoteAction = choice == -30 ? workspace::GitRemoteAction::Fetch : choice == -31 ? workspace::GitRemoteAction::Pull : workspace::GitRemoteAction::Push;
            m_gitPage = 0; m_searchBar.open(ui::PromptKind::GitRemoteReview); clearCommandFeedback(); refreshSuggestions();
            setCommandStatus("Review the branch and upstream. Credentials must be available without prompts.", false); return;
        }
        if (choice == -10) {
            std::string error;
            if (!workspace::gitCommitPreview(m_project.root(), m_gitCommitPreview, error)) { setCommandStatus(error, true); return; }
            m_gitPage = 0; m_searchBar.open(ui::PromptKind::GitCommitReview); clearCommandFeedback(); refreshSuggestions(); return;
        }
        if (choice < 0 || static_cast<std::size_t>(choice) >= m_gitChanges.size()) return;
        m_gitSelectedFile = static_cast<std::size_t>(choice); m_gitPage = 0;
        m_searchBar.open(ui::PromptKind::GitFile); clearCommandFeedback(); refreshSuggestions(); return;
    }
    if (kind == ui::PromptKind::GitCommitReview) {
        if (choice == -15) { m_searchBar.open(ui::PromptKind::GitCommitInput); clearCommandFeedback(); setCommandStatus("Enter a commit message; Enter commits all reviewed staged files", false); }
        return;
    }
    if (m_gitSelectedFile >= m_gitChanges.size()) return;
    if (kind == ui::PromptKind::GitFile) {
        const auto file = m_gitChanges[m_gitSelectedFile]; std::string error;
        if (choice == -11) {
            if (!workspace::gitReview(m_project.root(), file, m_gitReview, error)) { setCommandStatus(error, true); return; }
            m_gitPage = 0; m_searchBar.open(ui::PromptKind::GitDiff); clearCommandFeedback(); refreshSuggestions(); return;
        }
        if (choice == -14) { openFile(file.path); return; }
        if (choice != -12 && choice != -13) return;
        std::vector<workspace::GitChange> current;
        if (!workspace::gitChanges(m_project.root(), current, error)) { setCommandStatus(error, true); return; }
        const auto found = std::find_if(current.begin(), current.end(), [&file](const auto& item) { return item.path == file.path && item.status == file.status && item.oldPath == file.oldPath; });
        if (found == current.end()) { setCommandStatus("Git status changed; reopen review", true); return; }
        if (!(choice == -12 ? workspace::gitStage(m_project.root(), *found, error) : workspace::gitUnstage(m_project.root(), *found, error))) { setCommandStatus(error, true); return; }
        openGitReview(); setCommandStatus(choice == -12 ? "File staged" : "File unstaged", false); return;
    }
    if (kind != ui::PromptKind::GitDiff || choice < 0 || static_cast<std::size_t>(choice) >= m_gitReview.lines.size()) return;
    const auto& line = m_gitReview.lines[static_cast<std::size_t>(choice)]; if (line.line <= 0) return;
    const std::string path = m_gitChanges[m_gitSelectedFile].path; const int lineNumber = line.line;
    if (!m_project.exists(path)) { setCommandStatus("This file is deleted", true); return; }
    openFile(path); if (!m_hasDocument || m_filePath != path) return;
    m_document.setCursor(static_cast<std::size_t>(lineNumber - 1), 0); m_followCursor = true;
    std::string author, error;
    if (workspace::gitBlame(m_project.root(), path, lineNumber, author, error)) setStatus("Git blame: " + author, false);
    else setStatus("Opened change at line " + std::to_string(lineNumber), false);
}

void Application::openTasks() {
    if (!m_hasProject || m_settingsPanelVisible || m_compileTerminalFocused) {
        setStatus("Open a project to use tasks", true);
        return;
    }
    if (!saveActiveTaskContext()) return;
    workspace::TaskBoard board;
    std::string error;
    if (!workspace::loadTasks(m_project, board, error)) { setStatus(error, true); return; }
    m_taskBoard = std::move(board);
    if (!m_activeTaskId.empty() && !workspace::findTask(m_taskBoard, m_activeTaskId)) m_activeTaskId.clear();
    if (m_settings.taskIdentity.empty()) {
        m_taskInputKind = TaskInputKind::Identity;
        showTaskView(ui::PromptKind::TaskInput);
        setCommandStatus("Set a display name to record your task activity", false);
    } else showTaskView(ui::PromptKind::TaskList);
}

void Application::showTaskView(ui::PromptKind view) {
    m_taskPage = 0;
    m_completionOpen = false;
    m_searchBar.open(view);
    clearCommandFeedback();
    refreshSuggestions();
}

void Application::refreshTaskSuggestions() {
    struct Entry { std::string label; std::string detail; TaskChoice choice; };
    std::vector<Entry> entries;
    const auto add = [&entries](std::string label, std::string detail, TaskChoiceKind kind, std::string id = {}) {
        entries.push_back({std::move(label), std::move(detail), {kind, std::move(id)}});
    };
    const auto view = m_searchBar.kind();
    auto* task = workspace::findTask(m_taskBoard, m_selectedTaskId);
    if (view == ui::PromptKind::TaskInput) return;
    if (view == ui::PromptKind::TaskList) {
        if (m_searchBar.query().empty()) {
            add("+ New task", "", TaskChoiceKind::NewTask);
            add("Your name: " + m_settings.taskIdentity, "Change", TaskChoiceKind::Identity);
            if (!m_activeTaskId.empty()) add("Leave task workspace", "", TaskChoiceKind::LeaveWorkspace);
        }
        std::vector<const workspace::Task*> ordered;
        for (const auto& item : m_taskBoard.tasks) ordered.push_back(&item);
        std::stable_sort(ordered.begin(), ordered.end(), [](const auto* a, const auto* b) {
            if (a->done != b->done) return !a->done;
            return a->tier > b->tier;
        });
        std::string query(m_searchBar.query());
        std::transform(query.begin(), query.end(), query.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        for (const auto* item : ordered) {
            std::string name = item->title;
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (!query.empty() && name.find(query) == std::string::npos) continue;
            const std::string label = "[T" + std::to_string(item->tier) + (item->done ? " done] " : "] ") + item->title;
            add(label, item->id == m_activeTaskId ? "Active" : "By " + item->createdBy, TaskChoiceKind::SelectTask, item->id);
        }
    } else if (view == ui::PromptKind::TaskTier) {
        add("Tier 1 - Optional features and polish", "", TaskChoiceKind::Tier1);
        add("Tier 2 - Quality of life", "", TaskChoiceKind::Tier2);
        add("Tier 3 - Urgent bugs and security", "", TaskChoiceKind::Tier3);
    } else if (task && view == ui::PromptKind::TaskActions) {
        add("Open task workspace", "", TaskChoiceKind::OpenWorkspace);
        add("Add subtask", "", TaskChoiceKind::AddSubtask);
        add("View subtasks", std::to_string(task->subtasks.size()), TaskChoiceKind::ViewSubtasks);
        add("Assign task", "", TaskChoiceKind::Assign);
        const bool workingHere = std::find(task->workingBy.begin(), task->workingBy.end(), m_settings.taskIdentity) != task->workingBy.end();
        add(workingHere ? "Stop working on this" : "I'm working on this", "", TaskChoiceKind::ToggleWorking);
        add(task->done ? "Reopen task" : "Mark task done", "", TaskChoiceKind::ToggleDone);
        add("Record how it was solved", "", TaskChoiceKind::Solution);
        add("View activity", "", TaskChoiceKind::History);
        add("Created by: " + task->createdBy, "", TaskChoiceKind::Label);
        add("Assigned: " + (task->assignedTo.empty() ? std::string("Unassigned") : task->assignedTo), "", TaskChoiceKind::Label);
        std::string working;
        for (const auto& name : task->workingBy) { if (!working.empty()) working += ", "; working += name; }
        add("Working: " + (working.empty() ? std::string("Nobody") : working), "", TaskChoiceKind::Label);
        add(task->done ? "Completed by: " + task->doneBy : "Status: Open", "", TaskChoiceKind::Label);
    } else if (task && view == ui::PromptKind::TaskSubtasks) {
        add("+ Add subtask", "", TaskChoiceKind::AddSubtask);
        for (const auto& sub : task->subtasks) {
            const std::string label = std::string(sub.done ? "[x] " : "[ ] ") + sub.title;
            add(label, sub.done ? "Done by " + sub.doneBy : "By " + sub.createdBy, TaskChoiceKind::ToggleSubtask, sub.id);
        }
    } else if (task && view == ui::PromptKind::TaskHistory) {
        for (auto it = task->history.rbegin(); it != task->history.rend(); ++it) {
            std::string label = it->actor + " - " + it->action;
            if (!it->subject.empty()) label += ": " + it->subject;
            add(label, it->time, TaskChoiceKind::Label);
        }
    }
    const auto pageSize = pagedContentCount();
    const std::size_t pages = std::max<std::size_t>(1, (entries.size() + pageSize - 1) / pageSize);
    m_taskPage = std::min(m_taskPage, pages - 1);
    m_taskChoices.clear();
    const auto append = [this](const Entry& entry) {
        m_suggestionPaths.push_back(entry.label);
        m_suggestionDetails.push_back(entry.detail);
        m_suggestions.push_back(ui::PathSuggestion{m_suggestionPaths.back(), ui::SuggestionKind::Task, m_suggestionDetails.back()});
        m_taskChoices.push_back(entry.choice);
    };
    const auto start = m_taskPage * pageSize;
    for (std::size_t i = start; i < std::min(entries.size(), start + pageSize); ++i) append(entries[i]);
    if (m_taskPage > 0) append({"< Previous page", "", {TaskChoiceKind::PreviousPage, {}}});
    if (m_taskPage + 1 < pages) append({"Next page >", "", {TaskChoiceKind::NextPage, {}}});
    if (m_selectedSuggestion >= static_cast<int>(m_suggestions.size())) m_selectedSuggestion = -1;
}

void Application::applyTaskCommand() {
    const auto view = m_searchBar.kind();
    if (view == ui::PromptKind::TaskInput) {
        std::string value(m_searchBar.query());
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) value.clear();
        else value = value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
        if (m_taskInputKind == TaskInputKind::Identity) {
            if (value.empty() || value.size() > 128) { setCommandStatus("Enter a name of at most 128 bytes", true); return; }
            const std::string previous = m_settings.taskIdentity;
            m_settings.taskIdentity = value;
            if (!persistSettings()) {
                m_settings.taskIdentity = previous;
                setCommandStatus("Could not save your name in settings", true);
                return;
            }
            showTaskView(ui::PromptKind::TaskList);
            return;
        }
        if (m_taskInputKind == TaskInputKind::Title) {
            if (value.empty()) { setCommandStatus("Enter a task title", true); return; }
            m_taskDraftTitle = std::move(value);
            showTaskView(ui::PromptKind::TaskTier);
            return;
        }
        auto board = m_taskBoard;
        auto* task = workspace::findTask(board, m_selectedTaskId);
        if (!task) { setCommandStatus("Task no longer exists", true); return; }
        bool changed = false;
        if (m_taskInputKind == TaskInputKind::Subtask) {
            if (value.empty()) { setCommandStatus("Enter a subtask title", true); return; }
            changed = workspace::addSubtask(*task, value, m_settings.taskIdentity);
        } else if (m_taskInputKind == TaskInputKind::Assignee) {
            changed = workspace::setTaskAssignee(*task, value, m_settings.taskIdentity);
        } else if (m_taskInputKind == TaskInputKind::Solution) {
            changed = workspace::setTaskSolution(*task, value, m_settings.taskIdentity);
        }
        if (changed && !commitTaskBoard(std::move(board))) return;
        showTaskView(m_taskInputKind == TaskInputKind::Subtask ? ui::PromptKind::TaskSubtasks : ui::PromptKind::TaskActions);
        return;
    }
    refreshSuggestions();
    if (m_taskChoices.empty()) { setCommandStatus("No matching tasks", false); return; }
    const auto index = m_selectedSuggestion >= 0 && m_selectedSuggestion < static_cast<int>(m_taskChoices.size())
        ? static_cast<std::size_t>(m_selectedSuggestion) : 0U;
    const TaskChoice choice = m_taskChoices[index];
    if (choice.kind == TaskChoiceKind::PreviousPage || choice.kind == TaskChoiceKind::NextPage) {
        if (choice.kind == TaskChoiceKind::PreviousPage && m_taskPage > 0) --m_taskPage;
        if (choice.kind == TaskChoiceKind::NextPage) ++m_taskPage;
        m_pagedScroll = 0;
        m_selectedSuggestion = -1;
        m_menuReselectionRequired = true;
        refreshSuggestions();
        return;
    }
    switch (choice.kind) {
    case TaskChoiceKind::NewTask:
        m_taskInputKind = TaskInputKind::Title; showTaskView(ui::PromptKind::TaskInput); return;
    case TaskChoiceKind::Identity:
        m_taskInputKind = TaskInputKind::Identity; showTaskView(ui::PromptKind::TaskInput); return;
    case TaskChoiceKind::SelectTask:
        m_selectedTaskId = choice.id; showTaskView(ui::PromptKind::TaskActions); return;
    case TaskChoiceKind::Tier1:
    case TaskChoiceKind::Tier2:
    case TaskChoiceKind::Tier3: {
        const int tier = choice.kind == TaskChoiceKind::Tier1 ? 1 : choice.kind == TaskChoiceKind::Tier2 ? 2 : 3;
        auto board = m_taskBoard; std::string id;
        if (!workspace::createTask(board, m_taskDraftTitle, tier, m_settings.taskIdentity, id)) {
            setCommandStatus("Could not create task", true); return;
        }
        captureTaskContext(*workspace::findTask(board, id));
        if (!commitTaskBoard(std::move(board))) return;
        m_selectedTaskId = std::move(id); showTaskView(ui::PromptKind::TaskActions); return;
    }
    case TaskChoiceKind::OpenWorkspace:
        (void)activateTask(m_selectedTaskId); return;
    case TaskChoiceKind::LeaveWorkspace: {
        if (m_debugAdapter.alive()) { setCommandStatus("Stop the debugger before leaving this task", true); return; }
        if (!saveDocument()) return;
        auto board = m_taskBoard;
        auto* task = workspace::findTask(board, m_activeTaskId);
        if (!task) { setCommandStatus("Active task no longer exists", true); return; }
        captureTaskContext(*task);
        (void)workspace::setTaskWorking(*task, false, m_settings.taskIdentity);
        if (!commitTaskBoard(std::move(board))) return;
        m_activeTaskId.clear(); showTaskView(ui::PromptKind::TaskList); return;
    }
    case TaskChoiceKind::AddSubtask:
        m_taskInputKind = TaskInputKind::Subtask; showTaskView(ui::PromptKind::TaskInput); return;
    case TaskChoiceKind::ViewSubtasks:
        showTaskView(ui::PromptKind::TaskSubtasks); return;
    case TaskChoiceKind::Assign:
        m_taskInputKind = TaskInputKind::Assignee; showTaskView(ui::PromptKind::TaskInput); return;
    case TaskChoiceKind::History:
        showTaskView(ui::PromptKind::TaskHistory); return;
    case TaskChoiceKind::Solution:
        m_taskInputKind = TaskInputKind::Solution; showTaskView(ui::PromptKind::TaskInput); return;
    case TaskChoiceKind::ToggleWorking:
    case TaskChoiceKind::ToggleDone:
    case TaskChoiceKind::ToggleSubtask: {
        auto board = m_taskBoard;
        auto* task = workspace::findTask(board, m_selectedTaskId);
        if (!task) { setCommandStatus("Task no longer exists", true); return; }
        bool changed = false;
        if (choice.kind == TaskChoiceKind::ToggleWorking) {
            const bool working = std::find(task->workingBy.begin(), task->workingBy.end(), m_settings.taskIdentity) != task->workingBy.end();
            changed = workspace::setTaskWorking(*task, !working, m_settings.taskIdentity);
        } else if (choice.kind == TaskChoiceKind::ToggleDone) {
            changed = workspace::setTaskDone(*task, !task->done, m_settings.taskIdentity);
        } else {
            const auto found = std::find_if(task->subtasks.begin(), task->subtasks.end(),
                [&choice](const workspace::Subtask& sub) { return sub.id == choice.id; });
            if (found != task->subtasks.end()) changed = workspace::setSubtaskDone(*task, choice.id, !found->done, m_settings.taskIdentity);
        }
        if (changed && !commitTaskBoard(std::move(board))) return;
        showTaskView(choice.kind == TaskChoiceKind::ToggleSubtask ? ui::PromptKind::TaskSubtasks : ui::PromptKind::TaskActions);
        return;
    }
    case TaskChoiceKind::Label:
        return;
    case TaskChoiceKind::PreviousPage:
    case TaskChoiceKind::NextPage:
        return;
    }
}

void Application::openDocumentSwitcher() {
    if ((!m_hasDocument && m_inactiveDocuments.empty()) || m_settingsPanelVisible || m_compileTerminalFocused) return;
    m_completionOpen = false;
    m_searchBar.open(ui::PromptKind::OpenFiles);
    clearCommandFeedback();
    refreshSuggestions();
    setCommandStatus("Enter: open · Ctrl/Command+W: close selected file", false);
    if (!m_inactiveDocuments.empty()) selectSuggestionRow(m_hasDocument ? 1U : 0U);
}

void Application::closeSwitcherSelection() {
    refreshSuggestions();
    if (m_selectedSuggestion < 0 || static_cast<std::size_t>(m_selectedSuggestion) >= m_suggestions.size()) {
        setCommandStatus("Select a file to close", false); return;
    }
    const std::string path(m_suggestions[static_cast<std::size_t>(m_selectedSuggestion)].path);
    if (m_hasDocument && path == m_filePath) {
        closeDocument();
        if (m_hasDocument && m_filePath == path) { setCommandStatus("Could not close " + path, true); return; }
        if (!m_hasDocument && m_inactiveDocuments.empty()) { m_searchBar.close(); return; }
        openDocumentSwitcher(); setCommandStatus("Closed " + path, false); return;
    }
    const auto file = std::find_if(m_inactiveDocuments.begin(), m_inactiveDocuments.end(),
        [&path](const OpenDocument& item) { return item.path == path; });
    if (file == m_inactiveDocuments.end()) { setCommandStatus("File is no longer open", true); return; }
    if (file->document.dirty()) { setCommandStatus("Open and save " + path + " before closing it", true); return; }
    m_closedDocuments.erase(std::remove_if(m_closedDocuments.begin(), m_closedDocuments.end(),
        [&path](const OpenDocument& item) { return item.path == path; }), m_closedDocuments.end());
    m_closedDocuments.insert(m_closedDocuments.begin(), std::move(*file));
    if (m_closedDocuments.size() > 20) m_closedDocuments.pop_back();
    m_inactiveDocuments.erase(file);
    m_selectedSuggestion = -1; m_suggestionScroll = 0;
    refreshSuggestions(); setCommandStatus("Closed " + path, false);
}

void Application::openProjectPrompt() {
    if (m_searchBar.visible() && m_searchBar.isOpen() &&
        m_searchBar.kind() == ui::PromptKind::Project) {
        m_searchBar.close();
        return;
    }

    focusCompileTerminal(false);
    m_searchBar.open(ui::PromptKind::Project);
    clearCommandFeedback();
    refreshSuggestions();
}

void Application::openFunctionPrompt() {
    if (m_searchBar.visible() && m_searchBar.isOpen() &&
        m_searchBar.kind() == ui::PromptKind::Function) {
        m_searchBar.close();
        return;
    }

    focusCompileTerminal(false);
    m_searchBar.open(ui::PromptKind::Function);
    clearCommandFeedback();
    if (!m_hasProject) {
        setCommandStatus("Open a project first", true);
    } else {
        ensureFunctionIndex();
        if (!m_functionIndexValid || m_functionIndexDirty) setCommandStatus("Indexing project functions…", false);
    }
    refreshSuggestions();
}

void Application::openProjectSearch(bool replace) {
    if (!m_hasProject) { setStatus("Open a project to search", true); return; }
    if (m_projectSearchFuture.valid()) { setStatus("A project search is still running", false); return; }
    if (!saveDocument()) return;
    m_projectSearchReplace = replace; m_projectSearchResults = {}; m_projectSearchPage = 0; m_projectReviewFile.reset(); m_lastProjectSearchMatch.reset();
    m_projectSearchHistoryPosition = m_projectSearchHistory.size(); m_projectSearchDraft.clear();
    m_searchBar.open(ui::PromptKind::ProjectSearch); clearCommandFeedback();
    setCommandStatus(m_projectSearchWholeWord ? "Whole words on; Ctrl/Command+Shift+W toggles; Enter searches" : "Whole words off; Ctrl/Command+Shift+W toggles; Enter searches", false);
}

void Application::reopenProjectSearchResults() {
    if (!m_hasProject || m_project.root().generic_string() != m_projectSearchRoot || m_projectSearchResults.matches.empty()) {
        setStatus("No project search results to return to", false); return;
    }
    if (m_settingsPanelVisible || m_compileTerminalFocused || m_projectSearchFuture.valid()) return;
    m_searchBar.open(ui::PromptKind::ProjectSearchResults); clearCommandFeedback();
    if (m_lastProjectSearchMatch && *m_lastProjectSearchMatch < m_projectSearchResults.matches.size())
        m_projectSearchPage = *m_lastProjectSearchMatch / 6;
    refreshSuggestions();
    if (m_lastProjectSearchMatch) {
        const auto choice = std::find(m_projectSearchChoices.begin(), m_projectSearchChoices.end(), static_cast<int>(*m_lastProjectSearchMatch));
        if (choice != m_projectSearchChoices.end()) selectSuggestionRow(static_cast<std::size_t>(choice - m_projectSearchChoices.begin()));
    }
    setCommandStatus("Search results; select a match to open", false);
}

void Application::pollProjectSearch() {
    if (!m_projectSearchFuture.valid() || m_projectSearchFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    const bool cancelled = m_projectSearchCancel && m_projectSearchCancel->load();
    m_projectSearchCancel.reset();
    workspace::ProjectSearchResults results;
    try { results = m_projectSearchFuture.get(); }
    catch (const std::exception& e) {
        const std::string message = std::string("Project search failed: ") + e.what();
        if (m_hasProject && m_project.root().generic_string() == m_projectSearchRoot &&
            m_searchBar.isOpen() && m_searchBar.kind() == ui::PromptKind::ProjectSearch)
            setCommandStatus(message, true);
        else setStatus(message, true);
        return;
    }
    if (cancelled || !m_hasProject || m_project.root().generic_string() != m_projectSearchRoot || !m_searchBar.isOpen() || m_searchBar.kind() != ui::PromptKind::ProjectSearch) return;
    m_projectSearchResults = std::move(results); m_lastProjectSearchMatch.reset();
    m_projectSearchPage = 0; m_searchBar.open(ui::PromptKind::ProjectSearchResults); clearCommandFeedback(); refreshSuggestions();
    setCommandStatus(std::to_string(m_projectSearchResults.matches.size()) + " matches in " + std::to_string(m_projectSearchResults.files.size()) + " files; " + std::to_string(m_projectSearchResults.skipped) + " skipped" + (m_projectSearchResults.indexIncomplete ? " (directory scan incomplete; replace disabled)" : m_projectSearchResults.truncated ? " (limit reached; replace disabled)" : ""), m_projectSearchResults.indexIncomplete);
}

void Application::applyProjectSearch() {
    const auto kind = m_searchBar.kind();
    if (kind == ui::PromptKind::ProjectSearch) {
        if (m_projectSearchFuture.valid()) { setCommandStatus("Searching…", false); return; }
        m_projectSearchQuery = m_searchBar.query();
        if (m_projectSearchQuery.empty() || m_projectSearchQuery.find_first_of("\r\n") != std::string::npos) { setCommandStatus("Enter a single-line search", true); return; }
        m_projectSearchHistory.erase(std::remove(m_projectSearchHistory.begin(), m_projectSearchHistory.end(), m_projectSearchQuery), m_projectSearchHistory.end());
        m_projectSearchHistory.push_back(m_projectSearchQuery);
        if (m_projectSearchHistory.size() > 100) m_projectSearchHistory.erase(m_projectSearchHistory.begin());
        m_projectSearchHistoryPosition = m_projectSearchHistory.size(); m_projectSearchDraft = m_projectSearchQuery;
        m_projectSearchRoot = m_project.root().generic_string();
        auto project = m_project; const auto query = m_projectSearchQuery; const bool wholeWord = m_projectSearchWholeWord;
        m_projectSearchCancel = std::make_shared<std::atomic_bool>(false);
        m_projectSearchFuture = std::async(std::launch::async, [project, query, wholeWord, cancel = m_projectSearchCancel]() mutable {
            bool complete = false; project.refresh(&complete, cancel.get());
            auto results = workspace::searchProject(project, query, 1000, cancel.get(), wholeWord); results.indexIncomplete = results.indexIncomplete || !complete; return results;
        });
        setCommandStatus("Searching project…", false); return;
    }
    if (kind == ui::PromptKind::ProjectSearchResults) {
        refreshSuggestions();
        const int index = m_selectedSuggestion >= 0 ? m_selectedSuggestion : 0;
        if (index < 0 || static_cast<std::size_t>(index) >= m_projectSearchChoices.size()) return;
        const int choice = m_projectSearchChoices[static_cast<std::size_t>(index)];
        if (choice == -1) { if (m_projectSearchPage) --m_projectSearchPage; m_pagedScroll = 0; m_selectedSuggestion = -1; m_menuReselectionRequired = true; refreshSuggestions(); return; }
        if (choice == -2) { ++m_projectSearchPage; m_pagedScroll = 0; m_selectedSuggestion = -1; m_menuReselectionRequired = true; refreshSuggestions(); return; }
        if (choice == -3) { if (m_projectSearchResults.indexIncomplete || m_projectSearchResults.truncated) { setCommandStatus(m_projectSearchResults.indexIncomplete ? "Directory scan incomplete; resolve access issues and search again" : "Search limit reached; refine before replacing", true); return; } m_searchBar.open(ui::PromptKind::ProjectReplace); clearCommandFeedback(); setCommandStatus("Enter replacement text; empty deletes matches", false); return; }
        if (choice < 0 || static_cast<std::size_t>(choice) >= m_projectSearchResults.matches.size()) return;
        const auto match = m_projectSearchResults.matches[static_cast<std::size_t>(choice)];
        const auto& searchedFile = m_projectSearchResults.files[match.file];
        const auto path = searchedFile.path;
        std::string current, error;
        if (!m_project.readFile(path, current, error) || current != searchedFile.original) { setCommandStatus("File changed since search; search again", true); return; }
        openFile(path);
        if (m_hasDocument && m_filePath == path) {
            m_lastProjectSearchMatch = static_cast<std::size_t>(choice);
            m_document.setCursor(match.line, match.column); m_followCursor = true;
            setStatus("Search match opened; Ctrl/Command+Shift+O returns to results", false);
        }
        return;
    }
    if (kind == ui::PromptKind::ProjectReplace) {
        m_projectReplacement = m_searchBar.query(); m_projectSearchPage = 0; m_projectReviewFile.reset();
        m_projectSearchSelected.assign(m_projectSearchResults.matches.size(), true);
        m_searchBar.open(ui::PromptKind::ProjectReplaceReview); clearCommandFeedback(); refreshSuggestions();
        setCommandStatus("Review affected files. Select Apply to replace; Escape cancels", false); return;
    }
    if (kind == ui::PromptKind::ProjectReplaceReview) {
        refreshSuggestions();
        if (m_selectedSuggestion < 0 || static_cast<std::size_t>(m_selectedSuggestion) >= m_projectSearchChoices.size()) { setCommandStatus("Select Apply after reviewing the files", false); return; }
        const int selection = m_projectSearchChoices[static_cast<std::size_t>(m_selectedSuggestion)];
        if (selection == -1) { if (m_projectSearchPage) --m_projectSearchPage; m_pagedScroll = 0; m_selectedSuggestion = -1; m_menuReselectionRequired = true; refreshSuggestions(); return; }
        if (selection == -2) { ++m_projectSearchPage; m_pagedScroll = 0; m_selectedSuggestion = -1; m_menuReselectionRequired = true; refreshSuggestions(); return; }
        if (selection == -5) { m_projectReviewFile.reset(); m_projectSearchPage = 0; m_pagedScroll = 0; m_selectedSuggestion = -1; refreshSuggestions(); return; }
        if (selection <= -100 && m_projectReviewFile) {
            const auto index = static_cast<std::size_t>(-100 - selection);
            if (index < m_projectSearchResults.matches.size()) {
                if (m_projectSearchSelected.size() != m_projectSearchResults.matches.size()) m_projectSearchSelected.assign(m_projectSearchResults.matches.size(), true);
                m_projectSearchSelected[index] = !m_projectSearchSelected[index];
                setCommandStatus("Match selection changed; Escape returns to files", false); refreshSuggestions();
            }
            return;
        }
        if (selection > 0 && !m_projectReviewFile && static_cast<std::size_t>(selection - 1) < m_projectSearchResults.files.size()) {
            m_projectReviewFile = static_cast<std::size_t>(selection - 1); m_projectSearchPage = 0; m_pagedScroll = 0; m_selectedSuggestion = -1;
            setCommandStatus("Enter toggles a match; Escape returns to files", false); refreshSuggestions(); return;
        }
        if (selection != -4) return;
        workspace::ProjectSearchResults selected;
        selected.truncated = m_projectSearchResults.truncated; selected.indexIncomplete = m_projectSearchResults.indexIncomplete;
        std::vector<std::size_t> remap(m_projectSearchResults.files.size(), static_cast<std::size_t>(-1));
        for (std::size_t i = 0; i < m_projectSearchResults.matches.size(); ++i) {
            if (m_projectSearchSelected.size() == m_projectSearchResults.matches.size() && !m_projectSearchSelected[i]) continue;
            auto match = m_projectSearchResults.matches[i];
            if (match.file >= remap.size()) { setCommandStatus("Invalid search result; search again", true); return; }
            if (remap[match.file] == static_cast<std::size_t>(-1)) {
                remap[match.file] = selected.files.size(); selected.files.push_back(m_projectSearchResults.files[match.file]);
            }
            match.file = remap[match.file]; selected.matches.push_back(std::move(match));
        }
        if (selected.matches.empty()) { setCommandStatus("Select at least one match to replace", true); return; }
        std::size_t changed = 0; std::string error; std::vector<std::string> changedPaths;
        if (m_settingsRoot.empty()) { setCommandStatus("Cannot safely replace: local version history is unavailable", true); return; }
        for (const auto& file : selected.files) {
            if (m_hasDocument && file.path == m_filePath && (m_document.dirty() || m_diskContents != file.original)) {
                setCommandStatus("Open file has unsaved or changed content: " + file.path + "; save and search again", true); return;
            }
            const auto cached = std::find_if(m_inactiveDocuments.begin(), m_inactiveDocuments.end(),
                [&file](const OpenDocument& open) { return open.path == file.path; });
            if (cached != m_inactiveDocuments.end() && (cached->document.dirty() || cached->diskContents != file.original)) {
                setCommandStatus("Open file has unsaved or changed content: " + file.path + "; save and search again", true); return;
            }
        }
        const auto history = std::filesystem::path(m_settingsRoot).parent_path() / "Recovery";
        const bool ok = workspace::replaceProject(m_project, selected, m_projectSearchQuery, m_projectReplacement, changed, error, &changedPaths, history);
        std::string reloadWarning;
        // Reload the active file only if it was changed by this operation and remains unmodified in the editor.
        if (m_hasDocument && !m_document.dirty() && std::find(changedPaths.begin(), changedPaths.end(), m_filePath) != changedPaths.end()) {
            const auto found = std::find_if(m_projectSearchResults.files.begin(), m_projectSearchResults.files.end(), [this](const auto& file) { return file.path == m_filePath; });
            if (found != m_projectSearchResults.files.end()) {
                std::string contents, readError;
                const bool read = m_project.readFile(m_filePath, contents, readError);
                const bool binary = read && std::any_of(contents.begin(), contents.end(), [](unsigned char c) {
                    return c == 0 || (c < 0x20 && c != '\n' && c != '\r' && c != '\t' && c != '\f');
                });
                if (!read || binary || !editor::Document::canEditLosslessly(contents, readError)) {
                    m_document.markRecovered();
                    reloadWarning = "; open file could not be reloaded safely; its previous text remains open";
                } else if (contents != m_diskContents) {
                    const auto line = m_document.cursorLine(), column = m_document.cursorColumn();
                    m_document = editor::Document::fromUtf8(contents); m_document.setCursor(line, column); m_diskContents = contents; m_syntaxValid = false; m_compileScriptValid = false; resetDiagnostics();
                }
            }
        } else if (m_hasDocument && m_document.dirty() && std::find(changedPaths.begin(), changedPaths.end(), m_filePath) != changedPaths.end()) {
            reloadWarning = "; open file has unsaved edits and was not reloaded";
        }
        scheduleProjectIndex(); m_searchBar.close();
        setStatus((ok ? "Replaced matches in " + std::to_string(changed) + " files" : error) + reloadWarning, !ok || !reloadWarning.empty());
    }
}

void Application::openFind() {
    if (!m_hasDocument || m_settingsPanelVisible || m_compileTerminalFocused) return;
    m_completionOpen = false;
    m_replacePending = false;
    m_findAnchor = {m_document.cursorLine(), m_document.cursorColumn()};
    m_searchBar.open(ui::PromptKind::Find);
    clearCommandFeedback();
    setCommandStatus("Enter: next  Shift+Enter: previous", false);
}

void Application::openReplace() {
    if (!m_hasDocument || m_settingsPanelVisible || m_compileTerminalFocused) return;
    if (!(m_searchBar.isOpen() && m_searchBar.kind() == ui::PromptKind::Find)) openFind();
    m_replacePending = true;
    setCommandStatus("Find text; Enter: replacement  Shift+Enter: previous", false);
}

void Application::applyReplacement() {
    if (!m_hasDocument || m_filePath != m_replaceFile ||
        m_document.revision() != m_replaceRevision || !m_document.hasSelection() ||
        m_document.selectionUtf8() != m_replaceQuery) {
        setCommandStatus("Selection changed; search again", true);
        return;
    }
    const std::string replacement(m_searchBar.query());
    if (replacement == m_replaceQuery) {
        const auto end = m_document.selectionEnd();
        m_document.setCursor(end.line, end.column);
    } else if (replacement.empty()) m_document.deleteSelection();
    else m_document.pasteUtf8(replacement);
    m_findAnchor = {m_document.cursorLine(), m_document.cursorColumn()};
    m_searchBar.open(ui::PromptKind::Find);
    m_searchBar.setQuery(m_replaceQuery);
    m_replacePending = true;
    findInDocument(false);
    m_followCursor = true;
}

void Application::findInDocument(bool advance, bool backwards) {
    if (!m_hasDocument || m_searchBar.query().empty()) {
        m_document.clearSelection();
        m_document.setCursor(m_findAnchor.line, m_findAnchor.column);
        setCommandStatus(m_replacePending ? "Find text; Enter: replacement  Shift+Enter: previous" :
                         "Enter: next  Shift+Enter: previous", false);
        return;
    }
    const auto queryDocument = editor::Document::fromUtf8(m_searchBar.query());
    const auto& query = queryDocument.line(0);
    const auto start = advance && m_document.hasSelection()
        ? (backwards ? m_document.selectionStart() : m_document.selectionEnd())
        : m_findAnchor;
    const auto match = editor::findText(m_document.lines(), query, start, backwards);
    if (!match) {
        m_document.clearSelection();
        setCommandStatus("No matches", false);
        return;
    }
    m_document.setCursor(match->line, match->column);
    m_document.extendSelectionTo(match->line, match->column + query.size());
    m_followCursor = true;
    setCommandStatus(m_replacePending ? "Enter: replacement  Shift+Enter: previous" :
                     "Enter: next  Shift+Enter: previous", false);
}

void Application::openCompletion() {
    if (!m_hasDocument || m_searchBar.isOpen() || m_settingsPanelVisible ||
        m_compileTerminalFocused) return;
    m_completionOpen = true;
    m_completionSelected = 0;
    m_completionRequest = 0;
    const auto ext = std::filesystem::path(m_filePath).extension().string();
    if (ext == ".c" || ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".h" || ext == ".hpp") {
        const auto root = m_project.root().generic_string();
        if (!m_languageServer.alive() || m_languageRoot != root) {
            std::string error;
            if (m_languageServer.start(m_project.root(), error)) { m_languageRoot = root; m_languageFile.clear(); }
            else setStatus(error + "; using local suggestions", true);
        }
    }
    m_projectCompletionNames.clear();
    std::size_t scanned = 0;
    for (const auto& path : m_project.files()) {
        if (path == m_filePath || scanned >= 128) continue;
        const auto extension = std::filesystem::path(path).extension().string();
        if (extension != ".c" && extension != ".cpp" && extension != ".h" &&
            extension != ".hpp" && extension != ".py" && extension != ".js" &&
            extension != ".ts" && extension != ".rs" && extension != ".go") continue;
        std::error_code ec;
        std::string resolveError;
        const auto full = m_project.resolve(path, resolveError);
        if (full.empty() || std::filesystem::file_size(full, ec) > 262144 || ec) continue;
        std::string content, error;
        if (!m_project.readFile(path, content, error)) continue;
        ++scanned;
        const auto doc = editor::Document::fromUtf8(content);
        for (const auto& line : doc.lines()) {
            editor::collectFunctionNames(line, m_projectCompletionNames);
        }
    }
    refreshCompletions();
}

void Application::requestLanguageNavigation(bool references) {
    if (!m_hasDocument || !m_hasProject || m_searchBar.isOpen()) return;
    const auto ext = std::filesystem::path(m_filePath).extension().string();
    if (ext != ".c" && ext != ".cpp" && ext != ".cc" && ext != ".cxx" && ext != ".h" && ext != ".hpp") { setStatus("Language navigation currently supports C and C++", true); return; }
    const auto root = m_project.root().generic_string();
    if (!m_languageServer.alive() || m_languageRoot != root) {
        std::string error;
        if (!m_languageServer.start(m_project.root(), error)) { setStatus(error, true); return; }
        m_languageRoot = root; m_languageFile.clear();
    }
    m_languageNavigationFile = m_filePath; m_languageNavigationRevision = m_document.revision();
    m_languageNavigationLine = m_document.cursorLine(); m_languageNavigationColumn = m_document.cursorColumn();
    m_languageReferences = references; m_languageNavigationQueued = true; m_languageNavigationRequest = 0;
    setStatus(references ? "Finding references…" : "Finding definition…", false);
}

void Application::requestLanguageFormatting() {
    if (!m_hasDocument || !m_hasProject || m_searchBar.isOpen()) return;
    const auto ext = std::filesystem::path(m_filePath).extension().string();
    if (ext != ".c" && ext != ".cpp" && ext != ".cc" && ext != ".cxx" && ext != ".h" && ext != ".hpp") {
        setStatus("Formatting currently supports C and C++", true); return;
    }
    const auto root = m_project.root().generic_string();
    if (!m_languageServer.alive() || m_languageRoot != root) {
        std::string error;
        if (!m_languageServer.start(m_project.root(), error)) { setStatus(error, true); return; }
        m_languageRoot = root; m_languageFile.clear();
    }
    m_completionOpen = false;
    m_languageFormatFile = m_filePath; m_languageFormatRevision = m_document.revision();
    m_languageFormatRange.clear();
    if (m_document.hasSelection()) {
        const auto start = m_document.selectionStart(), end = m_document.selectionEnd();
        const auto position = [this](editor::TextPosition pos) {
            std::size_t units = 0;
            const auto& line = m_document.line(pos.line);
            for (std::size_t i = 0; i < pos.column; ++i) units += line[i] > 0xffff ? 2U : 1U;
            return "{\"line\":" + std::to_string(pos.line) + ",\"character\":" + std::to_string(units) + "}";
        };
        m_languageFormatRange = "{\"start\":" + position(start) + ",\"end\":" + position(end) + "}";
    }
    m_languageFormatOriginal = m_document.toUtf8(); m_languageFormatQueued = true; m_languageFormatRequest = 0;
    setStatus(m_languageFormatRange.empty() ? "Formatting document…" : "Formatting selection…", false);
}

void Application::applyLanguageLocation() {
    refreshSuggestions();
    const int selected = m_selectedSuggestion >= 0 ? m_selectedSuggestion : 0;
    if (selected < 0 || static_cast<std::size_t>(selected) >= m_languageLocationChoices.size()) return;
    const int choice = m_languageLocationChoices[static_cast<std::size_t>(selected)];
    if (choice == -1) { if (m_languageLocationPage) --m_languageLocationPage; m_pagedScroll = 0; m_selectedSuggestion = -1; m_menuReselectionRequired = true; refreshSuggestions(); return; }
    if (choice == -2) { ++m_languageLocationPage; m_pagedScroll = 0; m_selectedSuggestion = -1; m_menuReselectionRequired = true; refreshSuggestions(); return; }
    if (choice < 0 || static_cast<std::size_t>(choice) >= m_languageLocations.size()) return;
    const auto location = m_languageLocations[static_cast<std::size_t>(choice)];
    openFile(location.path);
    if (!m_hasDocument || m_filePath != location.path || location.line >= m_document.lineCount()) { setStatus("Language location is no longer available", true); return; }
    const auto& text = m_document.line(location.line);
    std::size_t column = 0, units = 0;
    while (column < text.size() && units + (text[column] > 0xffff ? 2U : 1U) <= location.character) { units += text[column] > 0xffff ? 2U : 1U; ++column; }
    m_document.setCursor(location.line, column); m_followCursor = true;
}

void Application::pollLanguageServer() {
    if (!m_languageServer.alive()) {
        if (!m_languageRoot.empty()) {
            m_languageServer.close(); m_languageRoot.clear(); m_languageFile.clear(); m_completionRequest = 0;
            m_languageNavigationQueued = false; m_languageNavigationRequest = 0;
            m_languageFormatQueued = false; m_languageFormatRequest = 0;
            setStatus("Language server stopped; request a language action to restart it", true);
        }
        return;
    }
    if (!m_hasProject || m_project.root().generic_string() != m_languageRoot) {
        m_languageServer.close(); m_languageRoot.clear(); m_languageFile.clear(); m_completionRequest = 0;
        m_languageNavigationQueued = false; m_languageNavigationRequest = 0;
        m_languageFormatQueued = false; m_languageFormatRequest = 0; return;
    }
    std::vector<std::string> messages; std::string error;
    if (!m_languageServer.poll(messages, error)) {
        m_languageServer.close(); m_languageRoot.clear(); m_languageFile.clear(); m_completionRequest = 0;
        m_languageNavigationQueued = false; m_languageNavigationRequest = 0;
        m_languageFormatQueued = false; m_languageFormatRequest = 0;
        setStatus("Language server stopped: " + error, true); return;
    }
    if ((m_languageFormatQueued || m_languageFormatRequest) &&
        (!m_hasDocument || m_languageFormatFile != m_filePath || m_languageFormatRevision != m_document.revision())) {
        m_languageFormatQueued = false; m_languageFormatRequest = 0;
        setStatus("Formatting cancelled because the document changed", true);
    }
    if ((m_languageNavigationQueued || m_languageNavigationRequest) &&
        (!m_hasDocument || m_languageNavigationFile != m_filePath || m_languageNavigationRevision != m_document.revision())) {
        m_languageNavigationQueued = false; m_languageNavigationRequest = 0;
    }
    if (!m_languageServer.ready() || !m_hasDocument) return;
    const auto ext = std::filesystem::path(m_filePath).extension().string();
    if (ext != ".c" && ext != ".cpp" && ext != ".cc" && ext != ".cxx" && ext != ".h" && ext != ".hpp") return;
    if (m_languageFile != m_filePath) {
        if (!m_languageServer.open(m_project.root() / m_filePath, ext == ".c" ? "c" : "cpp", m_document.toUtf8(), error)) { setStatus(error, true); return; }
        m_languageFile = m_filePath; m_languageRevision = m_document.revision(); m_completionRequest = 0; m_languageDiagnosticsFile.clear();
    } else if (m_languageRevision != m_document.revision()) {
        if (!m_languageServer.change(m_document.toUtf8(), error)) { setStatus(error, true); return; }
        m_languageRevision = m_document.revision(); m_completionRequest = 0;
    }
    const auto line = m_document.cursorLine(), column = m_document.cursorColumn();
    if ((m_languageNavigationQueued || m_languageNavigationRequest != 0) &&
        (m_languageNavigationFile != m_filePath || m_languageNavigationRevision != m_document.revision() || m_languageNavigationLine != line || m_languageNavigationColumn != column)) {
        m_languageNavigationQueued = false; m_languageNavigationRequest = 0;
    }
    if ((m_languageFormatQueued || m_languageFormatRequest) &&
        (m_languageFormatFile != m_filePath || m_languageFormatRevision != m_document.revision())) {
        m_languageFormatQueued = false; m_languageFormatRequest = 0;
        setStatus("Formatting cancelled because the document changed", true);
    }
    for (const auto& message : messages) {
        editor::JsonError parseError; const auto json = editor::parseJson(message, parseError);
        const auto* method = json ? json->find("method") : nullptr;
        if (method && method->type == editor::JsonType::String && method->stringValue == "textDocument/publishDiagnostics") {
            const auto* params = json->find("params");
            const auto* uri = params ? params->find("uri") : nullptr;
            const auto* version = params ? params->find("version") : nullptr;
            const auto* diagnostics = params ? params->find("diagnostics") : nullptr;
            if (!uri || uri->type != editor::JsonType::String || uri->stringValue != editor::lsp::fileUri(m_project.root() / m_filePath) ||
                !version || version->type != editor::JsonType::Number || version->numberValue != m_languageServer.documentVersion() ||
                !diagnostics || diagnostics->type != editor::JsonType::Array) continue;
            std::vector<editor::Diagnostic> converted;
            for (const auto& item : diagnostics->children) {
                const auto* range = item.find("range"); const auto* start = range ? range->find("start") : nullptr;
                const auto* end = range ? range->find("end") : nullptr;
                const auto* lineValue = start ? start->find("line") : nullptr; const auto* charValue = start ? start->find("character") : nullptr;
                const auto* endLine = end ? end->find("line") : nullptr; const auto* endChar = end ? end->find("character") : nullptr;
                const auto* text = item.find("message");
                if (!lineValue || !charValue || !endLine || !endChar || !text || lineValue->type != editor::JsonType::Number || charValue->type != editor::JsonType::Number || endLine->type != editor::JsonType::Number || endChar->type != editor::JsonType::Number || text->type != editor::JsonType::String ||
                    !std::isfinite(lineValue->numberValue) || !std::isfinite(charValue->numberValue) || lineValue->numberValue < 0 || charValue->numberValue < 0 || lineValue->numberValue >= static_cast<double>(m_document.lineCount()) || charValue->numberValue > 10000000 || std::floor(lineValue->numberValue) != lineValue->numberValue || std::floor(charValue->numberValue) != charValue->numberValue) continue;
                const auto row = static_cast<std::size_t>(lineValue->numberValue); const auto& source = m_document.line(row);
                auto columnFor = [&source](std::size_t units) { std::size_t col = 0, count = 0; while (col < source.size() && count + (source[col] > 0xffff ? 2U : 1U) <= units) { count += source[col] > 0xffff ? 2U : 1U; ++col; } return col; };
                const auto startColumn = columnFor(static_cast<std::size_t>(charValue->numberValue));
                auto finish = startColumn + 1;
                if (endLine->numberValue == lineValue->numberValue && std::isfinite(endChar->numberValue) && endChar->numberValue >= 0 && endChar->numberValue <= 10000000) finish = std::max(finish, columnFor(static_cast<std::size_t>(endChar->numberValue)));
                const auto* severity = item.find("severity");
                std::string description = severity && severity->type == editor::JsonType::Number && severity->numberValue == 2 ? "Warning: " + text->stringValue : text->stringValue;
                converted.emplace_back(static_cast<std::uint32_t>(row), static_cast<std::uint32_t>(startColumn + 1), static_cast<std::uint32_t>(finish - startColumn), std::move(description), "clangd");
                if (converted.size() == 200) break;
            }
            m_diagnostics = std::move(converted); m_diagnosticsRevision = m_document.revision(); m_compilerAppliedPath = absoluteDocumentPath(); m_diagnosticsValid = true;
            m_languageDiagnosticsFile = m_filePath; m_languageDiagnosticsRevision = m_document.revision(); updateActiveDiagnostic(); continue;
        }
        const auto* navigationId = json ? json->find("id") : nullptr;
        if (m_languageNavigationRequest && navigationId && navigationId->type == editor::JsonType::Number && navigationId->numberValue == static_cast<double>(m_languageNavigationRequest)) {
            m_languageNavigationRequest = 0; m_languageLocations.clear();
            const auto* problem = json->find("error");
            if (problem) { const auto* description = problem->find("message"); setStatus(description && description->type == editor::JsonType::String ? "Language server: " + description->stringValue : "Language navigation failed", true); continue; }
            const auto* result = json->find("result");
            const auto addLocation = [this](const editor::JsonValue& item) {
                const auto* uri = item.find("uri"); if (!uri) uri = item.find("targetUri");
                const auto* range = item.find("range"); if (!range) range = item.find("targetSelectionRange");
                const auto* start = range ? range->find("start") : nullptr;
                const auto* row = start ? start->find("line") : nullptr; const auto* col = start ? start->find("character") : nullptr;
                if (!uri || uri->type != editor::JsonType::String || !row || !col || row->type != editor::JsonType::Number || col->type != editor::JsonType::Number || !std::isfinite(row->numberValue) || !std::isfinite(col->numberValue) || row->numberValue < 0 || col->numberValue < 0 || row->numberValue > 10000000 || col->numberValue > 10000000 || std::floor(row->numberValue) != row->numberValue || std::floor(col->numberValue) != col->numberValue) return;
                std::filesystem::path absolute; if (!editor::lsp::pathFromFileUri(uri->stringValue, absolute)) return;
                std::error_code ec; absolute = std::filesystem::weakly_canonical(absolute, ec); if (ec) return;
                const auto relative = absolute.lexically_relative(m_project.root());
                if (relative.empty() || *relative.begin() == "..") return;
                std::string error; const auto resolved = m_project.resolve(relative.generic_string(), error);
                if (!error.empty() || resolved != absolute || !m_project.exists(relative.generic_string())) return;
                m_languageLocations.push_back({relative.generic_string(), static_cast<std::size_t>(row->numberValue), static_cast<std::size_t>(col->numberValue)});
            };
            if (result && result->type == editor::JsonType::Array) { for (const auto& item : result->children) { addLocation(item); if (m_languageLocations.size() >= 500) break; } }
            else if (result && result->type == editor::JsonType::Object) addLocation(*result);
            if (m_languageLocations.empty()) { setStatus("No location found in this project", false); continue; }
            m_languageLocationPage = 0; m_searchBar.open(ui::PromptKind::LanguageLocations); clearCommandFeedback(); refreshSuggestions();
            setCommandStatus(std::to_string(m_languageLocations.size()) + " locations; select one to open", false); continue;
        }
        const auto* formatId = json ? json->find("id") : nullptr;
        if (m_languageFormatRequest && formatId && formatId->type == editor::JsonType::Number && formatId->numberValue == static_cast<double>(m_languageFormatRequest)) {
            m_languageFormatRequest = 0;
            const auto* problem = json->find("error");
            if (problem) { const auto* detail = problem->find("message"); setStatus(detail && detail->type == editor::JsonType::String ? "Formatting failed: " + detail->stringValue : "Formatting failed", true); continue; }
            const auto* result = json->find("result");
            if (!result || result->type == editor::JsonType::Null) { setStatus("No formatting changes", false); continue; }
            std::string output, editError;
            if (!editor::lsp::applyTextEdits(m_languageFormatOriginal, *result, output, editError)) { setStatus("Formatting rejected: " + editError, true); continue; }
            const auto unsupportedControl = [](unsigned char c) { return (c < 32 && c != '\n' && c != '\t') || c == 127; };
            if (std::any_of(m_languageFormatOriginal.begin(), m_languageFormatOriginal.end(), unsupportedControl) || std::any_of(output.begin(), output.end(), unsupportedControl)) { setStatus("Formatting rejected: document contains unsupported control characters", true); continue; }
            if (output == m_languageFormatOriginal) { setStatus("Already formatted", false); continue; }
            if (!m_document.replaceAllUtf8(output)) { setStatus("Formatting rejected: invalid text returned by language server", true); continue; }
            m_followCursor = true;
            setStatus("Formatting applied; undo is available", false); continue;
        }
        if (!m_completionOpen) continue;
        const auto* id = json ? json->find("id") : nullptr;
        if (!id || id->type != editor::JsonType::Number || id->numberValue != static_cast<double>(m_completionRequest) ||
            m_completionRequestRevision != m_document.revision() || m_completionRequestLine != line || m_completionRequestColumn != column) continue;
        const auto* result = json->find("result");
        if (result && result->type == editor::JsonType::Object) result = result->find("items");
        if (!result || result->type != editor::JsonType::Array) continue;
        std::vector<std::string> names;
        for (const auto& item : result->children) {
            const auto* label = item.find("label");
            if (!label || label->type != editor::JsonType::String || label->stringValue.empty() || item.find("additionalTextEdits")) continue;
            const auto* format = item.find("insertTextFormat");
            const auto* insert = item.find("insertText");
            if (format && (format->type != editor::JsonType::Number || format->numberValue != 1)) continue;
            const editor::JsonValue* text = insert ? insert : label;
            if (const auto* edit = item.find("textEdit")) {
                const auto* range = edit->find("range"); const auto* start = range ? range->find("start") : nullptr; const auto* end = range ? range->find("end") : nullptr;
                const auto* startLine = start ? start->find("line") : nullptr; const auto* startCol = start ? start->find("character") : nullptr;
                const auto* endLine = end ? end->find("line") : nullptr; const auto* endCol = end ? end->find("character") : nullptr;
                std::size_t startUnits = 0, endUnits = 0; const auto& source = m_document.line(line);
                for (std::size_t i = 0; i < column && i < source.size(); ++i) { if (i < m_completionStart) startUnits += source[i] > 0xffff ? 2 : 1; endUnits += source[i] > 0xffff ? 2 : 1; }
                if (!startLine || !startCol || !endLine || !endCol || startLine->type != editor::JsonType::Number || startCol->type != editor::JsonType::Number || endLine->type != editor::JsonType::Number || endCol->type != editor::JsonType::Number || startLine->numberValue != static_cast<double>(line) || endLine->numberValue != static_cast<double>(line) || startCol->numberValue != static_cast<double>(startUnits) || endCol->numberValue != static_cast<double>(endUnits)) continue;
                text = edit->find("newText");
            }
            if (!text || text->type != editor::JsonType::String) continue;
            const auto& value = text->stringValue;
            if (value.empty()) continue;
            if (!editor::isCompletionIdentifierUtf8(value)) continue;
            if (std::find(names.begin(), names.end(), value) == names.end()) names.push_back(value);
            if (names.size() == 8) break;
        }
        if (!names.empty()) { m_completions = std::move(names); m_completionSelected = 0; }
    }
    if (m_languageNavigationQueued) {
        std::size_t utf16 = 0; const auto& source = m_document.line(line);
        for (std::size_t i = 0; i < column && i < source.size(); ++i) utf16 += source[i] > 0xffff ? 2 : 1;
        const auto uri = debug::jsonString(editor::lsp::fileUri(m_project.root() / m_filePath));
        std::string params = "{\"textDocument\":{\"uri\":" + uri + "},\"position\":{\"line\":" + std::to_string(line) + ",\"character\":" + std::to_string(utf16) + "}";
        if (m_languageReferences) params += ",\"context\":{\"includeDeclaration\":true}";
        params += "}";
        if (!m_languageServer.request(m_languageReferences ? "textDocument/references" : "textDocument/definition", params, m_languageNavigationRequest, error)) setStatus(error, true);
        m_languageNavigationQueued = false;
    }
    if (m_languageFormatQueued) {
        const auto uri = debug::jsonString(editor::lsp::fileUri(m_project.root() / m_filePath));
        std::string params = "{\"textDocument\":{\"uri\":" + uri + "},\"options\":{\"tabSize\":4,\"insertSpaces\":true}";
        if (!m_languageFormatRange.empty()) params += ",\"range\":" + m_languageFormatRange;
        params += "}";
        if (!m_languageServer.request(m_languageFormatRange.empty() ? "textDocument/formatting" : "textDocument/rangeFormatting", params, m_languageFormatRequest, error)) setStatus(error, true);
        m_languageFormatQueued = false;
    }
    if (!m_completionOpen) return;
    if (m_completionRequest != 0 && m_completionRequestRevision == m_document.revision() && m_completionRequestLine == line && m_completionRequestColumn == column) return;
    std::size_t utf16 = 0;
    const auto& text = m_document.line(line);
    for (std::size_t i = 0; i < column && i < text.size(); ++i) utf16 += text[i] > 0xffff ? 2 : 1;
    const auto uri = debug::jsonString(editor::lsp::fileUri(m_project.root() / m_filePath));
    const std::string params = "{\"textDocument\":{\"uri\":" + uri + "},\"position\":{\"line\":" + std::to_string(line) + ",\"character\":" + std::to_string(utf16) + "}}";
    if (!m_languageServer.request("textDocument/completion", params, m_completionRequest, error)) { setStatus(error, true); return; }
    m_completionRequestRevision = m_document.revision(); m_completionRequestLine = line; m_completionRequestColumn = column;
}

void Application::refreshCompletions() {
    m_completions.clear();
    if (!m_hasDocument) { m_completionOpen = false; return; }
    const auto& line = m_document.line(m_document.cursorLine());
    const std::size_t column = m_document.cursorColumn();
    m_completionStart = column;
    while (m_completionStart > 0 && editor::isCompletionIdentifier(line[m_completionStart - 1])) --m_completionStart;
    std::string prefix;
    for (std::size_t i = m_completionStart; i < column; ++i) appendUtf8(prefix, line[i]);
    auto names = m_projectCompletionNames;
    for (const auto& text : m_document.lines()) editor::collectFunctionNames(text, names);
    for (const auto& name : names) {
        if (name.starts_with(prefix)) m_completions.push_back(name);
        if (m_completions.size() == 8) break;
    }
    m_completionSelected = std::clamp(m_completionSelected, 0, std::max(0, static_cast<int>(m_completions.size()) - 1));
}

void Application::openAiSettings() {
    m_searchBar.open(ui::PromptKind::AiSettings); m_selectedSuggestion = -1; clearCommandFeedback();
    setCommandStatus("Local: Qwen2.5 Coder 0.5B. OpenRouter sends context online and may incur costs. Escape goes back.", false);
    refreshSuggestions();
}

void Application::selectSuggestionRow(std::size_t index) {
    m_suggestionScroll = 0;
    m_selectedSuggestion = -1;
    refreshSuggestions();
    const auto visible = m_suggestions.size();
    if (visible == 0) return;
    m_suggestionScroll = index >= visible ? index - visible + 1 : 0;
    refreshSuggestions();
    if (index >= m_suggestionScroll && index - m_suggestionScroll < m_suggestions.size())
        m_selectedSuggestion = static_cast<int>(index - m_suggestionScroll);
}

void Application::applyAiSettings() {
    const auto kind = m_searchBar.kind();
    if (kind == ui::PromptKind::AiKeyInput) {
        if (m_aiSecret.empty()) { setCommandStatus("Enter an API key", true); return; }
        std::string error; const bool ok = credentials::save(m_aiSecret, error);
        std::fill(m_aiSecret.begin(), m_aiSecret.end(), '\0'); m_aiSecret.clear();
        if (!ok) { m_searchBar.setQuery({}); setCommandStatus(error, true); return; }
        openAiSettings(); selectSuggestionRow(6); setCommandStatus("OpenRouter API key saved in system credentials", false); return;
    }
    if (kind == ui::PromptKind::AiModelInput) {
        const std::string model(m_searchBar.query());
        if (model.size() > 200 || std::any_of(model.begin(), model.end(), [](unsigned char c) { return c < 33 || c > 126; })) { setCommandStatus("Use a model ID of at most 200 printable characters", true); return; }
        auto* target = m_aiSettingsWorkflow == 0 ? &m_settings.aiAskModel : m_aiSettingsWorkflow == 1 ? &m_settings.aiCleanupModel : &m_settings.aiDebugModel;
        const auto old = *target; *target = model;
        if (!persistSettings()) { *target = old; setCommandStatus("Could not save settings", true); return; }
        openAiSettings(); selectSuggestionRow(static_cast<std::size_t>(m_aiSettingsWorkflow * 2 + 1)); setCommandStatus("Model saved", false); return;
    }
    const int choice = (m_selectedSuggestion >= 0 ? m_selectedSuggestion : 0) + static_cast<int>(m_suggestionScroll);
    if (choice >= 0 && choice < 6) {
        m_aiSettingsWorkflow = choice / 2;
        if (choice % 2 == 1) {
            const auto& model = m_aiSettingsWorkflow == 0 ? m_settings.aiAskModel : m_aiSettingsWorkflow == 1 ? m_settings.aiCleanupModel : m_settings.aiDebugModel;
            const std::string current = model; m_searchBar.open(ui::PromptKind::AiModelInput); m_searchBar.setQuery(current);
            clearCommandFeedback(); setCommandStatus("Enter an OpenRouter model ID; empty clears it", false); refreshSuggestions(); return;
        }
        auto* provider = m_aiSettingsWorkflow == 0 ? &m_settings.aiAskProvider : m_aiSettingsWorkflow == 1 ? &m_settings.aiCleanupProvider : &m_settings.aiDebugProvider;
        const auto old = *provider; *provider = old == "local" ? "openrouter" : "local";
        if (!persistSettings()) { *provider = old; setCommandStatus("Could not save settings", true); return; }
        refreshSuggestions(); setCommandStatus("Provider saved. OpenRouter sends context online and may incur costs.", false); return;
    }
    if (choice == 6) { m_searchBar.open(ui::PromptKind::AiKeyInput); clearCommandFeedback(); setCommandStatus("Paste key; Enter saves it to system credentials, Escape cancels", false); refreshSuggestions(); return; }
    if (choice == 7) { std::string error; if (!credentials::remove(error)) setCommandStatus(error, true); else setCommandStatus("OpenRouter key removed", false); }
    if (choice == 8) {
        if (m_aiFuture.valid()) { setCommandStatus("An AI operation is already running", true); return; }
        const auto settings = settingsDirectory();
        if (settings.empty()) { setCommandStatus("Model storage directory unavailable", true); return; }
        m_aiCancel = std::make_shared<std::atomic_bool>(false);
        m_aiProgress = std::make_shared<std::atomic<int>>(0);
        m_aiDownloadingModel = true;
        const auto cancel = m_aiCancel; const auto progress = m_aiProgress;
        m_aiFuture = std::async(std::launch::async, [directory = settings.parent_path() / "Models", cancel, progress] {
            AiResult result; std::filesystem::path model;
            try { (void)ai::ensureModel(directory, model, result.error, cancel.get(), progress.get()); }
            catch (const std::exception& e) { result.error = e.what(); }
            return result;
        });
        setCommandStatus("Downloading recommended local model (491 MB): 0%", false);
    }
}

void Application::openAiPrompt() {
    if (m_aiFuture.valid()) {
        setStatus("AI request is still running", false);
        return;
    }
    m_aiReady = false;
    m_aiCleanup = false;
    m_aiDebugExplanation = false; m_aiDebugOpenRouter = false; m_aiDebugContext.clear();
    m_aiResponse.clear();
    m_aiLines.clear();
    m_aiScroll = 0;
    m_aiFile = m_filePath;
    m_aiRevision = m_document.revision();
    m_aiStart = m_document.hasSelection() ? m_document.selectionStart() : editor::TextPosition{m_document.cursorLine(), m_document.cursorColumn()};
    m_aiEnd = m_document.hasSelection() ? m_document.selectionEnd() : m_aiStart;
    m_searchBar.open(ui::PromptKind::Ai);
    clearCommandFeedback();
    setCommandStatus(m_settings.aiAskProvider == "openrouter" ? "OpenRouter receives your prompt, selected code and diagnostic; Enter sends" : "Local AI: first use downloads a 491 MB model", false);
}

void Application::openAiCleanup() {
    if (!m_hasDocument || m_settingsPanelVisible || m_compileTerminalFocused || m_searchBar.isOpen()) return;
    std::string request, error;
    if (!ai::cleanupRequest(m_document, m_filePath, request, error)) { setStatus(error, true); return; }
    openAiPrompt();
    if (!m_searchBar.isOpen() || m_searchBar.kind() != ui::PromptKind::Ai || m_aiFuture.valid()) return;
    m_aiCleanup = true;
    setCommandStatus(m_settings.aiCleanupProvider == "openrouter" ? "OpenRouter receives selected code and surrounding context; Enter sends" : "Clean selection with local AI; Enter to generate, Esc cancels", false);
}

void Application::pollAi() {
    if (!m_aiFuture.valid()) return;
    if (m_aiFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        if (m_aiProgress) {
            const auto value = m_aiProgress->load();
            const auto message = value == ai::kModelGenerating ? std::string("Local AI is generating a response...") :
                value == ai::kModelVerifying ? std::string("Verifying local model integrity...") :
                value == ai::kModelInstalling ? std::string("Installing verified local model...") :
                "Downloading local model: " + std::to_string(std::clamp(value, 0, 100)) + "% (491 MB)";
            if (m_searchBar.isOpen() && (m_searchBar.kind() == ui::PromptKind::Ai || m_searchBar.kind() == ui::PromptKind::AiSettings)) setCommandStatus(message, false); else setStatus(message, false);
        }
        return;
    }
    AiResult result;
    try { result = m_aiFuture.get(); } catch (const std::exception& e) { result.error = e.what(); }
    m_aiProgress.reset();
    const bool cancelled = m_aiCancel && m_aiCancel->load();
    m_aiCancel.reset();
    if (m_aiDownloadingModel) {
        m_aiDownloadingModel = false;
        const std::string message = cancelled ? "Local model download cancelled" : result.error.empty() ? "Recommended local model is ready for local workflows" : result.error;
        const bool failed = !cancelled && !result.error.empty();
        if (m_searchBar.isOpen() && m_searchBar.kind() == ui::PromptKind::AiSettings) setCommandStatus(message, failed);
        else setStatus(message, failed);
        return;
    }
    if (cancelled) return;
    if (!m_searchBar.isOpen() || m_searchBar.kind() != ui::PromptKind::Ai) { if (!result.error.empty()) setStatus(result.error, true); return; }
    if (!result.error.empty()) { setCommandStatus(std::move(result.error), true); return; }
    m_aiResponse = std::move(result.output);
    if (m_aiResponse.empty()) { setCommandStatus("AI returned an empty response. Try again with a shorter request.", true); return; }
    if (m_aiCleanup && !ai::cleanupResponse(m_aiResponse, result.error)) { setCommandStatus(std::move(result.error), true); return; }
    m_aiLines.clear();
    m_aiWrapColumns = 0;
    m_aiScroll = 0;
    m_aiReady = true;
    setCommandStatus(m_aiDebugExplanation ? (m_aiDebugOpenRouter ? "OpenRouter explanation: Up/Down scroll, Enter or Esc closes" : "Local explanation: Up/Down scroll, Enter or Esc closes") : m_aiCleanup ? "Review rewrite: Up/Down scroll, Enter applies, Esc discards" : "AI preview: Up/Down scroll, Enter inserts, Esc dismisses", false);
    refreshSuggestions();
}

void Application::applyCommand(bool forceFolder) {
    if (!m_searchBar.isOpen()) {
        return;
    }
    if (m_searchBar.kind() == ui::PromptKind::SessionRestore) {
        const int choice = (m_selectedSuggestion >= 0 ? m_selectedSuggestion : 0) + static_cast<int>(m_pagedScroll);
        if (choice == 2) {
            m_rememberSessionChoice = !m_rememberSessionChoice;
            refreshSuggestions(); return;
        }
        if (choice < 0 || choice > 1 || !m_pendingSession) return;
        if (choice == 0 && !sessionProjectAvailable(*m_pendingSession)) {
            setCommandStatus("Previous project is unavailable; reconnect it or start fresh", true);
            return;
        }
        if (m_rememberSessionChoice) {
            const auto previousChoice = m_settings.sessionRestore;
            m_settings.sessionRestore = choice == 0 ? "restore" : "empty";
            if (!persistSettings()) {
                m_settings.sessionRestore = previousChoice;
                setCommandStatus(m_statusText, true); return;
            }
        }
        auto previous = std::move(*m_pendingSession);
        m_pendingSession.reset();
        m_rememberSessionChoice = false;
        m_searchBar.close();
        if (choice == 0) restoreSession(previous);
        else setStatus("Started without reopening the previous workspace", false);
        return;
    }
    if (m_searchBar.kind() == ui::PromptKind::LanguageLocations) { applyLanguageLocation(); return; }
    if (m_searchBar.kind() == ui::PromptKind::Diagnostics) { applyDiagnosticList(); return; }
    if (m_searchBar.kind() == ui::PromptKind::File && m_projectIndexFuture.valid()) { setCommandStatus("Indexing project files; try again shortly", false); return; }

    if (m_searchBar.kind() == ui::PromptKind::ProjectSearch || m_searchBar.kind() == ui::PromptKind::ProjectSearchResults || m_searchBar.kind() == ui::PromptKind::ProjectReplace || m_searchBar.kind() == ui::PromptKind::ProjectReplaceReview) { applyProjectSearch(); return; }
    if (m_searchBar.kind() == ui::PromptKind::SettingsMenu) {
        const int choice = (m_selectedSuggestion >= 0 ? m_selectedSuggestion : 0) + static_cast<int>(m_suggestionScroll);
        if (m_settingsMenuPage == 8) {
            if (choice < 0 || static_cast<std::size_t>(choice) >= m_backgroundSceneChoices.size()) return;
            const int selected = m_backgroundSceneChoices[static_cast<std::size_t>(choice)];
            if (selected == -1) { if (m_backgroundScenePage > 0) --m_backgroundScenePage; m_selectedSuggestion = -1; refreshSuggestions(); return; }
            if (selected == -2) { ++m_backgroundScenePage; m_selectedSuggestion = -1; refreshSuggestions(); return; }
            if (selected >= 0 && static_cast<std::size_t>(selected) < backgroundNames.size()) {
                m_allowBackgroundSceneOverride = false;
                m_settings.backgroundScene = selected;
                m_settings.backgroundMotion = true;
                applySettings(m_settings);
                const bool saved = persistSettings();
                if (saved) setStatus(std::string("Background: ") + backgroundNames[static_cast<std::size_t>(selected)], false);
                refreshSuggestions(); setCommandStatus(m_statusText, m_statusIsError);
            }
        } else if (m_settingsMenuPage == 0) {
            if (choice < 7) {
                m_settingsMenuPage = choice + 1;
                clearCommandFeedback();
                setCommandStatus("Enter applies; Escape goes back", false);
                refreshSuggestions();
            }
            else if (choice == 7) openAiSettings();
            else if (choice == 8) { m_searchBar.close(); openSettingsJson(); }
        } else {
            const auto& choices = settingsChoices(m_settingsMenuPage);
            if (choice >= 0 && static_cast<std::size_t>(choice) < choices.size()) {
                if (choices[static_cast<std::size_t>(choice)].second == ui::SettingsControl::BackgroundScene) {
                    m_settingsMenuPage = 8; m_backgroundScenePage = 0; m_selectedSuggestion = -1;
                    clearCommandFeedback(); setCommandStatus("Choose a scene; Escape goes back", false); refreshSuggestions(); return;
                }
                activateSettingsControl(choices[static_cast<std::size_t>(choice)].second);
                refreshSuggestions(); setCommandStatus(m_statusText, m_statusIsError);
            }
        }
        return;
    }
    if (m_searchBar.kind() == ui::PromptKind::AiSettings || m_searchBar.kind() == ui::PromptKind::AiModelInput || m_searchBar.kind() == ui::PromptKind::AiKeyInput) { applyAiSettings(); return; }
    if (m_searchBar.kind() == ui::PromptKind::DebugMenu || m_searchBar.kind() == ui::PromptKind::DebugInput || m_searchBar.kind() == ui::PromptKind::DebugStack || m_searchBar.kind() == ui::PromptKind::DebugThreads || m_searchBar.kind() == ui::PromptKind::DebugVariables || m_searchBar.kind() == ui::PromptKind::DebugOutput) { applyDebugCommand(); return; }
    if (m_searchBar.kind() == ui::PromptKind::RecoveryList || m_searchBar.kind() == ui::PromptKind::RecoveryDetail) { applyRecovery(); return; }
    if (m_searchBar.kind() == ui::PromptKind::ModuleList || m_searchBar.kind() == ui::PromptKind::ModuleActions || m_searchBar.kind() == ui::PromptKind::ModuleCommands || m_searchBar.kind() == ui::PromptKind::ModuleImport || m_searchBar.kind() == ui::PromptKind::ModulePreview || m_searchBar.kind() == ui::PromptKind::ModuleCatalog) { applyModuleCommand(); return; }
    if (m_searchBar.kind() == ui::PromptKind::GitChanges || m_searchBar.kind() == ui::PromptKind::GitFile || m_searchBar.kind() == ui::PromptKind::GitDiff || m_searchBar.kind() == ui::PromptKind::GitCommitReview || m_searchBar.kind() == ui::PromptKind::GitCommitInput || m_searchBar.kind() == ui::PromptKind::GitRemoteReview || m_searchBar.kind() == ui::PromptKind::GitHistory) { applyGitReview(); return; }
    if (isTaskPrompt(m_searchBar.kind())) {
        applyTaskCommand();
        return;
    }
    if (m_searchBar.kind() == ui::PromptKind::Ai) {
        if (m_aiFuture.valid()) return;
        if (m_aiReady) {
            if (m_aiDebugExplanation) { m_searchBar.close(); m_aiReady = false; return; }
            if (!m_hasDocument || m_filePath != m_aiFile || m_document.revision() != m_aiRevision) {
                setCommandStatus("Document changed; request a new suggestion", true);
                return;
            }
            m_document.setCursor(m_aiStart.line, m_aiStart.column);
            m_document.extendSelectionTo(m_aiEnd.line, m_aiEnd.column);
            m_document.pasteUtf8(m_aiResponse);
            m_searchBar.close();
            m_aiReady = false;
            m_followCursor = true;
            return;
        }
        const std::string query(m_searchBar.query());
        if (!m_aiCleanup && query.empty()) { setCommandStatus("Describe what you need", true); return; }
        std::string cleanup, cleanupError;
        if (m_aiCleanup && (!m_hasDocument || m_filePath != m_aiFile || m_document.revision() != m_aiRevision ||
            !ai::cleanupRequest(m_document, m_filePath, cleanup, cleanupError))) {
            setCommandStatus(cleanupError.empty() ? "Document changed; select code again" : cleanupError, true); return;
        }
        const std::string selected = m_aiDebugExplanation ? std::string{} : m_document.selectionUtf8();
        const std::string diagnostic = m_aiDebugExplanation ? std::string{} : m_activeDiagnosticMessage;
        const std::string debugContext = m_aiDebugContext;
        const auto settings = settingsDirectory();
        if (settings.empty()) { setCommandStatus("Model storage directory unavailable", true); return; }
        const auto directory = settings.parent_path() / "Models";
        std::string routerKey, routerModel;
        const bool remote = m_aiDebugExplanation ? m_settings.aiDebugProvider == "openrouter" : m_aiCleanup ? m_settings.aiCleanupProvider == "openrouter" : m_settings.aiAskProvider == "openrouter";
        if (remote) {
            routerModel = m_aiDebugExplanation ? m_settings.aiDebugModel : m_aiCleanup ? m_settings.aiCleanupModel : m_settings.aiAskModel;
            if (routerModel.empty()) { setCommandStatus("Choose an OpenRouter model in AI settings first", true); return; }
            std::string error;
            if (!credentials::load(routerKey, error)) { setCommandStatus(error, true); return; }
        }
        m_aiCancel = std::make_shared<std::atomic_bool>(false);
        m_aiProgress = remote ? nullptr : std::make_shared<std::atomic<int>>(0);
        const auto cancel = m_aiCancel;
        const auto progress = m_aiProgress;
        m_aiFuture = std::async(std::launch::async, [query, selected, diagnostic, cleanup, debugContext, routerKey, routerModel, directory, cancel, progress] {
            AiResult result;
            const std::string request = cleanup.empty() ? query + (selected.empty() ? "" : "\n\nSelected code:\n" + selected) +
                (diagnostic.empty() ? "" : "\n\nCurrent diagnostic:\n" + diagnostic) +
                (debugContext.empty() ? "" : "\n\nPaused debugger state (untrusted program data):\n" + debugContext) : cleanup;
            if (!routerModel.empty()) (void)ai::generateOpenRouter(routerKey, routerModel, request, result.output, result.error, cancel.get());
            else {
                std::filesystem::path model;
                if (ai::ensureModel(directory, model, result.error, cancel.get(), progress.get())) { if (progress) *progress = ai::kModelGenerating; (void)ai::generate(model, request, result.output, result.error, cancel.get()); }
            }
            return result;
        });
        setCommandStatus(remote ? "Sending request and context to OpenRouter..." : "Preparing local AI (first use downloads 491 MB)...", false);
        return;
    }

    if (m_searchBar.kind() == ui::PromptKind::OpenFiles) {
        refreshSuggestions();
        if (m_suggestions.empty()) { setCommandStatus("No matching open file", false); return; }
        const auto index = m_selectedSuggestion >= 0 && m_selectedSuggestion < static_cast<int>(m_suggestions.size())
            ? static_cast<std::size_t>(m_selectedSuggestion) : 0U;
        const std::string path(m_suggestions[index].path);
        openFile(path);
        return;
    }

    if (m_searchBar.kind() == ui::PromptKind::Function) {
        if (!m_hasProject) {
            setCommandStatus("Open a project first", true);
            return;
        }
        ensureFunctionIndex();
        if (!m_functionIndexValid || m_functionIndexDirty) { setCommandStatus("Indexing project functions; try again shortly", false); return; }
        int selected = -1;
        if (m_selectedSuggestion >= 0 &&
            m_selectedSuggestion < static_cast<int>(m_suggestionFunctionIndices.size())) {
            selected = static_cast<int>(
                m_suggestionFunctionIndices[static_cast<std::size_t>(m_selectedSuggestion)]);
        } else {
            if (!m_suggestionFunctionIndices.empty()) selected = static_cast<int>(m_suggestionFunctionIndices.front());
        }
        if (selected < 0 ||
            selected >= static_cast<int>(m_functionIndex.functions.size())) {
            setCommandStatus("No matching project function", true);
            return;
        }
        const workspace::ProjectFunction function =
            m_functionIndex.functions[static_cast<std::size_t>(selected)];
        m_searchBar.close();
        runProjectFunction(function);
        return;
    }

    if (m_searchBar.kind() == ui::PromptKind::Project) {
        const bool hasSelection =
            m_selectedSuggestion >= 0 &&
            m_selectedSuggestion < static_cast<int>(m_suggestions.size());
        const std::string path =
            hasSelection
                ? std::string(m_suggestions[static_cast<std::size_t>(
                      m_selectedSuggestion)]
                                  .path)
                : std::string(m_searchBar.query());
        if (path.empty()) {
            setCommandStatus("Type a project path", true);
            return;
        }
        openProject(path);
        return;
    }

    if (!m_hasProject) {
        setCommandStatus("Open a project first", true);
        return;
    }

    const std::string path = commandPath();
    if (path.empty()) {
        setCommandStatus("Type a file path", true);
        return;
    }

    if (forceFolder || path.back() == '/') {
        createFolder(path);
        return;
    }

    if (m_project.isDirectory(path)) {
        std::string folder = path;
        while (!folder.empty() && folder.back() == '/') {
            folder.pop_back();
        }
        m_searchBar.setQuery(folder + "/");
        setCommandStatus("Folder " + folder + "/", false);
        m_selectedSuggestion = -1;
        refreshSuggestions();
        return;
    }

    openFile(path);
}

void Application::createFolder(const std::string& path) {
    if (!m_hasProject) {
        setCommandStatus("Open a project first", true);
        return;
    }

    std::string relative = normalizeRelative(path);
    while (!relative.empty() && relative.back() == '/') {
        relative.pop_back();
    }
    if (relative.empty()) {
        setCommandStatus("Type a folder path", true);
        return;
    }

    const bool existed = m_project.isDirectory(relative);
    if (!existed) {
        std::string error;
        if (!m_project.createDirectory(relative, error)) {
            setCommandStatus(std::move(error), true);
            return;
        }
        scheduleProjectIndex();
    }

    setCommandStatus((existed ? "Folder " : "Created folder ") + relative + "/", false);
    if (!existed) {
        setStatus("Created folder " + relative, false);
    }
    m_searchBar.setQuery(relative + "/");
    m_selectedSuggestion = -1;
    m_suggestionEpoch = m_lastTimeSeconds;
    refreshSuggestions();
}

void Application::openExternalPath(const std::string& path) {
    std::error_code ec;
    const auto absolute = std::filesystem::canonical(std::filesystem::path(path), ec);
    if (ec || !std::filesystem::exists(absolute, ec) || ec) {
        setStatus("Could not open file: " + path, true);
        return;
    }
    const bool isDirectory = std::filesystem::is_directory(absolute, ec);
    const bool isRegularFile = !ec && !isDirectory && std::filesystem::is_regular_file(absolute, ec);
    if (ec || (!isDirectory && !isRegularFile)) {
        setStatus("Not a regular file: " + path, true);
        return;
    }
    const auto discardPendingSession = [this] {
        if (!m_pendingSession) return;
        m_pendingSession.reset();
        m_rememberSessionChoice = false;
        if (m_searchBar.kind() == ui::PromptKind::SessionRestore) m_searchBar.close();
        std::string error;
        if (!saveSession(error)) log::warn("Could not persist the opened workspace: " + error);
    };
    if (isDirectory) {
        if (openProject(absolute.string())) discardPendingSession();
        return;
    }
    if (m_hasProject) {
        const auto relative = absolute.lexically_relative(m_project.root());
        if (!relative.empty() && *relative.begin() != "..") {
            openFile(relative.generic_string(), false);
            if (m_hasDocument && m_filePath == relative.generic_string() && !m_searchBar.isOpen()) discardPendingSession();
            return;
        }
    }
    const auto parent = absolute.parent_path();
    if (openProject(parent.string())) {
        discardPendingSession();
        openFile(absolute.filename().string(), false);
        if (!m_recoveryFiles.empty()) openRecovery();
    }
}

bool Application::openProject(const std::string& path) {
    if (m_debugAdapter.alive()) { setStatus("Stop the debugger before switching projects", true); return false; }
    if (!saveDocument()) {
        setCommandStatus("Could not save the open file", true);
        return false;
    }

    bool created = false;
    std::string error;
    workspace::Project project =
        workspace::Project::openOrCreate(resolveProjectPath(path), created, error, false, !m_restoringSession);
    if (!error.empty()) {
        setCommandStatus(std::move(error), true);
        return false;
    }

    if (!saveActiveTaskContext()) return false;
    m_inactiveDocuments.clear();
    m_closedDocuments.clear();
    m_activeTaskId.clear();
    m_selectedTaskId.clear();
    m_taskBoard = {};
    m_project = std::move(project);
    if (!m_restoringSession && m_project.root().generic_string() != m_unrestoredProject) {
        m_unrestoredProject.clear();
        m_unrestoredFiles.clear();
    }
    m_debugBreakpoints.clear();
    m_recoveryFiles.clear();
    m_projectName = m_project.name();
    m_hasProject = true;
    scheduleProjectIndex();
    m_mouseSelecting = false;

    m_recentProjects.erase(
        std::remove(m_recentProjects.begin(), m_recentProjects.end(),
                    m_project.root().string()),
        m_recentProjects.end());
    m_recentProjects.insert(m_recentProjects.begin(), m_project.root().string());
    if (m_recentProjects.size() > 8) {
        m_recentProjects.resize(8);
    }
    closeCompileTerminal();
    m_hasDocument = false;
    m_filePath.clear();
    m_document = editor::Document{};
    m_firstLine = 0;
    m_followCursor = true;
    m_syntax.clear();
    m_visibleTokens.clear();
    m_syntaxValid = false;
    m_compileScript = editor::CompileScript{};
    m_compileScriptValid = false;
    resetDiagnostics();
    m_suggestions.clear();
    m_suggestionPaths.clear();
    m_suggestionDetails.clear();
    m_suggestionFunctionIndices.clear();
    m_selectedSuggestion = -1;
    m_searchBar.close();
    m_functionIndex = workspace::FunctionIndex{};
    m_functionIndexValid = false;
    m_functionIndexDirty = true;
    ++m_functionIndexGeneration;
    m_dragTarget = DragTarget::None;

    setStatus((created ? "Created project " : "Opened project ") + m_projectName, false);
    if (!m_restoringSession) { std::string sessionError; if (!saveSession(sessionError)) log::warn("Could not persist recent projects: " + sessionError); }
    const auto recoveryRoot = std::filesystem::path(m_settingsRoot).parent_path() / "Recovery";
    std::error_code recoveryError;
    if (!m_settingsRoot.empty() && !m_settingsPanelVisible && std::filesystem::is_directory(recoveryRoot, recoveryError)) {
        openRecovery(false);
        if (m_recoveryFiles.empty()) m_searchBar.close();
    }
    return true;
}

void Application::openFile(const std::string& path, bool allowCreate) {
    m_editorScrollRemainder = 0.0;
    if (!m_hasProject) {
        setCommandStatus("Open a project first", true);
        return;
    }

    const std::string relative = normalizeRelative(path);
    if (relative.empty()) {
        setCommandStatus("Type a file path", true);
        return;
    }
    if (relative.back() == '/') {
        createFolder(relative);
        return;
    }

    if (m_hasDocument && relative == m_filePath) {
        if (!saveDocument()) {
            setCommandStatus("Could not save " + relative, true);
            return;
        }
        std::string content, error;
        if (!m_project.readFile(relative, content, error)) {
            setCommandStatus("Cannot reopen file: " + error, true);
            setStatus("Cannot reopen file: " + error, true);
            return;
        }
        if (content != m_diskContents) {
            const bool binary = std::any_of(content.begin(), content.end(), [](unsigned char c) {
                return c == 0 || (c < 0x20 && c != '\n' && c != '\r' && c != '\t' && c != '\f');
            });
            if (binary || !editor::Document::canEditLosslessly(content, error)) {
                const std::string message = binary ? "File changed to binary data; existing text remains open" : "Cannot reopen safely: " + error;
                setCommandStatus(message, true); setStatus(message, true); return;
            }
            const auto line = m_document.cursorLine(), column = m_document.cursorColumn();
            m_document = editor::Document::fromUtf8(content);
            m_document.setCursor(line, column);
            m_diskContents = content;
            m_followCursor = false;
            m_syntaxValid = false; m_compileScriptValid = false;
            m_languageFile.clear(); m_languageDiagnosticsFile.clear(); m_accessibilityKey.clear();
            m_completionOpen = false; m_completionRequest = 0;
            m_languageNavigationQueued = false; m_languageNavigationRequest = 0;
            m_languageFormatQueued = false; m_languageFormatRequest = 0;
            if (isSettingsDocument()) {
                m_settingsBaselineRoot = m_settingsRoot;
                if (parseSettings(content).valid) m_settingsBaseline = content;
                else m_settingsBaseline.reset();
                m_settingsRevision = ~std::uint64_t{0};
                refreshSettings();
            }
            resetDiagnostics();
            setStatus("Reloaded externally changed file " + relative, false);
        }
        m_searchBar.close();
        return;
    }

    if (!saveDocument()) {
        setCommandStatus("Could not save " + m_filePath, true);
        return;
    }

    if (m_project.isDirectory(relative)) {
        setCommandStatus("Path is a folder: " + relative, true);
        return;
    }

    bool created = false;
    std::string error;
    if (!m_project.exists(relative)) {
        if (!allowCreate) {
            setCommandStatus("File is no longer available: " + relative, true);
            return;
        }
        const bool awaitingRestore = m_project.root().generic_string() == m_unrestoredProject &&
            std::any_of(m_unrestoredFiles.begin(), m_unrestoredFiles.end(),
                        [&relative](const auto& file) { return file.path == relative; });
        if (awaitingRestore) {
            setCommandStatus("Previously open file is unavailable; reconnect it before opening: " + relative, true);
            return;
        }
        const auto cachedFile = std::find_if(m_inactiveDocuments.begin(), m_inactiveDocuments.end(),
            [&relative](const OpenDocument& file) { return file.path == relative; });
        if (cachedFile != m_inactiveDocuments.end()) {
            setCommandStatus("Open file no longer exists: " + relative, true);
            setStatus("Open file no longer exists: " + relative, true);
            return;
        }
        if (!m_project.createFile(relative, error)) {
            setCommandStatus(std::move(error), true);
            return;
        }
        created = true;
    }

    std::string content;
    if (!m_project.readFile(relative, content, error)) {
        setCommandStatus(std::move(error), true);
        return;
    }

    // NUL bytes and non-whitespace control characters are strong indicators of
    // binary data. Do not feed such data to the text editor.
    const bool binary = std::any_of(content.begin(), content.end(), [](unsigned char c) {
        return c == 0 || (c < 0x20 && c != '\n' && c != '\r' && c != '\t' && c != '\f');
    });
    if (binary) {
        if (!platform::openBinaryFile((m_project.root() / relative).string())) {
            setCommandStatus("Binary file was not opened: " + relative, false);
        }
        return;
    }

    if (!editor::Document::canEditLosslessly(content, error)) {
        setCommandStatus(error, true); setStatus(error, true); return;
    }

    editor::Document next = editor::Document::fromUtf8(content);
    int nextFirstLine = 0;
    const auto cached = std::find_if(m_inactiveDocuments.begin(), m_inactiveDocuments.end(),
        [&relative](const OpenDocument& file) { return file.path == relative; });
    bool restoredContext = false;
    if (cached != m_inactiveDocuments.end()) {
        // Reload if another program changed the file while it was inactive.
        if (cached->document.toUtf8() == next.toUtf8()) {
            next = std::move(cached->document);
            nextFirstLine = cached->firstLine;
            restoredContext = true;
        }
        m_inactiveDocuments.erase(cached);
    } else if (m_project.root().generic_string() == m_unrestoredProject) {
        const auto saved = std::find_if(m_unrestoredFiles.begin(), m_unrestoredFiles.end(),
            [&relative](const auto& file) { return file.path == relative; });
        if (saved != m_unrestoredFiles.end()) {
            next.setCursor(saved->line, saved->column);
            nextFirstLine = std::max(0, saved->firstLine);
            restoredContext = true;
        }
    }
    if (m_hasDocument) {
        m_inactiveDocuments.insert(m_inactiveDocuments.begin(),
            OpenDocument{m_filePath, std::move(m_document), m_firstLine, m_diskContents});
    }
    closeCompileTerminal();
    m_document = std::move(next);
    m_languageFile.clear(); m_languageDiagnosticsFile.clear();
    m_accessibilityKey.clear();
    m_hasDocument = true;
    m_filePath = relative;
    if (m_project.root().generic_string() == m_unrestoredProject) {
        m_unrestoredFiles.erase(std::remove_if(m_unrestoredFiles.begin(), m_unrestoredFiles.end(),
            [&relative](const auto& file) { return file.path == relative; }), m_unrestoredFiles.end());
    }
    m_diskContents = content;
    m_firstLine = nextFirstLine;
    m_followCursor = !restoredContext;
    m_syntaxValid = false;
    m_compileScriptValid = false;
    resetDiagnostics();
    if (created) {
        scheduleProjectIndex();
    }
    m_searchBar.close();

    setStatus((created ? "Created " : "Opened ") + relative, false);
    const auto extension = std::filesystem::path(relative).extension().string();
    const bool cpp = extension == ".c" || extension == ".cpp" || extension == ".cc" || extension == ".cxx" || extension == ".h" || extension == ".hpp";
    const auto root = m_project.root().generic_string();
    if (cpp && (!m_languageServer.alive() || m_languageRoot != root) && m_languageAttemptRoot != root) {
        m_languageAttemptRoot = root;
        std::string languageError;
        if (m_languageServer.start(m_project.root(), languageError)) m_languageRoot = root;
        else setStatus("clangd unavailable; using built-in diagnostics and completion", true);
    }
}

bool Application::saveDocument() {
    if (!m_hasDocument || !m_document.dirty()) {
        return true;
    }
    if (!m_hasProject) {
        setStatus("No project is open", true);
        return false;
    }

    std::string error;
    std::string disk;
    if (!m_project.readFile(m_filePath, disk, error)) {
        setStatus("Cannot save: file was removed or cannot be read. Your changes remain open.", true);
        return false;
    }
    if (disk != m_diskContents) {
        setStatus("File changed outside Lucid; save blocked. Open Recovery to save a copy of your edits.", true);
        return false;
    }
    std::string contents;
    if (!m_document.serializeForFile(m_diskContents, contents, error)) { setStatus("Cannot save safely: " + error, true); return false; }
    if (contents.size() > workspace::Project::maxFileSize) {
        setStatus("Cannot save: file exceeds Lucid's 16 MiB limit. Your edits remain open.", true);
        return false;
    }
    if (m_settingsRoot.empty()) { setStatus("Cannot save safely: local version history is unavailable", true); return false; }
    workspace::recovery::Snapshot previous;
    previous.project = m_project.root().generic_string(); previous.file = m_filePath;
    previous.contents = m_diskContents;
    previous.line = m_document.cursorLine(); previous.column = m_document.cursorColumn(); previous.firstLine = m_firstLine;
    if (!workspace::recovery::recordHistory(std::filesystem::path(m_settingsRoot).parent_path() / "Recovery", previous, error)) {
        setStatus("Cannot save safely; could not create a previous-version backup: " + error, true); return false;
    }
    if (!m_project.writeFile(m_filePath, contents, error, &m_diskContents)) {
        setStatus(std::move(error), true);
        return false;
    }
    m_diskContents = contents;
    m_document.markSaved();
    if (isSettingsDocument()) {
        m_settingsBaselineRoot = m_settingsRoot;
        if (parseSettings(contents).valid) m_settingsBaseline = contents;
        else m_settingsBaseline.reset();
    }
    invalidateFunctionIndex();
    setStatus("Saved " + m_filePath, false);
    return true;
}

void Application::deleteCurrentFile() {
    if (!m_hasDocument) {
        setStatus("No file is open", false);
        return;
    }
    if (!m_hasProject) {
        setStatus("No project is open", true);
        return;
    }

    const std::string deleted = m_filePath;
    const std::string fullPath = absoluteDocumentPath();
    if (!platform::confirmDestructiveAction(
            "Delete File", "Delete " + fullPath +
                               "?\n\nThis cannot be undone.",
            "Delete")) {
        setStatus("Deletion cancelled", false);
        return;
    }

    std::string error;
    if (!m_project.deleteFile(deleted, error)) {
        setStatus(std::move(error), true);
        return;
    }
    scheduleProjectIndex();

    closeCompileTerminal();
    m_hasDocument = false;
    m_filePath.clear();
    m_document = editor::Document{};
    m_firstLine = 0;
    m_followCursor = true;
    m_mouseSelecting = false;
    m_syntax.clear();
    m_visibleTokens.clear();
    m_syntaxValid = false;
    m_compileScript = editor::CompileScript{};
    m_compileScriptValid = false;
    resetDiagnostics();
    m_searchBar.close();
    invalidateFunctionIndex();
    setStatus("Deleted " + deleted, false);
}

void Application::deleteCurrentProject() {
    if (m_debugAdapter.alive()) { setStatus("Stop the debugger before deleting the project", true); return; }
    if (!m_hasProject) {
        setStatus("No project is open", true);
        return;
    }

    const std::filesystem::path root = m_project.root();
    std::error_code pathError;
    const std::filesystem::path canonicalRoot =
        std::filesystem::weakly_canonical(root, pathError);
    const std::filesystem::path resolvedRoot =
        pathError ? root.lexically_normal() : canonicalRoot;

    const auto normalized = [](const std::filesystem::path& path) {
        std::error_code error;
        const std::filesystem::path canonical =
            std::filesystem::weakly_canonical(path, error);
        return error ? path.lexically_normal() : canonical;
    };

    const bool protectedRoot =
        resolvedRoot == resolvedRoot.root_path() ||
        resolvedRoot == normalized(homeDirectory()) ||
        resolvedRoot == normalized(projectsDirectory()) ||
        (!m_settingsRoot.empty() &&
         resolvedRoot == normalized(std::filesystem::path(m_settingsRoot)));
    if (protectedRoot) {
        setStatus("Refusing to delete this folder", true);
        return;
    }

    if (!platform::confirmDestructiveAction(
            "Delete Project", "Delete the project " + m_projectName +
                                  "?\n\n" + resolvedRoot.generic_string() +
                                  "\n\nThis cannot be undone.",
            "Delete")) {
        setStatus("Deletion cancelled", false);
        return;
    }

    const std::string projectName = m_projectName;
    closeProject();

    std::error_code error;
    const std::uintmax_t removed = std::filesystem::remove_all(root, error);
    if (error) {
        setStatus("Could not delete project: " + error.message(), true);
        return;
    }
    if (removed == 0) {
        setStatus("Project folder was not found", true);
        return;
    }

    if (m_unrestoredProject == root.generic_string()) {
        m_unrestoredProject.clear();
        m_unrestoredFiles.clear();
    }
    setStatus("Deleted project " + projectName, false);
}

void Application::closeProject() {
    closeCompileTerminal();
    const std::string root = m_project.root().string();
    m_project = workspace::Project{};
    m_taskBoard = {};
    m_activeTaskId.clear();
    m_selectedTaskId.clear();
    m_inactiveDocuments.clear();
    m_closedDocuments.clear();
    m_hasProject = false;
    m_projectName.clear();
    m_hasDocument = false;
    m_filePath.clear();
    m_document = editor::Document{};
    m_firstLine = 0;
    m_followCursor = true;
    m_mouseSelecting = false;
    m_syntax.clear();
    m_visibleTokens.clear();
    m_syntaxValid = false;
    m_compileScript = editor::CompileScript{};
    m_compileScriptValid = false;
    resetDiagnostics();
    m_suggestions.clear();
    m_suggestionPaths.clear();
    m_suggestionDetails.clear();
    m_suggestionFunctionIndices.clear();
    m_selectedSuggestion = -1;
    m_searchBar.close();
    m_settingsPanelVisible = false;
    m_functionIndex = workspace::FunctionIndex{};
    m_functionIndexValid = false;
    m_functionIndexDirty = true;
    ++m_functionIndexGeneration;
    m_dragTarget = DragTarget::None;
    m_recentProjects.erase(
        std::remove(m_recentProjects.begin(), m_recentProjects.end(), root),
        m_recentProjects.end());
}

void Application::closeDocument() {
    if (!m_hasDocument) {
        setStatus("No file is open", false);
        return;
    }

    if (!saveDocument()) {
        return;
    }

    const std::string closed = m_filePath;
    m_closedDocuments.erase(std::remove_if(m_closedDocuments.begin(), m_closedDocuments.end(),
        [&closed](const OpenDocument& file) { return file.path == closed; }), m_closedDocuments.end());
    m_closedDocuments.insert(m_closedDocuments.begin(), OpenDocument{m_filePath, std::move(m_document), m_firstLine, m_diskContents});
    if (m_closedDocuments.size() > 20) m_closedDocuments.pop_back();
    closeCompileTerminal();
    m_hasDocument = false;
    m_filePath.clear();
    m_document = editor::Document{};
    m_firstLine = 0;
    m_followCursor = true;
    m_syntax.clear();
    m_visibleTokens.clear();
    m_syntaxValid = false;
    m_compileScript = editor::CompileScript{};
    m_compileScriptValid = false;
    resetDiagnostics();
    if (!m_inactiveDocuments.empty()) {
        std::vector<std::string> paths;
        paths.reserve(m_inactiveDocuments.size());
        for (const auto& document : m_inactiveDocuments) paths.push_back(document.path);
        std::size_t skipped = 0;
        for (const auto& path : paths) {
            if (!m_project.exists(path) || m_project.isDirectory(path)) { ++skipped; continue; }
            std::string content, error;
            if (!m_project.readFile(path, content, error) ||
                std::any_of(content.begin(), content.end(), [](unsigned char c) {
                    return c == 0 || (c < 0x20 && c != '\n' && c != '\r' && c != '\t' && c != '\f');
                }) || !editor::Document::canEditLosslessly(content, error)) { ++skipped; continue; }
            openFile(path, false);
            if (m_hasDocument) break;
            ++skipped;
        }
        if (skipped) {
            setStatus("Closed " + closed + "; " + std::to_string(skipped) +
                      " previously open file(s) unavailable", true);
        } else if (!m_hasDocument) {
            setStatus("Closed " + closed, false);
        }
    } else {
        setStatus("Closed " + closed, false);
    }
}

void Application::reopenClosedDocument() {
    if (!m_hasProject || m_closedDocuments.empty() || m_settingsPanelVisible || m_compileTerminalFocused) {
        setStatus("No recently closed file to reopen", false); return;
    }
    const auto path = m_closedDocuments.front().path;
    const bool alreadyOpen = (m_hasDocument && m_filePath == path) ||
        std::any_of(m_inactiveDocuments.begin(), m_inactiveDocuments.end(), [&path](const OpenDocument& file) { return file.path == path; });
    openFile(path, false);
    if (!m_hasDocument || m_filePath != path) { setStatus("Could not reopen " + path, true); return; }
    if (!alreadyOpen && m_diskContents == m_closedDocuments.front().diskContents &&
        m_document.toUtf8() == m_closedDocuments.front().document.toUtf8()) {
        m_document = std::move(m_closedDocuments.front().document);
        m_firstLine = m_closedDocuments.front().firstLine; m_followCursor = false;
    }
    m_closedDocuments.erase(m_closedDocuments.begin());
    setStatus("Reopened " + path, false);
}

void Application::moveSuggestion(int delta) {
    m_menuReselectionRequired = false;
    refreshSuggestions();
    if (m_suggestions.empty()) {
        m_selectedSuggestion = -1;
        return;
    }

    const int count = static_cast<int>(m_suggestions.size());
    const bool paged = isAdaptivePagedPrompt(m_searchBar.kind());
    const bool scrollable = paged || m_searchBar.kind() == ui::PromptKind::File || m_searchBar.kind() == ui::PromptKind::Project || m_searchBar.kind() == ui::PromptKind::OpenFiles || m_searchBar.kind() == ui::PromptKind::Function || m_searchBar.kind() == ui::PromptKind::SettingsMenu || m_searchBar.kind() == ui::PromptKind::AiSettings;
    if (scrollable && delta > 0 && m_selectedSuggestion == count - 1 && m_suggestionHasMoreBelow) {
        if (paged) ++m_pagedScroll; else ++m_suggestionScroll;
        refreshSuggestions();
        m_selectedSuggestion = static_cast<int>(m_suggestions.size()) - 1;
        return;
    }
    if (scrollable && delta < 0 && m_selectedSuggestion == 0 && (paged ? m_pagedScroll : m_suggestionScroll) > 0) {
        if (paged) --m_pagedScroll; else --m_suggestionScroll;
        refreshSuggestions();
        m_selectedSuggestion = 0;
        return;
    }
    if (delta > 0) {
        m_selectedSuggestion =
            m_selectedSuggestion < 0 ? 0 : std::min(m_selectedSuggestion + delta, count - 1);
    } else {
        m_selectedSuggestion = std::max(m_selectedSuggestion + delta, -1);
    }
}

void Application::acceptSuggestion() {
    refreshSuggestions();
    if (m_suggestions.empty()) {
        return;
    }

    if (m_selectedSuggestion < 0 ||
        m_selectedSuggestion >= static_cast<int>(m_suggestions.size())) {
        m_selectedSuggestion = 0;
    }

    m_searchBar.setQuery(std::string(m_suggestions[static_cast<std::size_t>(
        m_selectedSuggestion)]
                                         .path));
    m_suggestionScroll = 0;
    m_selectedSuggestion = -1;
    m_suggestionEpoch = m_lastTimeSeconds;
    m_suggestionSelectionRow = -1.0F;
    m_suggestionSelectionOpacity = 0.0F;
    m_suggestionSelectionRowVelocity = 0.0F;
    m_suggestionSelectionOpacityVelocity = 0.0F;
    m_commandStatus.clear();
    m_commandStatusIsError = false;
    if (m_searchBar.kind() == ui::PromptKind::ProjectSearch)
        setCommandStatus(m_projectSearchWholeWord ? "Whole words on; Ctrl/Command+Up/Down: history" : "Whole words off; Ctrl/Command+Up/Down: history", false);
    refreshSuggestions();
}

void Application::refreshSuggestions() {
    m_suggestions.clear();
    m_suggestionPaths.clear();
    m_suggestionDetails.clear();
    m_suggestionFunctionIndices.clear();
    m_suggestionHasMoreBelow = false;

    if (!m_searchBar.isOpen()) {
        m_selectedSuggestion = -1;
        m_pagedLayoutValid = false;
        return;
    }
    if (isAdaptivePagedPrompt(m_searchBar.kind())) {
        const auto currentCapacity = pagedContentCount();
        if (!m_pagedLayoutValid || m_pagedLayoutKind != m_searchBar.kind()) {
            m_pagedScroll = 0;
            m_menuReselectionRequired = false;
        }
        if (m_pagedLayoutValid && m_pagedLayoutKind == m_searchBar.kind() &&
            currentCapacity != m_pagedLayoutCapacity) { m_selectedSuggestion = -1; m_pagedScroll = 0; }
        m_pagedLayoutKind = m_searchBar.kind();
        m_pagedLayoutCapacity = currentCapacity;
        m_pagedLayoutValid = true;
    } else m_pagedLayoutValid = false;
    if (m_searchBar.kind() == ui::PromptKind::File && m_projectIndexFuture.valid()) { m_selectedSuggestion = -1; return; }

    const bool scrollablePaths = m_searchBar.kind() == ui::PromptKind::File || m_searchBar.kind() == ui::PromptKind::Project || m_searchBar.kind() == ui::PromptKind::OpenFiles || m_searchBar.kind() == ui::PromptKind::Function;
    const std::size_t visibleCapacity = scrollablePaths ? visibleSuggestionCount() : static_cast<std::size_t>(ui::theme::kMaxSuggestions);
    const std::size_t capacity = visibleCapacity + (scrollablePaths ? m_suggestionScroll + 1 : 0);
    m_suggestionPaths.reserve(capacity);
    m_suggestionDetails.reserve(capacity);
    m_suggestionFunctionIndices.reserve(capacity);

    const auto addDetailedSuggestion = [this](std::string path,
                                              ui::SuggestionKind kind,
                                              std::string detail) {
        m_suggestionPaths.push_back(std::move(path));
        m_suggestionDetails.push_back(std::move(detail));
        m_suggestions.push_back(ui::PathSuggestion{
            m_suggestionPaths.back(), kind, m_suggestionDetails.back()});
    };
    const auto addSuggestion = [&addDetailedSuggestion](
                                   std::string path, ui::SuggestionKind kind) {
        addDetailedSuggestion(std::move(path), kind, {});
    };
    const auto remaining = [this, capacity]() {
        return m_suggestions.size() < capacity ? capacity - m_suggestions.size() : 0;
    };
    const auto clipSettings = [this]() {
        const auto visible = visibleSuggestionCount();
        const auto maximumScroll = m_suggestions.size() > visible ? m_suggestions.size() - visible : 0;
        m_suggestionScroll = std::min(m_suggestionScroll, maximumScroll);
        m_suggestionHasMoreBelow = m_suggestionScroll < maximumScroll;
        m_suggestions.erase(m_suggestions.begin(), m_suggestions.begin() + static_cast<std::ptrdiff_t>(m_suggestionScroll));
        if (m_suggestions.size() > visible) m_suggestions.resize(visible);
        if (m_selectedSuggestion >= static_cast<int>(m_suggestions.size())) m_selectedSuggestion = -1;
    };
    const auto clipPaged = [this](auto& choices) {
        const auto visible = visibleSuggestionCount();
        const auto maximum = m_suggestions.size() > visible ? m_suggestions.size() - visible : 0;
        m_pagedScroll = std::min(m_pagedScroll, maximum);
        m_suggestionHasMoreBelow = m_pagedScroll < maximum;
        const auto offset = static_cast<std::ptrdiff_t>(m_pagedScroll);
        m_suggestions.erase(m_suggestions.begin(), m_suggestions.begin() + offset);
        choices.erase(choices.begin(), choices.begin() + offset);
        if (m_suggestions.size() > visible) m_suggestions.resize(visible);
        if (choices.size() > visible) choices.resize(visible);
        if (m_selectedSuggestion >= static_cast<int>(m_suggestions.size())) m_selectedSuggestion = -1;
    };

    if (m_searchBar.kind() == ui::PromptKind::SessionRestore) {
        if (m_pendingSession) {
            auto projectName = std::filesystem::path(m_pendingSession->project).filename().string();
            if (projectName.empty()) projectName = m_pendingSession->project;
            const auto count = m_pendingSession->files.size();
            addDetailedSuggestion("Restore " + projectName, ui::SuggestionKind::Task,
                                  std::to_string(count) + (count == 1 ? " open file" : " open files"));
            addDetailedSuggestion("Start fresh", ui::SuggestionKind::Task, "Open no project");
            addDetailedSuggestion(m_rememberSessionChoice ? "[x] Don't show again" : "[ ] Don't show again",
                                  ui::SuggestionKind::Task, "Remember the next choice");
        }
        std::vector<int> choices(m_suggestions.size());
        clipPaged(choices);
        return;
    }

    if (m_searchBar.kind() == ui::PromptKind::Diagnostics) {
        m_diagnosticListChoices.clear();
        std::vector<std::size_t> visible;
        if (m_hasDocument && m_diagnosticsValid && m_diagnosticsRevision == m_document.revision()) {
            for (std::size_t i = 0; i < m_diagnostics.size(); ++i)
                if (m_diagnostics[i].line < m_document.lineCount() && !m_ignoredDiagnostics.contains(m_diagnostics[i].key())) visible.push_back(i);
        }
        constexpr std::size_t pageSize = 6;
        const auto pages = std::max<std::size_t>(1, (visible.size() + pageSize - 1) / pageSize);
        m_diagnosticListPage = std::min(m_diagnosticListPage, pages - 1);
        const auto add = [this, &addDetailedSuggestion](std::string label, std::string detail, int choice) {
            addDetailedSuggestion(std::move(label), ui::SuggestionKind::Task, std::move(detail)); m_diagnosticListChoices.push_back(choice);
        };
        if (m_diagnosticListPage) add("< Previous page", "", -1);
        const auto begin = m_diagnosticListPage * pageSize;
        for (std::size_t i = begin; i < std::min(begin + pageSize, visible.size()); ++i) {
            const auto& diagnostic = m_diagnostics[visible[i]];
            add("Line " + std::to_string(diagnostic.line + 1) + ":" + std::to_string(diagnostic.column), diagnostic.message, static_cast<int>(visible[i]));
        }
        if (m_diagnosticListPage + 1 < pages) add("Next page >", "", -2);
        clipPaged(m_diagnosticListChoices); return;
    }
    if (m_searchBar.kind() == ui::PromptKind::LanguageLocations) {
        m_languageLocationChoices.clear();
        const auto add = [this, &addDetailedSuggestion](std::string label, int choice) { addDetailedSuggestion(std::move(label), ui::SuggestionKind::Task, ""); m_languageLocationChoices.push_back(choice); };
        constexpr std::size_t pageSize = 7;
        const auto pages = std::max<std::size_t>(1, (m_languageLocations.size() + pageSize - 1) / pageSize);
        m_languageLocationPage = std::min(m_languageLocationPage, pages - 1);
        if (m_languageLocationPage) add("< Previous page", -1);
        const auto begin = m_languageLocationPage * pageSize;
        for (std::size_t i = begin; i < std::min(begin + pageSize, m_languageLocations.size()); ++i) {
            const auto& loc = m_languageLocations[i]; add(loc.path + ":" + std::to_string(loc.line + 1) + ":" + std::to_string(loc.character + 1), static_cast<int>(i));
        }
        if (m_languageLocationPage + 1 < pages) add("Next page >", -2);
        if (m_selectedSuggestion >= static_cast<int>(m_languageLocationChoices.size())) m_selectedSuggestion = -1;
        clipPaged(m_languageLocationChoices);
        return;
    }

    if (m_searchBar.kind() == ui::PromptKind::ProjectSearchResults) {
        m_projectSearchChoices.clear();
        const auto add = [this, &addDetailedSuggestion](std::string label, std::string detail, int choice) {
            addDetailedSuggestion(std::move(label), ui::SuggestionKind::Task, std::move(detail)); m_projectSearchChoices.push_back(choice);
        };
        constexpr std::size_t pageSize = 6;
        const auto pages = std::max<std::size_t>(1, (m_projectSearchResults.matches.size() + pageSize - 1) / pageSize);
        m_projectSearchPage = std::min(m_projectSearchPage, pages - 1);
        if (m_projectSearchPage) add("< Previous page", "", -1);
        const auto begin = m_projectSearchPage * pageSize;
        for (std::size_t i = begin; i < std::min(begin + pageSize, m_projectSearchResults.matches.size()); ++i) {
            const auto& match = m_projectSearchResults.matches[i];
            const auto& file = m_projectSearchResults.files[match.file];
            add(file.path + ":" + std::to_string(match.line + 1) + ":" + std::to_string(match.column + 1), match.preview, static_cast<int>(i));
        }
        if (m_projectSearchPage + 1 < pages) add("Next page >", "", -2);
        if (!m_projectSearchResults.matches.empty()) add("Replace all matches…", "Review before applying", -3);
        if (m_selectedSuggestion >= static_cast<int>(m_suggestions.size())) m_selectedSuggestion = -1;
        clipPaged(m_projectSearchChoices);
        return;
    }
    if (m_searchBar.kind() == ui::PromptKind::ProjectReplaceReview) {
        m_projectSearchChoices.clear();
        const auto add = [this, &addDetailedSuggestion](std::string name, ui::SuggestionKind type, std::string detail, int choice) {
            addDetailedSuggestion(std::move(name), type, std::move(detail)); m_projectSearchChoices.push_back(choice);
        };
        if (m_projectReviewFile && *m_projectReviewFile < m_projectSearchResults.files.size()) {
            const auto file = *m_projectReviewFile;
            std::vector<std::size_t> matches;
            for (std::size_t i = 0; i < m_projectSearchResults.matches.size(); ++i)
                if (m_projectSearchResults.matches[i].file == file) matches.push_back(i);
            constexpr std::size_t matchPageSize = 3;
            const auto pages = std::max<std::size_t>(1, (matches.size() + matchPageSize - 1) / matchPageSize);
            m_projectSearchPage = std::min(m_projectSearchPage, pages - 1);
            add("< Back to files", ui::SuggestionKind::Task, "", -5);
            if (m_projectSearchPage) add("< Previous page", ui::SuggestionKind::Task, "", -1);
            const auto begin = m_projectSearchPage * matchPageSize;
            for (std::size_t i = begin; i < std::min(begin + matchPageSize, matches.size()); ++i) {
                const auto index = matches[i]; const auto& match = m_projectSearchResults.matches[index];
                const bool included = m_projectSearchSelected.size() != m_projectSearchResults.matches.size() || m_projectSearchSelected[index];
                add(std::string(included ? "[x] " : "[ ] ") + "Before " + std::to_string(match.line + 1) + ":" + std::to_string(match.column + 1),
                    ui::SuggestionKind::Task, match.preview, -100 - static_cast<int>(index));
                add("After this match", ui::SuggestionKind::ReadOnlyText,
                    workspace::replacementPreview(m_projectSearchResults.files[file], match, m_projectSearchQuery, m_projectReplacement), 0);
            }
            if (m_projectSearchPage + 1 < pages) add("Next page >", ui::SuggestionKind::Task, "", -2);
            clipPaged(m_projectSearchChoices); return;
        }
        const auto selectedCount = m_projectSearchSelected.size() == m_projectSearchResults.matches.size() ?
            static_cast<std::size_t>(std::count(m_projectSearchSelected.begin(), m_projectSearchSelected.end(), true)) : m_projectSearchResults.matches.size();
        add("Apply " + std::to_string(selectedCount) + " selected replacements", ui::SuggestionKind::Task, "Cannot undo as one operation", -4);
        add("Find: " + m_projectSearchQuery, ui::SuggestionKind::ReadOnlyText, "", 0);
        add("Replace: " + (m_projectReplacement.empty() ? std::string("<empty>") : m_projectReplacement), ui::SuggestionKind::ReadOnlyText, "", 0);
        constexpr std::size_t pageSize = 4;
        const auto pages = std::max<std::size_t>(1, (m_projectSearchResults.files.size() + pageSize - 1) / pageSize);
        m_projectSearchPage = std::min(m_projectSearchPage, pages - 1);
        if (m_projectSearchPage) add("< Previous page", ui::SuggestionKind::Task, "", -1);
        const auto begin = m_projectSearchPage * pageSize;
        for (std::size_t i = begin; i < std::min(begin + pageSize, m_projectSearchResults.files.size()); ++i) {
            std::size_t count = 0, included = 0; std::string preview;
            for (std::size_t matchIndex = 0; matchIndex < m_projectSearchResults.matches.size(); ++matchIndex) {
                const auto& match = m_projectSearchResults.matches[matchIndex];
                if (match.file != i) continue;
                ++count;
                if (m_projectSearchSelected.size() != m_projectSearchResults.matches.size() || m_projectSearchSelected[matchIndex]) ++included;
                if (preview.empty()) preview = match.preview;
            }
            const std::string detail = std::to_string(included) + "/" + std::to_string(count) + " selected; first: " + preview;
            add(m_projectSearchResults.files[i].path, ui::SuggestionKind::Task, detail, static_cast<int>(i + 1));
        }
        if (m_projectSearchPage + 1 < pages) add("Next page >", ui::SuggestionKind::Task, "", -2);
        clipPaged(m_projectSearchChoices);
        return;
    }
    if (m_searchBar.kind() == ui::PromptKind::ProjectSearch || m_searchBar.kind() == ui::PromptKind::ProjectReplace) return;
    if (m_searchBar.kind() == ui::PromptKind::SettingsMenu) {
        if (m_settingsMenuPage == 8) {
            m_backgroundSceneChoices.clear();
            const auto add = [this, &addDetailedSuggestion](std::string label, std::string detail, int choice) {
                addDetailedSuggestion(std::move(label), ui::SuggestionKind::Task, std::move(detail));
                m_backgroundSceneChoices.push_back(choice);
            };
            constexpr std::size_t pageSize = 6;
            const auto pages = (backgroundChoices.size() + pageSize - 1) / pageSize;
            m_backgroundScenePage = std::min(m_backgroundScenePage, pages - 1);
            if (m_backgroundScenePage) add("< Previous page", "", -1);
            const auto begin = m_backgroundScenePage * pageSize;
            for (std::size_t index = begin; index < std::min(begin + pageSize, backgroundChoices.size()); ++index) {
                const int choice = backgroundChoices[index];
                add(backgroundNames[static_cast<std::size_t>(choice)], choice == m_settings.backgroundScene ? "Current" : "Enter to select", choice);
            }
            if (m_backgroundScenePage + 1 < pages) add("Next page >", "", -2);
        } else if (m_settingsMenuPage == 0) {
            for (const char* name : {"Theme", "Primary color", "Text size", "Cursor", "App icon", "Editor", "Animation", "AI settings", "Open settings file"})
                addDetailedSuggestion(name, ui::SuggestionKind::Task, "Enter to open");
        } else {
            for (const auto& [name, control] : settingsChoices(m_settingsMenuPage)) {
                using C = ui::SettingsControl;
                std::string detail;
                if (control == C::ShowSyntaxErrors) detail = m_settings.showSyntaxErrors ? "On" : "Off";
                else if (control == C::ShowLineNumbers) detail = m_settings.showLineNumbers ? "On" : "Off";
                else if (control == C::HighlightCurrentLine) detail = m_settings.highlightCurrentLine ? "On" : "Off";
                else if (control == C::HighContrast) detail = m_settings.highContrast ? "On" : "Off";
                else if (control == C::CursorBlink) detail = m_settings.cursorBlink ? "On" : "Off";
                else if (control == C::BackgroundMotion) detail = m_settings.backgroundMotion ? "On" : "Off";
                else if (control == C::BackgroundColorMode) detail = m_settings.backgroundColorMode == "natural" ? "Scene colors" : "Primary color";
                else if (control == C::StartupSession) detail = m_settings.sessionRestore == "ask" ? "Ask every time" : m_settings.sessionRestore == "restore" ? "Always restore" : "Always start fresh";
                else if (control == C::BackgroundScene) detail = std::string(backgroundNames[static_cast<std::size_t>(m_settings.backgroundScene)]) + " — Enter to change";
                else {
                    bool current = false;
                    if (control == C::ThemeLucidDark) current = m_settings.theme.name == "lucid-dark";
                    else if (control == C::ThemeGraphite) current = m_settings.theme.name == "graphite";
                    else if (control == C::ThemeLucidLight) current = m_settings.theme.name == "lucid-light";
                    else if (control == C::IconClassic) current = m_settings.iconTheme == "classic";
                    else if (control == C::IconAccent) current = m_settings.iconTheme == "accent";
                    else if (control == C::IconOutline) current = m_settings.iconTheme == "outline";
                    else if (control == C::Zoom75) current = m_settings.zoomFactor == 0.75F;
                    else if (control == C::Zoom100) current = m_settings.zoomFactor == 1.0F;
                    else if (control == C::Zoom125) current = m_settings.zoomFactor == 1.25F;
                    else if (control == C::Zoom150) current = m_settings.zoomFactor == 1.5F;
                    else if (control == C::SpeedHalf) current = m_settings.animationSpeed == 0.5F;
                    else if (control == C::SpeedNormal) current = m_settings.animationSpeed == 1.0F;
                    else if (control == C::SpeedDouble) current = m_settings.animationSpeed == 2.0F;
                    else if (control == C::CursorLine) current = m_settings.cursorStyle == editor::CursorStyle::Line;
                    else if (control == C::CursorBlock) current = m_settings.cursorStyle == editor::CursorStyle::Block;
                    else if (control == C::CursorUnderscore) current = m_settings.cursorStyle == editor::CursorStyle::Underscore;
                    else if (control == C::CursorKitty) current = m_settings.cursorStyle == editor::CursorStyle::Kitty;
                    else if (control >= C::PrimaryColor0 && control <= C::PrimaryColor7 && m_settings.theme.primary) {
                        const auto index = static_cast<std::size_t>(control) - static_cast<std::size_t>(C::PrimaryColor0);
                        const auto& color = ui::theme::kPrimarySwatches[index]; const auto& selected = *m_settings.theme.primary;
                        current = color.r == selected.r && color.g == selected.g && color.b == selected.b;
                    }
                    if (current) detail = "Current";
                }
                addDetailedSuggestion(name, ui::SuggestionKind::Task, std::move(detail));
            }
        }
        clipSettings();
        return;
    }
    if (m_searchBar.kind() == ui::PromptKind::AiSettings) {
        const std::array<std::string, 3> names{"Ask AI", "Code cleanup", "Debugger"};
        const std::array<std::string, 3> providers{m_settings.aiAskProvider, m_settings.aiCleanupProvider, m_settings.aiDebugProvider};
        const std::array<std::string, 3> models{m_settings.aiAskModel, m_settings.aiCleanupModel, m_settings.aiDebugModel};
        for (std::size_t i = 0; i < names.size(); ++i) {
            addDetailedSuggestion(names[i] + ": " + (providers[i] == "local" ? "Local" : "OpenRouter"), ui::SuggestionKind::Task, "Enter toggles provider");
            addDetailedSuggestion(names[i] + " OpenRouter model", ui::SuggestionKind::Task, models[i].empty() ? "Not set" : models[i]);
        }
        addDetailedSuggestion("Set or change OpenRouter API key", ui::SuggestionKind::Task, "Stored in system credentials");
        addDetailedSuggestion("Remove OpenRouter API key", ui::SuggestionKind::Task, "Deletes saved key");
        addDetailedSuggestion("Download recommended local model", ui::SuggestionKind::Task, "Qwen2.5 Coder 0.5B, 491 MB; used by local workflows");
        clipSettings();
        return;
    }
    if (m_searchBar.kind() == ui::PromptKind::AiKeyInput || m_searchBar.kind() == ui::PromptKind::AiModelInput) return;
    if (m_searchBar.kind() == ui::PromptKind::DebugMenu || m_searchBar.kind() == ui::PromptKind::DebugInput || m_searchBar.kind() == ui::PromptKind::DebugStack || m_searchBar.kind() == ui::PromptKind::DebugThreads || m_searchBar.kind() == ui::PromptKind::DebugVariables || m_searchBar.kind() == ui::PromptKind::DebugOutput) { refreshDebugSuggestions(); clipPaged(m_debugChoices); return; }
    if (m_searchBar.kind() == ui::PromptKind::RecoveryList || m_searchBar.kind() == ui::PromptKind::RecoveryDetail) { refreshRecoverySuggestions(); clipPaged(m_recoveryChoices); return; }
    if (m_searchBar.kind() == ui::PromptKind::ModuleList || m_searchBar.kind() == ui::PromptKind::ModuleActions || m_searchBar.kind() == ui::PromptKind::ModuleCommands || m_searchBar.kind() == ui::PromptKind::ModulePreview || m_searchBar.kind() == ui::PromptKind::ModuleCatalog) { refreshModuleSuggestions(); clipPaged(m_moduleChoices); return; }
    if (m_searchBar.kind() == ui::PromptKind::GitChanges || m_searchBar.kind() == ui::PromptKind::GitFile || m_searchBar.kind() == ui::PromptKind::GitDiff || m_searchBar.kind() == ui::PromptKind::GitCommitReview || m_searchBar.kind() == ui::PromptKind::GitCommitInput || m_searchBar.kind() == ui::PromptKind::GitRemoteReview || m_searchBar.kind() == ui::PromptKind::GitHistory) { refreshGitSuggestions(); clipPaged(m_gitChoices); return; }
    if (isTaskPrompt(m_searchBar.kind())) {
        refreshTaskSuggestions();
        clipPaged(m_taskChoices);
        return;
    }
    if (m_searchBar.kind() == ui::PromptKind::OpenFiles) {
        std::vector<std::string> paths;
        if (m_hasDocument) paths.push_back(m_filePath);
        for (const auto& file : m_inactiveDocuments) paths.push_back(file.path);
        const auto detailFor = [this](std::string_view path) {
            if (m_hasDocument && path == m_filePath) return std::string(m_document.dirty() ? "Current · Unsaved" : "Current");
            const auto found = std::find_if(m_inactiveDocuments.begin(), m_inactiveDocuments.end(),
                [path](const OpenDocument& file) { return file.path == path; });
            return std::string(found != m_inactiveDocuments.end() && found->document.dirty() ? "Open · Unsaved" : "Open");
        };
        if (m_searchBar.query().empty()) {
            for (const auto& path : paths) {
                if (m_suggestions.size() == capacity) break;
                addDetailedSuggestion(path, ui::SuggestionKind::File, detailFor(path));
            }
        } else {
            for (const auto path : workspace::filterPaths(paths, m_searchBar.query(), capacity))
                addDetailedSuggestion(std::string(path), ui::SuggestionKind::File, detailFor(path));
        }
        const auto maxOffset = m_suggestions.size() > visibleCapacity ? m_suggestions.size() - visibleCapacity : 0;
        m_suggestionScroll = std::min(m_suggestionScroll, maxOffset);
        m_suggestionHasMoreBelow = m_suggestions.size() > m_suggestionScroll + visibleCapacity;
        m_suggestions.erase(m_suggestions.begin(), m_suggestions.begin() + static_cast<std::ptrdiff_t>(m_suggestionScroll));
        if (m_suggestions.size() > visibleCapacity) m_suggestions.resize(visibleCapacity);
        if (m_selectedSuggestion >= static_cast<int>(m_suggestions.size())) m_selectedSuggestion = -1;
        return;
    }

    if (m_searchBar.kind() == ui::PromptKind::Find || m_searchBar.kind() == ui::PromptKind::Replace) return;
    if (m_searchBar.kind() == ui::PromptKind::Ai) {
        if (m_aiReady) {
            int width = 0, height = 0;
            uiSize(width, height);
            const float available = std::max(0.0F, std::min(static_cast<float>(width) - ui::theme::kSearchBarMargin * 2.0F, 520.0F) -
                2.0F * (ui::theme::kSuggestionInset + ui::theme::kSuggestionPaddingX));
            const float advance = m_charAdvance > 0.0F ? m_charAdvance : 7.0F;
            const auto columns = static_cast<std::size_t>(std::clamp(static_cast<int>(std::floor(available / advance)), 1, 65));
            if (!m_aiResponse.empty() && columns != m_aiWrapColumns) {
                const auto anchor = m_aiWrapColumns != 0 && m_aiScroll < m_aiLineOffsets.size()
                    ? m_aiLineOffsets[m_aiScroll] : 0;
                m_aiLines = text::wrapLines(m_aiResponse, columns, &m_aiLineOffsets);
                const auto position = std::upper_bound(m_aiLineOffsets.begin(), m_aiLineOffsets.end(), anchor);
                m_aiScroll = position == m_aiLineOffsets.begin() ? 0 : static_cast<std::size_t>(position - m_aiLineOffsets.begin() - 1);
                m_aiWrapColumns = columns;
            }
            const auto visible = visibleSuggestionCount();
            const auto maximum = m_aiLines.size() > visible ? m_aiLines.size() - visible : 0;
            m_aiScroll = std::min(m_aiScroll, maximum);
            for (std::size_t i = m_aiScroll; i < m_aiLines.size() && m_suggestions.size() < visible; ++i)
                addSuggestion(m_aiLines[i], ui::SuggestionKind::ReadOnlyText);
            m_selectedSuggestion = -1;
        }
        return;
    }

    if (m_searchBar.kind() == ui::PromptKind::Function) {
        if (!m_hasProject) {
            m_selectedSuggestion = -1;
            return;
        }
        ensureFunctionIndex();
        if (!m_functionIndexValid || m_functionIndexDirty) { m_selectedSuggestion = -1; return; }
        const std::vector<std::size_t> matches = workspace::searchFunctions(
            m_functionIndex.functions, m_searchBar.query(), capacity);
        for (const std::size_t index : matches) {
            if (index >= m_functionIndex.functions.size()) {
                continue;
            }
            const workspace::ProjectFunction& function =
                m_functionIndex.functions[index];
            std::string detail = function.file + ":" +
                                 std::to_string(function.line + 1);
            const std::string_view platform =
                workspace::functionPlatformName(function.platform);
            if (!platform.empty()) {
                detail += "  ";
                detail += platform;
            }
            addDetailedSuggestion(function.name, ui::SuggestionKind::Function,
                                  std::move(detail));
            m_suggestionFunctionIndices.push_back(index);
        }
    } else if (m_searchBar.kind() == ui::PromptKind::Project) {
        const std::string rawQuery(m_searchBar.query());

        if (rawQuery.empty()) {
            for (const std::string& recent : m_recentProjects) {
                if (m_suggestions.size() >= 4) {
                    break;
                }
                addSuggestion(recent + "/", ui::SuggestionKind::Project);
            }
        } else {
            std::error_code error;
            const bool exists =
                std::filesystem::exists(resolveProjectPath(rawQuery), error) && !error;
            if (!exists) {
                addSuggestion(rawQuery, ui::SuggestionKind::CreateDirectory);
            }
        }

        const std::filesystem::path projects = projectsDirectory();
        const std::filesystem::path fallback =
            projects.empty() ? homeDirectory() : projects;
        for (std::string suggestion :
             suggestDirectories(rawQuery, fallback, fallback, remaining(), false)) {
            addSuggestion(std::move(suggestion), ui::SuggestionKind::Project);
        }
    } else {
        if (!m_hasProject) {
            m_selectedSuggestion = -1;
            return;
        }

        const std::string typed = normalizeRelative(m_searchBar.query());

        if (!typed.empty()) {
            if (typed.back() == '/') {
                if (!m_project.isDirectory(typed)) {
                    addSuggestion(typed, ui::SuggestionKind::CreateDirectory);
                }
            } else if (!m_project.exists(typed) && !m_project.isDirectory(typed)) {
                const bool awaitingRestore = m_project.root().generic_string() == m_unrestoredProject &&
                    std::any_of(m_unrestoredFiles.begin(), m_unrestoredFiles.end(),
                                [&typed](const auto& file) { return file.path == typed; });
                if (!awaitingRestore) {
                    addSuggestion(typed, ui::SuggestionKind::CreateFile);
                    if (remaining() > 0) addSuggestion(typed + "/", ui::SuggestionKind::CreateDirectory);
                }
            }
        }

        const std::size_t directoryBudget = remaining();
        for (const std::string_view match :
             workspace::filterChildPaths(m_project.directories(), typed,
                                    directoryBudget)) {
            std::string path(match);
            if (path.empty()) {
                continue;
            }
            if (path.back() != '/') {
                path.push_back('/');
            }
            if (path == typed) {
                continue;
            }
            addSuggestion(std::move(path), ui::SuggestionKind::Directory);
        }

        for (const std::string_view match :
             workspace::filterChildPaths(m_project.files(), typed, remaining())) {
            addSuggestion(std::string(match), ui::SuggestionKind::File);
        }
    }

    if (scrollablePaths) {
        const std::size_t maxOffset = m_suggestions.size() > visibleCapacity ? m_suggestions.size() - visibleCapacity : 0;
        const std::size_t offset = std::min(m_suggestionScroll, maxOffset);
        m_suggestionScroll = offset;
        m_suggestionHasMoreBelow = m_suggestions.size() > offset + visibleCapacity;
        m_suggestions.erase(m_suggestions.begin(), m_suggestions.begin() + static_cast<std::ptrdiff_t>(offset));
        if (m_suggestions.size() > visibleCapacity) m_suggestions.resize(visibleCapacity);
        if (m_searchBar.kind() == ui::PromptKind::Function) {
            m_suggestionFunctionIndices.erase(m_suggestionFunctionIndices.begin(), m_suggestionFunctionIndices.begin() + static_cast<std::ptrdiff_t>(offset));
            if (m_suggestionFunctionIndices.size() > visibleCapacity) m_suggestionFunctionIndices.resize(visibleCapacity);
        }
    }
    if (m_selectedSuggestion >= static_cast<int>(m_suggestions.size())) {
        m_selectedSuggestion = -1;
    }
}

void Application::onCommandQueryChanged() {
    m_menuReselectionRequired = false;
    m_menuScrollRemainder = 0.0;
    m_suggestionScroll = 0;
    m_pagedScroll = 0;
    if (m_searchBar.kind() == ui::PromptKind::TaskList) m_taskPage = 0;
    if (m_searchBar.kind() == ui::PromptKind::ModuleCatalog) m_modulePage = 0;
    if (m_searchBar.kind() == ui::PromptKind::ProjectSearch) {
        m_projectSearchHistoryPosition = m_projectSearchHistory.size(); m_projectSearchDraft = m_searchBar.query();
    }
    m_selectedSuggestion = -1;
    m_suggestionEpoch = m_lastTimeSeconds;
    m_suggestionSelectionRow = -1.0F;
    m_suggestionSelectionOpacity = 0.0F;
    m_suggestionSelectionRowVelocity = 0.0F;
    m_suggestionSelectionOpacityVelocity = 0.0F;
    m_commandStatus.clear();
    m_commandStatusIsError = false;
    if (m_searchBar.kind() == ui::PromptKind::File && m_projectIndexFuture.valid()) setCommandStatus("Indexing project files…", false);
    if (m_searchBar.kind() == ui::PromptKind::Find) findInDocument(false);
    else if (m_searchBar.kind() == ui::PromptKind::Replace)
        setCommandStatus("Enter: replace this match  Escape: cancel", false);
    refreshSuggestions();
}

void Application::clearCommandFeedback() {
    m_menuReselectionRequired = false;
    m_menuScrollRemainder = 0.0;
    m_suggestionScroll = 0;
    m_pagedScroll = 0;
    m_suggestionHasMoreBelow = false;
    m_selectedSuggestion = -1;
    m_suggestionEpoch = m_lastTimeSeconds;
    m_suggestionSelectionRow = -1.0F;
    m_suggestionSelectionOpacity = 0.0F;
    m_suggestionSelectionRowVelocity = 0.0F;
    m_suggestionSelectionOpacityVelocity = 0.0F;
    m_suggestions.clear();
    m_suggestionPaths.clear();
    m_suggestionDetails.clear();
    m_suggestionFunctionIndices.clear();
    m_commandStatus.clear();
    m_commandStatusIsError = false;
}

void Application::refreshHighlight() {
    if (!m_hasDocument) {
        m_syntax.clear();
        m_syntaxValid = false;
        return;
    }

    if (m_syntaxValid && m_syntaxRevision == m_document.revision()) {
        return;
    }

    m_syntax.clear();
    m_syntax.reserve(m_document.lineCount());

    if (isJsonDocument()) {
        for (const std::u32string& line : m_document.lines()) {
            m_syntax.push_back(editor::highlightJsonLine(line));
        }
    } else {
        editor::CppState state;
        for (const std::u32string& line : m_document.lines()) {
            m_syntax.push_back(editor::highlightCppLine(line, state));
        }
    }

    m_syntaxRevision = m_document.revision();
    m_syntaxValid = true;
}

void Application::refreshCompileScript() {
    if (!m_hasDocument || !isCodeDocument()) {
        m_compileScript = editor::CompileScript{};
        m_compileScriptValid = false;
        return;
    }

    const std::uint64_t revision = m_document.revision();
    if (m_compileScriptValid && m_compileScriptRevision == revision) {
        return;
    }

    m_compileScript = editor::parseCompileScript(m_document.lines());
    m_compileScriptRevision = revision;
    m_compileScriptValid = true;
}

float Application::textWidth(const std::u32string& line, std::size_t column) const {
    const float advance = m_charAdvance > 0.0F ? m_charAdvance : 7.0F;
    const std::size_t limit = std::min(column, line.size());
    float width = 0.0F;
    for (std::size_t index = 0; index < limit; ++index) {
        width += line[index] == U'\t' ? advance * 4.0F : advance;
    }
    return width;
}

float Application::diagnosticAnchorX() const {
    const auto column = static_cast<std::size_t>(std::max(0, m_activeDiagnosticColumn - 1));
    if (m_hasDocument && m_activeDiagnosticLine >= 0 &&
        static_cast<std::size_t>(m_activeDiagnosticLine) < m_document.lineCount()) {
        return ui::theme::kEditorCodeLeft + textWidth(m_document.line(static_cast<std::size_t>(m_activeDiagnosticLine)), column);
    }
    const float advance = m_charAdvance > 0.0F ? m_charAdvance : 7.0F;
    return ui::theme::kEditorCodeLeft + static_cast<float>(column) * advance;
}

ui::CompileTerminalLayout Application::compileTerminalBaseLayout() const {
    int width = 0;
    int height = 0;
    uiSize(width, height);

    const int row = m_compileTerminalLine - m_firstLine;
    const int rows = visibleRowCount();
    const int clampedRow = std::clamp(row, 0, std::max(0, rows - 1));
    const float anchorTop =
        ui::theme::kEditorTop +
        static_cast<float>(clampedRow) * ui::theme::kEditorLineHeight;
    return ui::compileTerminalLayout(static_cast<float>(width),
                                     static_cast<float>(height), anchorTop);
}

ui::CompileTerminalLayout Application::compileTerminalLayout() const {
    return ui::offsetCompileTerminal(compileTerminalBaseLayout(),
                                     m_compileTerminalOffsetX,
                                     m_compileTerminalOffsetY);
}

const editor::CompileFunction* Application::compileFunctionAt(double x,
                                                              double y) const {
    if (!m_hasDocument || !m_compileScriptValid || m_searchBar.isOpen()) {
        return nullptr;
    }

    int width [[maybe_unused]] = 0;
    int height = 0;
    uiSize(width, height);

    const float top = ui::theme::kEditorTop;
    const float bottom = static_cast<float>(height) - ui::theme::kEditorBottom;
    if (y < top || y >= bottom) {
        return nullptr;
    }

    const int row = static_cast<int>((y - top) / ui::theme::kEditorLineHeight);
    const int line = m_firstLine + row;
    const editor::CompileFunction* function =
        editor::findCompileFunctionAt(m_compileScript, line);
    if (function == nullptr || function->definitionLine != line) {
        return nullptr;
    }

    const std::u32string& text =
        m_document.line(static_cast<std::size_t>(line));
    const float x0 = ui::theme::kEditorCodeLeft +
                     textWidth(text, static_cast<std::size_t>(
                                         std::max(0, function->directiveColumn)));
    const float x1 = ui::theme::kEditorCodeLeft +
                     textWidth(text, static_cast<std::size_t>(
                                         std::max(0, function->directiveColumn) +
                                         std::max(1, function->directiveLength)));
    return x >= x0 - 2.0 && x <= x1 + 2.0 ? function : nullptr;
}

void Application::beginDrag(double x, double y, bool terminal) {
    m_dragTarget = terminal ? DragTarget::CompileTerminal : DragTarget::DiagnosticCard;
    m_dragStartX = x;
    m_dragStartY = y;
    if (terminal) {
        focusCompileTerminal(true);
        m_dragStartOffsetX = m_compileTerminalOffsetX;
        m_dragStartOffsetY = m_compileTerminalOffsetY;
    } else {
        m_dragStartOffsetX = m_diagnosticOffsetX;
        m_dragStartOffsetY = m_diagnosticOffsetY;
    }
}

void Application::updateDrag(double x, double y) {
    int width = 0;
    int height = 0;
    uiSize(width, height);
    constexpr float margin = ui::theme::kSearchBarMargin;

    const float dx = static_cast<float>(x - m_dragStartX);
    const float dy = static_cast<float>(y - m_dragStartY);
    const auto clampOffset = [](float value, float minValue, float maxValue) {
        return minValue <= maxValue ? std::clamp(value, minValue, maxValue)
                                    : (minValue + maxValue) * 0.5F;
    };

    if (m_dragTarget == DragTarget::CompileTerminal) {
        const ui::CompileTerminalLayout base = compileTerminalBaseLayout();
        m_compileTerminalOffsetX = clampOffset(
            m_dragStartOffsetX + dx, margin - base.panel.x0,
            static_cast<float>(width) - margin - base.panel.x1);
        m_compileTerminalOffsetY = clampOffset(
            m_dragStartOffsetY + dy, margin - base.panel.y0,
            static_cast<float>(height) - margin - base.panel.y1);
    } else if (m_dragTarget == DragTarget::DiagnosticCard) {
        const ui::DiagnosticCardLayout current = diagnosticCardLayout();
        const ui::DiagnosticCardLayout base =
            ui::offsetDiagnosticCard(current, -m_diagnosticOffsetX,
                                     -m_diagnosticOffsetY);
        m_diagnosticOffsetX = clampOffset(
            m_dragStartOffsetX + dx, margin - base.card.x0,
            static_cast<float>(width) - margin - base.card.x1);
        m_diagnosticOffsetY = clampOffset(
            m_dragStartOffsetY + dy, margin - base.card.y0,
            static_cast<float>(height) - margin - base.card.y1);
    }
}

void Application::endDrag() {
    m_dragTarget = DragTarget::None;
}

void Application::updateCompileTerminalHover(double x, double y) {
    if (!m_compileTerminalVisible || m_searchBar.isOpen() ||
        m_compileTerminalWindow != nullptr) {
        m_compileTerminalCloseHovered = false;
        return;
    }
    m_compileTerminalCloseHovered =
        compileTerminalLayout().closeButton.contains(x, y);
}

void Application::focusCompileTerminal(bool focused) {
    m_compileTerminalFocused = m_compileTerminalVisible && focused;
}

bool Application::ensureCompileTerminalWindow() {
    if (m_compileTerminalWindow != nullptr &&
        m_compileTerminalRenderer != nullptr) {
        return true;
    }

    destroyCompileTerminalWindow();

    try {
        WindowConfig config;
        config.width = 620;
        config.height = 250;
        config.minWidth = 380;
        config.minHeight = 180;
        config.title = std::string(kAppDisplayName) + " - Compile";
        config.resizable = true;
        m_compileTerminalWindow = std::make_unique<Window>(config);
        platform::configureApplicationWindow(m_compileTerminalWindow->handle(),
                                             config.title);
        m_compileTerminalRenderer = std::make_unique<Renderer>(
            *m_compileTerminalWindow, m_config.renderer);
        m_compileTerminalRenderer->setUiZoom(m_uiZoom);

        m_compileTerminalWindow->setKeyCallback(
            [this](const input::KeyEvent& event) {
                handleCompileTerminalKey(event);
            });
        m_compileTerminalWindow->setCharCallback(
            [this](char32_t codepoint) {
                handleCompileTerminalChar(codepoint);
            });
        m_compileTerminalWindow->setMouseButtonCallback(
            [this](const input::MouseButtonEvent& event) {
                handleCompileTerminalMouseButton(event);
            });
        m_compileTerminalWindow->setMouseMoveCallback(
            [this](const input::MouseMoveEvent& event) {
                handleCompileTerminalMouseMove(event);
            });
        m_compileTerminalWindow->setScrollCallback([this](const input::ScrollEvent& event) {
            scrollCompileOutput(event.yOffset, true);
        });
        m_compileTerminalWindow->show();
        m_compileTerminalWindow->focus();
        return true;
    } catch (const std::exception& exception) {
        destroyCompileTerminalWindow();
        setStatus(std::string("Could not open compile window: ") +
                      exception.what(),
                  true);
        return false;
    }
}

void Application::destroyCompileTerminalWindow() {
    m_compileTerminalCloseRequested = false;
    m_compileTerminalRenderer.reset();
    m_compileTerminalWindow.reset();
}

void Application::handleCompileTerminalKey(const input::KeyEvent& event) {
    if (event.action == input::KeyAction::Release) {
        return;
    }
    if (event.action == input::KeyAction::Press &&
        event.key == input::Key::Escape) {
        m_compileTerminalCloseRequested = true;
        return;
    }
#if defined(__APPLE__)
    const bool commandDown = event.super;
#else
    const bool commandDown = event.control;
#endif
    if (event.action == input::KeyAction::Press && commandDown && event.key == input::Key::A) {
        selectAllCompileOutput(); return;
    }
    if (event.action == input::KeyAction::Press && commandDown && event.key == input::Key::C &&
        m_compileSelectionAnchor != m_compileSelectionEnd) { copyCompileSelection(); return; }
    if (event.action == input::KeyAction::Press && commandDown &&
        event.key == input::Key::W) {
        m_compileTerminalCloseRequested = true;
        return;
    }
    if (!m_compileTerminalProcess.alive()) {
        return;
    }

    switch (event.key) {
    case input::Key::Enter:
        if (!m_compileTerminalInput.empty()) {
            m_compileTerminalProcess.appendEcho(m_compileTerminalInput);
        }
        m_compileTerminalProcess.write("\n");
        m_compileTerminalInput.clear();
        break;
    case input::Key::Tab:
        m_compileTerminalProcess.write("\t");
        break;
    case input::Key::Backspace:
        m_compileTerminalProcess.write("\x7f");
        popUtf8(m_compileTerminalInput);
        break;
    case input::Key::Delete:
        m_compileTerminalProcess.write("\x1b[3~");
        break;
    case input::Key::Left:
        m_compileTerminalProcess.write("\x1b[D");
        break;
    case input::Key::Right:
        m_compileTerminalProcess.write("\x1b[C");
        break;
    case input::Key::Up:
        m_compileTerminalProcess.write("\x1b[A");
        break;
    case input::Key::Down:
        m_compileTerminalProcess.write("\x1b[B");
        break;
    case input::Key::Home:
        m_compileTerminalProcess.write("\x1b[H");
        break;
    case input::Key::End:
        m_compileTerminalProcess.write("\x1b[F");
        break;
    default:
        break;
    }
}

void Application::handleCompileTerminalChar(char32_t codepoint) {
    if (!m_compileTerminalProcess.alive()) {
        return;
    }
    std::string encoded;
    appendUtf8(encoded, codepoint);
    m_compileTerminalProcess.write(encoded);
    if (m_compileTerminalInput.size() < kMaxTerminalInput) {
        m_compileTerminalInput += encoded;
    }
}

std::size_t Application::compileOutputPosition(double x, double y, bool standalone) const {
    ui::Rect output;
    float advance = m_charAdvance > 0.0F ? m_charAdvance : 7.0F;
    if (standalone && m_compileTerminalWindow) {
        int width = 0, height = 0; m_compileTerminalWindow->windowSize(width, height);
        const float padding = ui::theme::kCompileTerminalPadding;
        output = {padding, ui::theme::kCompileTerminalTitleHeight,
                  static_cast<float>(width) / m_uiZoom - padding,
                  static_cast<float>(height) / m_uiZoom - padding - 24.0F};
        if (m_compileTerminalRenderer) advance = m_compileTerminalRenderer->logicalCharacterAdvance();
    } else output = compileTerminalLayout().output;
    const auto& text = m_compileSelectionSnapshot;
    std::vector<std::size_t> starts;
    if (!text.empty()) starts.push_back(0);
    for (std::size_t i = 0; i + 1 < text.size(); ++i) if (text[i] == '\n') starts.push_back(i + 1);
    if (starts.empty()) return 0;
    const int visible = std::max(1, static_cast<int>((output.y1 - output.y0) / ui::theme::kEditorLineHeight));
    const int first = std::max(0, static_cast<int>(starts.size()) - visible - m_compileOutputScroll);
    const int row = std::clamp(static_cast<int>((y - output.y0) / ui::theme::kEditorLineHeight), 0,
                               static_cast<int>(starts.size()) - first - 1);
    std::size_t position = starts[static_cast<std::size_t>(first + row)];
    const auto end = text.find('\n', position);
    const auto lineEnd = end == std::string::npos ? text.size() : end;
    const double target = std::max(0.0, x - output.x0);
    const double cellWidth = std::max(advance, 1.0F);
    double width = 0.0;
    const std::string_view line(text.data(), lineEnd);
    while (position < lineEnd) {
        const auto previous = position;
        const auto codepoint = lucid::text::nextUtf8(line, position);
        const double characterWidth = cellWidth * (codepoint == U'\t' ? 4.0 : 1.0);
        if (target < width + characterWidth * 0.5) return previous;
        width += characterWidth;
    }
    return position;
}

void Application::selectAllCompileOutput() {
    m_compileSelectionSnapshot = std::string(m_compileTerminalProcess.output());
    m_compileSelectionAnchor = 0;
    m_compileSelectionEnd = m_compileSelectionSnapshot.size();
    m_compileSelecting = false;
}

void Application::copyCompileSelection() {
    const auto start = std::min(m_compileSelectionAnchor, m_compileSelectionEnd);
    const auto end = std::min(std::max(m_compileSelectionAnchor, m_compileSelectionEnd), m_compileSelectionSnapshot.size());
    if (start >= end) return;
    if (m_compileTerminalWindow) m_compileTerminalWindow->setClipboardText(m_compileSelectionSnapshot.substr(start, end - start));
    else m_window.setClipboardText(m_compileSelectionSnapshot.substr(start, end - start));
}

void Application::updateCompileOutput() {
    const auto updateSelection = [this] {
        if (!m_compileSelecting) return;
        const auto output = m_compileTerminalProcess.output();
        if (output.starts_with(m_compileSelectionSnapshot)) {
            if (output.size() != m_compileSelectionSnapshot.size()) m_compileSelectionSnapshot.assign(output);
        } else {
            // Output was replaced or trimmed. Keep the existing selection
            // copyable, but stop mapping a drag against different text.
            m_compileSelecting = false;
        }
    };
    if (m_compileOutputScroll <= 0 || !m_compileTerminalProcess.alive()) {
        m_compileTerminalProcess.update();
        updateSelection();
        return;
    }
    const auto lineCount = [](std::string_view output) {
        return output.empty() ? 0 : static_cast<int>(std::count(output.begin(), output.end(), '\n')) + (output.back() == '\n' ? 0 : 1);
    };
    const int before = lineCount(m_compileTerminalProcess.output());
    m_compileTerminalProcess.update();
    updateSelection();
    const int after = lineCount(m_compileTerminalProcess.output());
    // The offset is measured from the end. Keep the same lines visible when
    // output grows, while following new output if the user is already at the end.
    if (after > before) m_compileOutputScroll += after - before;
}

void Application::scrollCompileOutput(double offset, bool standalone) {
    if (!std::isfinite(offset) || offset == 0.0) return;
    const auto output = m_compileTerminalProcess.output();
    const int lines = output.empty() ? 0 : static_cast<int>(std::count(output.begin(), output.end(), '\n')) + (output.back() == '\n' ? 0 : 1);
    float height = 0.0F;
    if (standalone && m_compileTerminalWindow) {
        int width = 0, windowHeight = 0; m_compileTerminalWindow->windowSize(width, windowHeight);
        height = static_cast<float>(windowHeight) / m_uiZoom - ui::theme::kCompileTerminalTitleHeight - ui::theme::kCompileTerminalPadding - 24.0F;
    } else {
        const auto area = compileTerminalLayout().output; height = area.y1 - area.y0;
    }
    const int visible = std::max(1, static_cast<int>(height / ui::theme::kEditorLineHeight));
    const int maximum = std::max(0, lines - visible);
    const int clamped = std::clamp(m_compileOutputScroll, 0, maximum);
    if (clamped != m_compileOutputScroll) {
        m_compileOutputScroll = clamped;
        m_compileScrollRemainder = 0.0;
    }
    if ((m_compileOutputScroll <= 0 && offset < 0.0) || (m_compileOutputScroll >= maximum && offset > 0.0)) {
        m_compileScrollRemainder = 0.0;
        return;
    }
    m_compileScrollRemainder += std::clamp(offset, -1000.0, 1000.0) * 3.0;
    const int steps = static_cast<int>(m_compileScrollRemainder);
    m_compileScrollRemainder -= static_cast<double>(steps);
    const int target = m_compileOutputScroll + steps;
    m_compileOutputScroll = std::clamp(target, 0, maximum);
    if (target != m_compileOutputScroll) m_compileScrollRemainder = 0.0;
}

void Application::handleCompileTerminalMouseButton(
    const input::MouseButtonEvent& event) {
    if (m_compileTerminalWindow == nullptr ||
        event.button != input::MouseButton::Left) {
        return;
    }
    if (event.action == input::KeyAction::Press) {
        const double x = event.x / m_uiZoom, y = event.y / m_uiZoom;
        int width = 0, height = 0; m_compileTerminalWindow->windowSize(width, height);
        const ui::Rect output{ui::theme::kCompileTerminalPadding, ui::theme::kCompileTerminalTitleHeight,
            static_cast<float>(width) / m_uiZoom - ui::theme::kCompileTerminalPadding,
            static_cast<float>(height) / m_uiZoom - ui::theme::kCompileTerminalPadding - 24.0F};
        if (output.contains(x, y)) {
            m_compileSelectionSnapshot = std::string(m_compileTerminalProcess.output());
            m_compileSelectionAnchor = m_compileSelectionEnd = compileOutputPosition(x, y, true);
            m_compileSelecting = true;
        } else if (y <= ui::theme::kCompileTerminalTitleHeight && x > 80.0) {
            m_compileTerminalWindow->beginWindowDrag();
        }
    } else {
        m_compileSelecting = false;
        m_compileTerminalWindow->endWindowDrag();
    }
}

void Application::handleCompileTerminalMouseMove(
    const input::MouseMoveEvent& event) {
    if (m_compileSelecting) m_compileSelectionEnd = compileOutputPosition(event.x / m_uiZoom, event.y / m_uiZoom, true);
    if (m_compileTerminalWindow != nullptr &&
        m_compileTerminalWindow->isDraggingWindow()) {
        m_compileTerminalWindow->updateWindowDrag();
    }
}

ui::FrameState
Application::makeCompileTerminalFrameState(double timeSeconds) const {
    ui::FrameState state;
    state.timeSeconds = timeSeconds;
    state.animationSpeed = m_animationSpeed;
    state.backgroundMotion = false;
    state.showWelcome = false;
    state.hasProject = false;
    state.hasDocument = false;
    state.compileTerminalVisible = !m_compileTerminalTitle.empty();
    state.compileTerminalStandalone = true;
    state.compileTerminalFocused = true;
    state.compileTerminalRunning =
        m_compileTerminalProcess.alive() &&
        !m_compileTerminalProcess.commandsDone();
    state.compileTerminalFailed = std::any_of(m_compileTerminalProcess.exitCodes().begin(), m_compileTerminalProcess.exitCodes().end(), [](int code) { return code != 0; });
    state.compileTerminalOpacity = 1.0F;
    state.compileTerminalReveal = 1.0F;
    state.compileTerminalLine = 0;
    state.compileOutputScroll = m_compileOutputScroll;
    state.compileTerminalTitle = m_compileTerminalTitle;
    state.compileTerminalOutput = m_compileTerminalProcess.output();
    if (state.compileTerminalOutput.starts_with(m_compileSelectionSnapshot)) {
        state.compileSelectionStart = std::min(m_compileSelectionAnchor, m_compileSelectionEnd);
        state.compileSelectionEnd = std::max(m_compileSelectionAnchor, m_compileSelectionEnd);
    }
    state.compileTerminalInput = m_compileTerminalInput;
    return state;
}

void Application::closeCompileTerminal() {
    m_compileOutputScroll = 0;
    m_compileScrollRemainder = 0.0;
    m_compileSelecting = false; m_compileSelectionSnapshot.clear();
    m_compileSelectionAnchor = m_compileSelectionEnd = 0;
    m_compileTerminalProcess.close();
    destroyCompileTerminalWindow();
    m_compileTerminalVisible = false;
    m_compileTerminalFocused = false;
    m_compileTerminalCloseHovered = false;
    m_compileTerminalTitle.clear();
    m_compileTerminalInput.clear();
    m_compileTerminalLine = 0;
    m_compileTerminalReveal = 0.0F;
    m_compileTerminalVelocity = 0.0F;
    m_compileTerminalOffsetX = 0.0F;
    m_compileTerminalOffsetY = 0.0F;
    if (m_dragTarget == DragTarget::CompileTerminal) {
        m_dragTarget = DragTarget::None;
    }
}

void Application::runCompileAtCursor() {
    if (m_searchBar.isOpen()) {
        return;
    }
    if (!m_hasDocument) {
        setStatus("No file is open", false);
        return;
    }
    refreshCompileScript();
    const editor::CompileFunction* function = editor::findCompileFunctionAt(
        m_compileScript, static_cast<int>(m_document.cursorLine()));
    if (function == nullptr) {
        setStatus("No compile function at cursor", true);
        return;
    }
    runCompileFunction(*function);
}

void Application::runCompileFunction(editor::CompileFunction function,
                                     std::string sourcePath) {
    if (!m_hasProject) {
        setStatus("Open a project before running a compile function", true);
        return;
    }
    if (!function.runsHere()) {
        setStatus("Compile function " + function.name +
                      " is not available on this platform",
                  true);
        return;
    }
    if (function.commands.empty()) {
        setStatus("Compile function " + function.name + " has no commands", true);
        return;
    }
    if (!saveDocument()) {
        return;
    }
    if (sourcePath.empty()) {
        std::string disk, error;
        if (!m_hasDocument || !m_project.readFile(m_filePath, disk, error) || disk != m_diskContents) {
            setStatus("Compile source changed or became unavailable; reopen it before running", true);
            return;
        }
    }

    const std::string projectRoot = m_project.root().generic_string();
    const std::string relativePath = sourcePath.empty() ? m_filePath : sourcePath;
    const std::filesystem::path file =
        sourcePath.empty() ? std::filesystem::path(absoluteDocumentPath())
                           : m_project.root() / sourcePath;
    const std::string filePath = file.generic_string();
    const std::string platform =
        editor::currentCompilePlatform() == editor::CompilePlatform::MacOS
            ? "macOS"
            : (editor::currentCompilePlatform() == editor::CompilePlatform::Windows
                   ? "windows"
                   : (editor::currentCompilePlatform() ==
                              editor::CompilePlatform::Linux
                          ? "linux"
                          : (editor::currentCompilePlatform() == editor::CompilePlatform::FreeBSD
                              ? "freebsd" : editor::currentCompilePlatform() == editor::CompilePlatform::OpenBSD
                                  ? "openbsd" : "unknown")));

    std::unordered_map<std::string, std::string> variables;
    variables["project"] = projectRoot;
    variables["projectName"] = m_projectName;
    variables["file"] = filePath;
    variables["fileDir"] = file.parent_path().generic_string();
    variables["fileName"] = file.filename().string();
    variables["relative"] = relativePath;
    variables["line"] = std::to_string(function.definitionLine + 1);
    variables["platform"] = platform;

    std::vector<std::string> commands;
    commands.reserve(function.commands.size());
    for (const editor::CompileCommand& command : function.commands) {
        commands.push_back(editor::expandCompileShellVariables(command.text, variables));
    }

    if (!ensureCompileTerminalWindow()) {
        return;
    }

    std::string error;
    if (!m_compileTerminalProcess.start(projectRoot, std::move(commands), error)) {
        closeCompileTerminal();
        setStatus(error, true);
        return;
    }

    m_compileOutputScroll = 0;
    m_compileScrollRemainder = 0.0;
    m_compileSelecting = false;
    m_compileSelectionSnapshot.clear();
    m_compileSelectionAnchor = m_compileSelectionEnd = 0;
    m_compileTerminalTitle = function.name;
    m_compileTerminalLine = function.definitionLine;
    m_compileTerminalOffsetX = 0.0F;
    m_compileTerminalOffsetY = 0.0F;
    m_compileTerminalInput.clear();
    m_compileTerminalVisible = true;
    m_compileTerminalFocused = false;
    m_compileTerminalCloseHovered = false;
    m_compileTerminalReveal = 1.0F;
    m_compileTerminalVelocity = 0.0F;
    if (m_compileTerminalWindow != nullptr) {
        m_compileTerminalWindow->setTitle(std::string(kAppDisplayName) + " - " + function.name);
        m_compileTerminalWindow->focus();
    }
    setStatus("Running compile function " + function.name, false);
    log::info("Running compile function " + function.name + " in " + projectRoot);
}

void Application::ensureFunctionIndex() {
    if (!m_hasProject || (m_functionIndexValid && !m_functionIndexDirty) || m_functionIndexFuture.valid()) {
        return;
    }
    auto project = m_project; const auto root = project.root().generic_string(); const auto generation = m_functionIndexGeneration;
    m_functionIndexFuture = std::async(std::launch::async, [project, root, generation]() mutable {
        FunctionIndexResult result; result.root = root; result.generation = generation;
        project.refresh(); result.index = workspace::loadFunctionIndex(project, result.error); return result;
    });
}

void Application::invalidateFunctionIndex() {
    m_functionIndexDirty = true;
    ++m_functionIndexGeneration;
}

void Application::pollFunctionIndex() {
    if (!m_functionIndexFuture.valid() || m_functionIndexFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    try {
        auto result = m_functionIndexFuture.get();
        if (m_hasProject && result.root == m_project.root().generic_string() && result.generation == m_functionIndexGeneration) {
            m_functionIndex = std::move(result.index); m_functionIndexValid = true; m_functionIndexDirty = false;
            if (!result.error.empty()) log::warn("Function index cache: " + result.error);
            if (m_searchBar.isOpen() && m_searchBar.kind() == ui::PromptKind::Function) { clearCommandFeedback(); refreshSuggestions(); }
        }
    } catch (const std::exception& e) { setStatus(std::string("Function indexing failed: ") + e.what(), true); }
    if (m_hasProject && m_searchBar.isOpen() && m_searchBar.kind() == ui::PromptKind::Function && (!m_functionIndexValid || m_functionIndexDirty)) ensureFunctionIndex();
}

void Application::scheduleProjectIndex() {
    ++m_projectIndexGeneration;
    m_projectIndexPending = m_hasProject;
    if (!m_hasProject || m_projectIndexFuture.valid()) return;
    auto project = m_project; const auto root = project.root().generic_string(); const auto generation = m_projectIndexGeneration;
    m_projectIndexPending = false;
    m_projectIndexFuture = std::async(std::launch::async, [project, root, generation]() mutable {
        project.refresh(); return ProjectIndexResult{std::move(project), root, generation};
    });
}

void Application::pollProjectIndex() {
    if (!m_projectIndexFuture.valid() || m_projectIndexFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    try {
        auto result = m_projectIndexFuture.get();
        if (m_hasProject && result.root == m_project.root().generic_string() && result.generation == m_projectIndexGeneration) {
            m_project = std::move(result.project);
            invalidateFunctionIndex();
            if (m_searchBar.isOpen() && m_searchBar.kind() == ui::PromptKind::File) { clearCommandFeedback(); refreshSuggestions(); }
        }
    } catch (const std::exception& e) { setStatus(std::string("Project indexing failed: ") + e.what(), true); }
    if (m_projectIndexPending && m_hasProject) scheduleProjectIndex();
}

void Application::runProjectFunction(const workspace::ProjectFunction& function) {
    if (!m_hasProject) {
        setStatus("Open a project before running a function", true);
        return;
    }
    if (!function.runsHere()) {
        const std::string_view platform =
            workspace::functionPlatformName(function.platform);
        setStatus("Function " + function.name + " is only available on " +
                      std::string(platform.empty() ? "another platform" : platform),
                  true);
        return;
    }

    if (m_filePath != function.file) {
        openFile(function.file, false);
        if (!m_hasDocument || m_filePath != function.file) {
            return;
        }
    }

    if (!m_hasDocument) {
        setStatus("No file is open", true);
        return;
    }
    if (!saveDocument()) return;
    std::string disk, readError;
    if (!m_project.readFile(function.file, disk, readError)) { setStatus("Function source is unavailable: " + readError, true); return; }
    if (disk != m_diskContents) {
        openFile(function.file, false);
        if (!m_hasDocument || m_filePath != function.file || m_diskContents != disk) return;
    }

    const int line = std::clamp(
        function.line, 0, std::max(0, static_cast<int>(m_document.lineCount()) - 1));
    m_document.setCursor(static_cast<std::size_t>(line), 0);
    m_followCursor = true;
    refreshCompileScript();

    const editor::CompileFunction* parsed =
        editor::findCompileFunctionAt(m_compileScript, line);
    if (parsed != nullptr && parsed->name == function.name &&
        parsed->definitionLine == function.line) {
        runCompileFunction(*parsed);
        return;
    }

    invalidateFunctionIndex();
    setStatus("Function changed or moved; search again before running it", true);
}

void Application::updateCompileTerminalAnimation(double deltaSeconds) {
    const float target = m_compileTerminalVisible ? 1.0F : 0.0F;
    float remaining = std::clamp(static_cast<float>(deltaSeconds), 0.0F, 0.05F) *
                      std::clamp(m_animationSpeed, 0.25F, 4.0F);
    constexpr float omega = 23.0F;
    constexpr float damping = 0.88F;
    constexpr float maxStep = 1.0F / 240.0F;

    while (remaining > 0.0F) {
        const float step = std::min(remaining, maxStep);
        const float acceleration =
            omega * omega * (target - m_compileTerminalReveal) -
            2.0F * damping * omega * m_compileTerminalVelocity;
        m_compileTerminalVelocity += acceleration * step;
        m_compileTerminalReveal += m_compileTerminalVelocity * step;
        remaining -= step;
    }

    if (!m_compileTerminalVisible && std::abs(m_compileTerminalReveal) < 0.002F &&
        std::abs(m_compileTerminalVelocity) < 0.02F) {
        m_compileTerminalReveal = 0.0F;
        m_compileTerminalVelocity = 0.0F;
    }
}

void Application::resetDiagnostics() {
    m_diagnostics.clear();
    m_ignoredDiagnostics.clear();
    m_diagnosticsValid = false;
    m_activeDiagnosticKey.clear();
    m_activeDiagnosticMessage.clear();
    m_activeDiagnosticCode.clear();
    m_activeDiagnosticLine = 0;
    m_activeDiagnosticColumn = 0;
    m_activeDiagnosticLength = 1;
    m_diagnosticIgnoreHovered = false;
    m_diagnosticOffsetX = 0.0F;
    m_diagnosticOffsetY = 0.0F;
    m_compilerAppliedRevision = 0;
    m_compilerAppliedPath.clear();
    m_compilerDueTime = 0.0;
}

void Application::openDiagnosticList() {
    if (!m_hasDocument || !m_diagnosticsValid || m_diagnosticsRevision != m_document.revision()) {
        setStatus("Diagnostics are not ready for this version of the file", false); return;
    }
    m_diagnosticListPage = 0; m_pagedScroll = 0; m_searchBar.open(ui::PromptKind::Diagnostics);
    clearCommandFeedback(); refreshSuggestions();
    setCommandStatus(m_diagnosticListChoices.empty() ? "No diagnostics in this file" : "Select a diagnostic to jump to it", false);
}

void Application::applyDiagnosticList() {
    refreshSuggestions();
    if (m_selectedSuggestion < 0 || static_cast<std::size_t>(m_selectedSuggestion) >= m_diagnosticListChoices.size()) return;
    const int choice = m_diagnosticListChoices[static_cast<std::size_t>(m_selectedSuggestion)];
    if (choice == -1 || choice == -2) {
        if (choice == -1 && m_diagnosticListPage) --m_diagnosticListPage;
        if (choice == -2) ++m_diagnosticListPage;
        m_pagedScroll = 0; m_selectedSuggestion = -1; m_menuReselectionRequired = true; refreshSuggestions(); return;
    }
    if (choice < 0 || !m_hasDocument || !m_diagnosticsValid || m_diagnosticsRevision != m_document.revision() ||
        static_cast<std::size_t>(choice) >= m_diagnostics.size()) { setCommandStatus("Diagnostics changed; reopen the list", true); return; }
    const auto& diagnostic = m_diagnostics[static_cast<std::size_t>(choice)];
    if (diagnostic.line >= m_document.lineCount() || m_ignoredDiagnostics.contains(diagnostic.key())) { setCommandStatus("Diagnostic is no longer available", true); return; }
    m_document.setCursor(diagnostic.line, diagnostic.column ? diagnostic.column - 1 : 0); m_followCursor = true;
    m_activeDiagnosticKey = diagnostic.key(); m_activeDiagnosticMessage = diagnostic.message; m_activeDiagnosticCode = diagnostic.code;
    m_activeDiagnosticLine = static_cast<int>(diagnostic.line); m_activeDiagnosticColumn = static_cast<int>(diagnostic.column);
    m_activeDiagnosticLength = static_cast<int>(diagnostic.length);
    m_searchBar.close(); setStatus(diagnostic.message, false);
}

void Application::navigateDiagnostic(bool backwards) {
    if (!m_hasDocument || !m_diagnosticsValid || m_diagnosticsRevision != m_document.revision()) {
        setStatus("Diagnostics are not ready for this version of the file", false); return;
    }
    std::vector<const editor::Diagnostic*> available;
    for (const auto& diagnostic : m_diagnostics) {
        if (diagnostic.line < m_document.lineCount() && !m_ignoredDiagnostics.contains(diagnostic.key())) available.push_back(&diagnostic);
    }
    if (available.empty()) { setStatus("No diagnostics in this file", false); return; }
    std::stable_sort(available.begin(), available.end(), [](const auto* a, const auto* b) {
        return a->line < b->line || (a->line == b->line && a->column < b->column);
    });
    const auto cursor = std::pair{m_document.cursorLine(), m_document.cursorColumn()};
    const auto position = [](const editor::Diagnostic* d) {
        return std::pair{static_cast<std::size_t>(d->line), static_cast<std::size_t>(d->column > 0 ? d->column - 1 : 0)};
    };
    std::size_t index = available.size();
    for (std::size_t i = 0; i < available.size(); ++i)
        if (available[i]->key() == m_activeDiagnosticKey && position(available[i]) == cursor) { index = i; break; }
    if (index < available.size()) index = backwards ? (index + available.size() - 1) % available.size() : (index + 1) % available.size();
    else if (backwards) {
        index = available.size() - 1;
        for (std::size_t i = available.size(); i > 0; --i) if (position(available[i - 1]) < cursor) { index = i - 1; break; }
    } else {
        index = 0;
        for (std::size_t i = 0; i < available.size(); ++i) if (position(available[i]) >= cursor) { index = i; break; }
    }
    const auto& diagnostic = *available[index];
    const auto target = position(&diagnostic);
    m_document.setCursor(target.first, target.second); m_followCursor = true;
    m_activeDiagnosticKey = diagnostic.key(); m_activeDiagnosticMessage = diagnostic.message; m_activeDiagnosticCode = diagnostic.code;
    m_activeDiagnosticLine = static_cast<int>(diagnostic.line); m_activeDiagnosticColumn = static_cast<int>(diagnostic.column);
    m_activeDiagnosticLength = static_cast<int>(diagnostic.length);
    setStatus("Diagnostic " + std::to_string(index + 1) + "/" + std::to_string(available.size()) + ": " + diagnostic.message, false);
}

void Application::updateActiveDiagnostic() {
    m_activeDiagnosticKey.clear();
    m_activeDiagnosticMessage.clear();
    m_activeDiagnosticCode.clear();
    m_activeDiagnosticLine = 0;
    m_activeDiagnosticColumn = 0;
    m_activeDiagnosticLength = 1;

    if (isJsonDocument()) {
        m_diagnosticLabel = "JSON ERROR";
    } else {
        const std::string extension =
            std::filesystem::path(m_filePath).extension().string();
        m_diagnosticLabel = extension == ".c" ? "C ERROR" : "C++ ERROR";
    }

    for (const editor::Diagnostic& diagnostic : m_diagnostics) {
        const std::string key = diagnostic.key();
        if (m_ignoredDiagnostics.contains(key)) {
            continue;
        }
        m_activeDiagnosticKey = key;
        m_activeDiagnosticMessage = diagnostic.message;
        m_activeDiagnosticCode = diagnostic.code;
        m_activeDiagnosticLine = static_cast<int>(diagnostic.line);
        m_activeDiagnosticColumn = static_cast<int>(diagnostic.column);
        m_activeDiagnosticLength = static_cast<int>(diagnostic.length);
        return;
    }
}

void Application::pollCompilerDiagnostics(double timeSeconds) {
    if (!m_compilerRunning || !m_compilerFuture.valid()) {
        return;
    }
    if (m_compilerFuture.wait_for(std::chrono::seconds(0)) !=
        std::future_status::ready) {
        return;
    }

    editor::CompilerResult result = m_compilerFuture.get();
    m_compilerRunning = false;

    if (m_hasDocument && m_languageDiagnosticsFile == m_filePath && m_languageDiagnosticsRevision == m_document.revision()) return;

    if (!m_hasDocument || m_document.revision() != m_compilerRunningRevision ||
        absoluteDocumentPath() != m_compilerRunningPath) {
        m_compilerDueTime = timeSeconds + 0.05;
        return;
    }

    if (!result.error.empty()) {
        log::warn("Compiler diagnostics unavailable: " + result.error);
        m_diagnostics.clear();
    } else {
        m_diagnostics = std::move(result.diagnostics);
    }
    m_compilerAppliedRevision = m_compilerRunningRevision;
    m_compilerAppliedPath = m_compilerRunningPath;
    m_diagnosticsRevision = m_compilerRunningRevision;
    m_diagnosticsValid = true;
    updateActiveDiagnostic();
}

void Application::refreshDiagnostics(double timeSeconds) {
    if (!m_hasDocument) {
        resetDiagnostics();
        return;
    }

    const std::string path = absoluteDocumentPath();
    const std::uint64_t revision = m_document.revision();

    if (isJsonDocument()) {
        if (m_diagnosticsValid && m_diagnosticsRevision == revision &&
            m_compilerAppliedPath == path) {
            return;
        }
        m_diagnostics = editor::diagnoseJson(m_document.lines());
        m_diagnosticsRevision = revision;
        m_compilerAppliedRevision = revision;
        m_compilerAppliedPath = path;
        m_diagnosticsValid = true;
        updateActiveDiagnostic();
        return;
    }

    if (!isCodeDocument()) {
        if (m_diagnosticsValid && m_diagnosticsRevision == revision) {
            return;
        }
        m_diagnostics.clear();
        m_diagnosticsRevision = revision;
        m_compilerAppliedRevision = revision;
        m_compilerAppliedPath = path;
        m_diagnosticsValid = true;
        updateActiveDiagnostic();
        return;
    }

    if (m_diagnosticsValid && m_diagnosticsRevision == revision &&
        m_compilerAppliedPath == path) {
        return;
    }
    if (m_compilerRunning) {
        return;
    }

    if (m_compilerDueTime == 0.0) {
        m_compilerDueTime = timeSeconds + 0.18;
    }
    if (timeSeconds < m_compilerDueTime) {
        return;
    }
    m_compilerDueTime = 0.0;

    editor::CompilerRequest request;
    request.filePath = path;
    request.projectRoot = m_project.root().generic_string();
    request.content = m_document.toUtf8();
    request.cpp = std::filesystem::path(m_filePath).extension() != ".c";

    m_compilerRunningRevision = revision;
    m_compilerRunningPath = path;
    m_compilerRunning = true;
    m_compilerFuture = std::async(std::launch::async, [request = std::move(request)] {
        return editor::runCompilerDiagnostics(request);
    });
}

void Application::initializeSettings() {
    const auto directory = settingsDirectory();
    std::error_code canonicalError;
    const auto canonical = directory.empty() ? std::filesystem::path{} : std::filesystem::weakly_canonical(directory, canonicalError);
    m_settingsRoot = (canonicalError ? directory : canonical).generic_string();
    if (m_settingsRoot.empty()) {
        m_settingsError = "Could not locate the settings directory";
        return;
    }
    if (ensureSettingsFile()) loadSettingsFromDisk();
}

bool Application::ensureSettingsFile() {
    if (m_settingsRoot.empty()) {
        m_settingsError = "Settings directory is unavailable";
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(m_settingsRoot, error);
    if (error) {
        m_settingsError = "Could not create settings project: " + error.message();
        return false;
    }

    const std::filesystem::path file =
        std::filesystem::path(m_settingsRoot) / "settings.json";
    if (std::filesystem::exists(file, error)) {
        if (std::filesystem::is_regular_file(file, error) && !error) return true;
        m_settingsError = "settings.json is not a regular file"; return false;
    }
    if (error) { m_settingsError = "Could not inspect settings.json: " + error.message(); return false; }

    std::string projectError;
    bool created = false;
    auto project = workspace::Project::openOrCreate(m_settingsRoot, created, projectError, false);
    if (!projectError.empty()) { m_settingsError = "Could not open settings directory: " + projectError; return false; }
    if (!project.writeFile("settings.json", defaultSettingsJson(), projectError, nullptr, true)) {
        // Another process may have created it after the existence check.
        if (std::filesystem::is_regular_file(file, error) && !error) return true;
        m_settingsError = "Could not safely initialize settings.json: " + projectError;
        return false;
    }
    return true;
}

void Application::loadSettingsFromDisk() {
    if (m_settingsRoot.empty()) {
        return;
    }

    const std::filesystem::path file =
        std::filesystem::path(m_settingsRoot) / "settings.json";
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        m_settingsError = "Could not read settings.json";
        return;
    }
    std::string text((std::istreambuf_iterator<char>(stream)),
                     std::istreambuf_iterator<char>());
    if (stream.bad()) { m_settingsError = "Could not read settings.json completely"; m_settingsBaseline.reset(); return; }
    const SettingsResult result = parseSettings(text);
    if (!result.valid) {
        m_settingsError = result.error;
        m_settingsBaseline.reset();
        m_settingsBaselineRoot = m_settingsRoot;
        return;
    }

    m_settingsError.clear();
    m_settingsBaselineRoot = m_settingsRoot;
    m_settingsBaseline = text;
    applySettings(result.settings);
}

void Application::applySettings(const EditorSettings& settings) {
    m_settings = settings;
    m_animationSpeed = settings.animationSpeed;
    m_showDiagnostics = settings.showSyntaxErrors;
    m_showLineNumbers = settings.showLineNumbers;
    m_highlightCurrentLine = settings.highlightCurrentLine;
    m_cursorBlink = settings.cursorBlink;
    m_cursorStyle = settings.cursorStyle;
    m_backgroundMotion = settings.backgroundMotion;
    applyTheme(settings.theme);
    platform::setThemedApplicationIcon(m_window.handle(), ui::theme::kPrimary.r,
        ui::theme::kPrimary.g, ui::theme::kPrimary.b, settings.iconTheme);

    if (std::abs(m_uiZoom - settings.zoomFactor) > 0.001F) {
        m_uiZoom = std::clamp(settings.zoomFactor, 0.5F, 3.0F);
        if (m_renderer != nullptr) {
            m_renderer->setUiZoom(m_uiZoom);
            m_charAdvance = m_renderer->logicalCharacterAdvance();
        }
    }
}

void Application::applyTheme(const ThemeSettings& themeSettings) {
    ui::theme::Palette palette;
    if (!ui::theme::paletteByName(themeSettings.name, palette)) {
        palette = ui::theme::lucidDarkPalette();
    }
    if (themeSettings.primary.has_value()) {
        palette.primary = *themeSettings.primary;
        palette.syntaxFunction = *themeSettings.primary;
        palette.selection =
            ui::theme::selectionTint(palette.background, palette.primary);
    }

    if (m_settings.highContrast) palette = ui::theme::highContrastPalette(palette);
    ui::theme::applyPalette(palette);
    if (m_renderer != nullptr) {
        const ui::theme::Color& background = ui::theme::kBackground;
        m_renderer->setClearColor({background.r, background.g, background.b, 1.0F});
    }
}

void Application::refreshSettings() {
    if (m_settingsRoot.empty() || !m_hasDocument || !isSettingsDocument()) {
        return;
    }

    const std::uint64_t revision = m_document.revision();
    if (m_settingsRevision == revision) {
        return;
    }
    m_settingsRevision = revision;

    const SettingsResult result = parseSettings(m_document.toUtf8());
    if (!result.valid) {
        m_settingsError = result.error;
        return;
    }

    m_settingsError.clear();
    applySettings(result.settings);
}

void Application::syncAccessibility(const ui::FrameState& frame) {
#if defined(__APPLE__)
    std::string label, value, key;
    std::size_t start = 0, length = 0;
    if (frame.settingsPanelVisible) {
        label = "Lucid settings";
        const auto settingName = [](ui::SettingsControl control) -> std::string {
            using C = ui::SettingsControl;
            if (control >= C::PrimaryColor0 && control <= C::PrimaryColor7) return "Primary color " + std::to_string(static_cast<int>(control) - static_cast<int>(C::PrimaryColor0) + 1);
            switch (control) {
            case C::ThemeLucidDark: return "Dark theme"; case C::ThemeGraphite: return "Graphite theme"; case C::ThemeLucidLight: return "Light theme";
            case C::SpeedHalf: return "Animation speed half"; case C::SpeedNormal: return "Animation speed normal"; case C::SpeedDouble: return "Animation speed double";
            case C::Zoom75: return "Text size 75 percent"; case C::Zoom100: return "Text size 100 percent"; case C::Zoom125: return "Text size 125 percent"; case C::Zoom150: return "Text size 150 percent";
            case C::ShowSyntaxErrors: return "Show syntax errors"; case C::ShowLineNumbers: return "Show line numbers"; case C::HighlightCurrentLine: return "Highlight current line";
            case C::CursorBlink: return "Cursor blink"; case C::CursorLine: return "Line cursor"; case C::CursorBlock: return "Block cursor";
            case C::CursorUnderscore: return "Underscore cursor"; case C::CursorKitty: return "Kitty cursor";
            case C::IconClassic: return "Classic icon"; case C::IconAccent: return "Accent icon"; case C::IconOutline: return "Outline icon";
            case C::AiSettings: return "AI settings"; case C::BackgroundMotion: return "Background motion"; case C::OpenSettingsJson: return "Open settings file"; case C::Done: return "Done";
            default: return "None";
            }
        };
        value = "Settings. Use Tab or arrow keys to choose a control, Enter to activate, and Escape to close. Focus: " + settingName(frame.settingsHovered);
        key = label + value;
    } else if (frame.commandVisible && m_searchBar.isOpen()) {
        label = "Lucid command";
        const std::string prefix = std::string(frame.commandPlaceholder.empty() ? "Command" : frame.commandPlaceholder) + "\n";
        value = prefix + std::string(frame.commandQuery);
        const auto utf16Length = [](std::string_view text) {
            std::size_t length = 0, offset = 0;
            while (offset < text.size()) length += text::nextUtf8(text, offset) > 0xffff ? 2 : 1;
            return length;
        };
        start = utf16Length(prefix) + utf16Length(frame.commandQuery.substr(0, m_searchBar.selectionStart()));
        length = utf16Length(frame.commandQuery.substr(m_searchBar.selectionStart(), m_searchBar.selectionEnd() - m_searchBar.selectionStart()));
        for (const auto& item : frame.suggestions) { value += "\n"; value += item.path; if (!item.detail.empty()) { value += ": "; value += item.detail; } }
        const auto kind = m_searchBar.kind();
        const bool scrollable = kind == ui::PromptKind::File || kind == ui::PromptKind::Project || kind == ui::PromptKind::OpenFiles || kind == ui::PromptKind::Function ||
                                kind == ui::PromptKind::SettingsMenu || kind == ui::PromptKind::AiSettings;
        if (scrollable && m_suggestionScroll > 0) value += "\nMore options above";
        if (scrollable && m_suggestionHasMoreBelow) value += "\nMore options below";
        if (isAdaptivePagedPrompt(kind) && m_pagedScroll > 0) value += "\nMore options above";
        if (isAdaptivePagedPrompt(kind) && m_suggestionHasMoreBelow) value += "\nMore options below";
        if (kind == ui::PromptKind::Ai && m_aiReady) {
            if (m_aiScroll > 0) value += "\nMore response above";
            if (m_aiScroll + visibleSuggestionCount() < m_aiLines.size()) value += "\nMore response below";
        }
        if (!frame.commandStatus.empty()) { value += frame.commandStatusIsError ? "\nError: " : "\n"; value += frame.commandStatus; }
        if (m_selectedSuggestion >= 0 && static_cast<std::size_t>(m_selectedSuggestion) < frame.suggestions.size()) {
            const auto& selected = frame.suggestions[static_cast<std::size_t>(m_selectedSuggestion)];
            value += "\nSelected: ";
            value += selected.path;
            if (!selected.detail.empty()) { value += ": "; value += selected.detail; }
        }
        key = label + value + ":" + std::to_string(start) + ":" + std::to_string(length);
    } else if (m_hasDocument) {
        label = "Editor: " + m_filePath;
        if (!m_activeDiagnosticMessage.empty()) label += "; error: " + m_activeDiagnosticMessage;
        if (frame.completionVisible) {
            if (frame.completions.empty()) label += "; no matching completions";
            else {
                const auto selected = std::min(static_cast<std::size_t>(std::max(0, frame.selectedCompletion)), frame.completions.size() - 1);
                label += "; completion " + std::to_string(selected + 1) + " of " + std::to_string(frame.completions.size()) + ": ";
                label += frame.completions[selected];
                label += "; use up and down to choose, Enter to insert, Escape to dismiss";
            }
        }
        const auto from = m_document.hasSelection() ? m_document.selectionStart() : editor::TextPosition{m_document.cursorLine(), m_document.cursorColumn()};
        const auto to = m_document.hasSelection() ? m_document.selectionEnd() : from;
        key = label + ":" + std::to_string(m_document.revision()) + ":" + std::to_string(from.line) + ":" + std::to_string(from.column) + ":" + std::to_string(to.line) + ":" + std::to_string(to.column);
        if (key == m_accessibilityKey) return;
        value = m_document.toUtf8();
        const auto offset = [this](editor::TextPosition position) {
            std::size_t n = 0;
            for (std::size_t i = 0; i < position.line && i < m_document.lineCount(); ++i) {
                for (char32_t c : m_document.line(i)) n += c > 0xffff ? 2 : 1;
                ++n;
            }
            if (position.line < m_document.lineCount()) {
                const auto& line = m_document.line(position.line);
                for (std::size_t i = 0; i < std::min(position.column, line.size()); ++i) n += line[i] > 0xffff ? 2 : 1;
            }
            return n;
        };
        start = offset(from); length = offset(to) - start;
    } else { label = "Lucid"; value = "Open a project to start editing."; key = label + value; }
    if (key == m_accessibilityKey) return;
    m_accessibilityKey = std::move(key);
    platform::updateAccessibility(m_window.handle(), label, value, start, length);
#else
    (void)frame;
#endif
}

void Application::changeZoom(float delta) {
    auto settings = m_settings;
    settings.zoomFactor = delta == 0.0F ? 1.0F : std::clamp(settings.zoomFactor + delta, 0.5F, 3.0F);
    applySettings(settings);
    (void)persistSettings();
}

void Application::toggleHighContrast() {
    auto settings = m_settings; settings.highContrast = !settings.highContrast;
    applySettings(settings);
    if (persistSettings()) setStatus(settings.highContrast ? "High contrast enabled" : "High contrast disabled", false);
}

void Application::openSettings() {
    if (!ensureSettingsFile()) {
        setStatus(m_settingsError, true);
        return;
    }

    if (isSettingsDocument()) {
        const SettingsResult current = parseSettings(m_document.toUtf8());
        if (current.valid) {
            m_settingsError.clear();
            applySettings(current.settings);
        } else {
            m_settingsError = current.error;
        }
    } else {
        loadSettingsFromDisk();
    }
    m_settingsPanelVisible = false;
    m_settingsMenuPage = 0;
    m_searchBar.open(ui::PromptKind::SettingsMenu);
    m_selectedSuggestion = -1;
    clearCommandFeedback();
    setCommandStatus("Choose a setting; Enter opens, Escape closes", false);
    refreshSuggestions();
    m_settingsHovered = ui::SettingsControl::None;
    focusCompileTerminal(false);
    m_welcomeActionHovered = false;
}

void Application::openSettingsJson() {
    if (!ensureSettingsFile()) {
        setStatus(m_settingsError, true);
        return;
    }

    m_settingsPanelVisible = false;
    if (!m_hasProject || m_project.root().generic_string() != m_settingsRoot) {
        openProject(m_settingsRoot);
        if (!m_hasProject || m_project.root().generic_string() != m_settingsRoot) {
            return;
        }
    }

    m_settingsRevision = ~std::uint64_t{0};
    openFile("settings.json");
}

void Application::closeSettingsPanel() {
    m_settingsPanelVisible = false;
    m_settingsHovered = ui::SettingsControl::None;
    m_window.setCursorShape(CursorShape::Arrow);
}

void Application::updateSettingsPanelHover(double x, double y) {
    m_settingsHovered = settingsControlAt(x, y);
}

void Application::moveSettingsFocus(int direction) {
    using ui::SettingsControl;
    static constexpr auto controls = std::to_array<SettingsControl>({
        SettingsControl::ThemeLucidDark, SettingsControl::ThemeGraphite, SettingsControl::ThemeLucidLight,
        SettingsControl::PrimaryColor0, SettingsControl::PrimaryColor1, SettingsControl::PrimaryColor2,
        SettingsControl::PrimaryColor3, SettingsControl::PrimaryColor4, SettingsControl::PrimaryColor5,
        SettingsControl::PrimaryColor6, SettingsControl::PrimaryColor7,
        SettingsControl::SpeedHalf, SettingsControl::SpeedNormal, SettingsControl::SpeedDouble,
        SettingsControl::Zoom75, SettingsControl::Zoom100, SettingsControl::Zoom125, SettingsControl::Zoom150,
        SettingsControl::ShowSyntaxErrors, SettingsControl::ShowLineNumbers,
        SettingsControl::HighlightCurrentLine, SettingsControl::CursorBlink,
        SettingsControl::CursorLine, SettingsControl::CursorBlock,
        SettingsControl::CursorUnderscore, SettingsControl::CursorKitty,
        SettingsControl::IconClassic, SettingsControl::IconAccent, SettingsControl::IconOutline,
        SettingsControl::OpenSettingsJson, SettingsControl::AiSettings, SettingsControl::BackgroundMotion, SettingsControl::Done});
    const auto found = std::find(controls.begin(), controls.end(), m_settingsHovered);
    const int count = static_cast<int>(controls.size());
    const int index = found == controls.end() ? (direction > 0 ? -1 : 0)
        : static_cast<int>(found - controls.begin());
    m_settingsHovered = controls[static_cast<std::size_t>((index + direction + count) % count)];
}

void Application::handleSettingsPanelClick(double x, double y) {
    activateSettingsControl(settingsControlAt(x, y));
}

void Application::activateSettingsControl(ui::SettingsControl control) {
    using ui::SettingsControl;
    if (control == SettingsControl::None) {
        return;
    }
    if (control == SettingsControl::AiSettings) { closeSettingsPanel(); openAiSettings(); return; }
    if (control == SettingsControl::OpenSettingsJson) {
        if (!m_settingsRoot.empty()) {
            closeSettingsPanel();
            openSettingsJson();
        }
        return;
    }
    if (control == SettingsControl::Done) {
        closeSettingsPanel();
        return;
    }

    if (isSettingsDocument()) {
        const SettingsResult current = parseSettings(m_document.toUtf8());
        if (!current.valid) {
            m_settingsError = current.error;
            setStatus("Fix settings.json before using the panel", true);
            return;
        }
        m_settingsError.clear();
        m_settings = current.settings;
    }

    if (control >= SettingsControl::PrimaryColor0 &&
        control <= SettingsControl::PrimaryColor7) {
        const auto index = static_cast<std::size_t>(control) -
                           static_cast<std::size_t>(SettingsControl::PrimaryColor0);
        m_settings.theme.primary = ui::theme::kPrimarySwatches[index];
        applySettings(m_settings);
        persistSettings();
        return;
    }

    switch (control) {
    case SettingsControl::IconClassic:
        m_settings.iconTheme = "classic";
        break;
    case SettingsControl::IconAccent:
        m_settings.iconTheme = "accent";
        break;
    case SettingsControl::IconOutline:
        m_settings.iconTheme = "outline";
        break;
    case SettingsControl::ThemeLucidDark:
        m_settings.theme.name = "lucid-dark";
        m_settings.theme.primary.reset();
        break;
    case SettingsControl::ThemeGraphite:
        m_settings.theme.name = "graphite";
        m_settings.theme.primary.reset();
        break;
    case SettingsControl::ThemeLucidLight:
        m_settings.theme.name = "lucid-light";
        m_settings.theme.primary.reset();
        break;
    case SettingsControl::SpeedHalf:
        m_settings.animationSpeed = 0.5F;
        break;
    case SettingsControl::SpeedNormal:
        m_settings.animationSpeed = 1.0F;
        break;
    case SettingsControl::SpeedDouble:
        m_settings.animationSpeed = 2.0F;
        break;
    case SettingsControl::Zoom75:
        m_settings.zoomFactor = 0.75F;
        break;
    case SettingsControl::Zoom100:
        m_settings.zoomFactor = 1.0F;
        break;
    case SettingsControl::Zoom125:
        m_settings.zoomFactor = 1.25F;
        break;
    case SettingsControl::Zoom150:
        m_settings.zoomFactor = 1.5F;
        break;
    case SettingsControl::ShowSyntaxErrors:
        m_settings.showSyntaxErrors = !m_settings.showSyntaxErrors;
        break;
    case SettingsControl::ShowLineNumbers:
        m_settings.showLineNumbers = !m_settings.showLineNumbers;
        break;
    case SettingsControl::HighlightCurrentLine:
        m_settings.highlightCurrentLine = !m_settings.highlightCurrentLine;
        break;
    case SettingsControl::HighContrast:
        m_settings.highContrast = !m_settings.highContrast;
        break;
    case SettingsControl::CursorBlink:
        m_settings.cursorBlink = !m_settings.cursorBlink;
        break;
    case SettingsControl::CursorLine:
        m_settings.cursorStyle = editor::CursorStyle::Line;
        break;
    case SettingsControl::CursorBlock:
        m_settings.cursorStyle = editor::CursorStyle::Block;
        break;
    case SettingsControl::CursorUnderscore:
        m_settings.cursorStyle = editor::CursorStyle::Underscore;
        break;
    case SettingsControl::CursorKitty:
        m_settings.cursorStyle = editor::CursorStyle::Kitty;
        break;
    case SettingsControl::BackgroundMotion:
        m_settings.backgroundMotion = !m_settings.backgroundMotion;
        break;
    case SettingsControl::BackgroundColorMode:
        m_settings.backgroundColorMode = m_settings.backgroundColorMode == "accent" ? "natural" : "accent";
        break;
    case SettingsControl::StartupSession:
        m_settings.sessionRestore = m_settings.sessionRestore == "ask" ? "restore" : m_settings.sessionRestore == "restore" ? "empty" : "ask";
        break;
    case SettingsControl::BackgroundScene:
        {
            const auto current = std::find(backgroundChoices.begin(), backgroundChoices.end(), m_settings.backgroundScene);
            m_settings.backgroundScene = current == backgroundChoices.end() || std::next(current) == backgroundChoices.end()
                ? backgroundChoices.front() : *std::next(current);
        }
        break;
    case SettingsControl::AiSettings:
    case SettingsControl::OpenSettingsJson:
    case SettingsControl::Done:
    case SettingsControl::None:
        return;
    default:
        return;
    }

    applySettings(m_settings);
    persistSettings();
}

bool Application::persistSettings() {
    if (std::getenv("LUCID_SETTINGS_READ_ONLY")) {
        setStatus("Settings changed for this session", false);
        return true;
    }
    if (m_settingsRoot.empty()) {
        setStatus("Settings are unavailable", true);
        return false;
    }

    if (m_hasDocument && isSettingsDocument()) {
        const auto parsed = parseSettings(m_document.toUtf8());
        if (!parsed.valid) {
            setStatus("Cannot save settings while settings.json is invalid: " + parsed.error, true);
            return false;
        }
        if (!saveDocument()) return false;
    }

    const std::string text = serializeSettings(m_settings);
    std::string error; bool created = false;
    auto settingsProject = workspace::Project::openOrCreate(m_settingsRoot, created, error);
    if (!error.empty()) {
        setStatus("Could not safely write settings.json: " + error, true);
        return false;
    }
    if (m_settingsBaselineRoot != m_settingsRoot || !m_settingsBaseline) {
        setStatus("Cannot save settings: reload settings.json first", true);
        return false;
    }
    std::string current;
    if (!settingsProject.readFile("settings.json", current, error) || current != *m_settingsBaseline) {
        setStatus(isSettingsDocument()
            ? "Settings changed outside Lucid; reopen settings.json to reload it before saving"
            : "Settings changed outside Lucid; reopen Settings to reload them before saving", true);
        return false;
    }
    if (!settingsProject.writeFile("settings.json", text, error, &*m_settingsBaseline)) {
        setStatus("Could not safely write settings.json: " + error, true);
        return false;
    }
    m_settingsBaseline = text;

    if (m_hasDocument && isSettingsDocument()) {
        if (m_document.toUtf8() != text) {
            const auto line = m_document.cursorLine(), column = m_document.cursorColumn();
            m_document = editor::Document::fromUtf8(text);
            m_document.setCursor(line, column);
            m_followCursor = false;
            m_syntaxValid = false;
            m_accessibilityKey.clear();
        } else {
            m_document.markSaved();
        }
        m_diskContents = text;
        m_settingsRevision = m_document.revision();
        m_settingsError.clear();
    }
    m_settingsError.clear();
    setStatus("Settings saved", false);
    return true;
}

bool Application::isJsonDocument() const {
    std::string extension =
        std::filesystem::path(m_filePath).extension().string();
    for (char& character : extension) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    }
    return extension == ".json";
}

bool Application::isCodeDocument() const {
    std::string extension =
        std::filesystem::path(m_filePath).extension().string();
    for (char& character : extension) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    }
    return extension == ".c" || extension == ".cc" || extension == ".cpp" ||
           extension == ".cxx" || extension == ".h" || extension == ".hh" ||
           extension == ".hpp" || extension == ".hxx";
}

bool Application::isSettingsDocument() const {
    if (!m_hasProject || !m_hasDocument || m_settingsRoot.empty()) {
        return false;
    }
    return absoluteDocumentPath() ==
           (std::filesystem::path(m_settingsRoot) / "settings.json")
               .generic_string();
}

std::string Application::absoluteDocumentPath() const {
    if (!m_hasProject || m_filePath.empty()) {
        return {};
    }
    return (m_project.root() / m_filePath).generic_string();
}

ui::SettingsErrorLayout Application::settingsErrorLayout() const {
    int width = 0;
    int height = 0;
    uiSize(width, height);
    return ui::settingsErrorLayout(static_cast<float>(width),
                                   static_cast<float>(height));
}

ui::SettingsPanelLayout Application::settingsPanelLayout() const {
    int width = 0;
    int height = 0;
    uiSize(width, height);
    return ui::settingsPanelLayout(static_cast<float>(width),
                                   static_cast<float>(height));
}

ui::SettingsControl Application::settingsControlAt(double x, double y) const {
    return ui::settingsControlAt(settingsPanelLayout(), x, y);
}

ui::DiagnosticCardLayout Application::diagnosticCardLayout() const {
    int width = 0;
    int height = 0;
    uiSize(width, height);

    const ui::DiagnosticCardLayout fallback = ui::offsetDiagnosticCard(
        ui::diagnosticCardLayout(static_cast<float>(width),
                                 static_cast<float>(height)),
        m_diagnosticOffsetX, m_diagnosticOffsetY);
    if (m_activeDiagnosticKey.empty() || !m_hasDocument) {
        return fallback;
    }

    const int row = m_activeDiagnosticLine - m_firstLine;
    const int rows = std::max(
        1, static_cast<int>(std::floor(
               (static_cast<float>(height) - ui::theme::kEditorTop -
                ui::theme::kEditorBottom) /
               std::max(1.0F, ui::theme::kEditorLineHeight))));
    if (row < 0 || row >= rows) {
        return fallback;
    }

    const float lineTop =
        ui::theme::kEditorTop +
        static_cast<float>(row) * ui::theme::kEditorLineHeight;
    const float lineBottom = lineTop + ui::theme::kEditorLineHeight;
    return ui::offsetDiagnosticCard(
        ui::diagnosticCardLayoutAt(static_cast<float>(width),
                                   static_cast<float>(height), diagnosticAnchorX(),
                                   lineTop, lineBottom),
        m_diagnosticOffsetX, m_diagnosticOffsetY);
}

void Application::uiSize(int& width, int& height) const {
    m_window.windowSize(width, height);
    const float zoom = m_uiZoom > 0.0F ? m_uiZoom : 1.0F;
    width = std::max(1, static_cast<int>(std::lround(width / zoom)));
    height = std::max(1, static_cast<int>(std::lround(height / zoom)));
}

void Application::updateVisibleLines() {
    m_visibleLines.clear();
    m_visibleTokens.clear();

    if (!m_hasDocument) {
        m_firstLine = 0;
        return;
    }

    const int rows = visibleRowCount();
    const int lineCount = static_cast<int>(m_document.lineCount());
    const int cursorLine = static_cast<int>(m_document.cursorLine());

    if (m_followCursor) {
        if (cursorLine < m_firstLine) {
            m_firstLine = cursorLine;
        }
        if (cursorLine >= m_firstLine + rows) {
            m_firstLine = cursorLine - rows + 1;
        }
        m_followCursor = false;
    }
    m_firstLine = std::clamp(m_firstLine, 0, std::max(0, lineCount - 1));

    const int lastLine = std::min(lineCount, m_firstLine + rows);
    const std::size_t visibleCount =
        static_cast<std::size_t>(std::max(0, lastLine - m_firstLine));
    m_visibleLines.reserve(visibleCount);
    m_visibleTokens.reserve(visibleCount);
    for (int line = m_firstLine; line < lastLine; ++line) {
        const std::size_t lineIndex = static_cast<std::size_t>(line);
        m_visibleLines.push_back(m_document.line(lineIndex));
        if (lineIndex < m_syntax.size()) {
            m_visibleTokens.push_back(
                std::span<const editor::TokenSpan>(m_syntax[lineIndex]));
        } else {
            m_visibleTokens.emplace_back();
        }
    }
}

void Application::updateSuggestionAnimation(double deltaSeconds) {
    const float targetRow =
        m_selectedSuggestion >= 0 ? static_cast<float>(m_selectedSuggestion) : -1.0F;
    const float targetOpacity = m_selectedSuggestion >= 0 ? 1.0F : 0.0F;
    const float delta = std::clamp(static_cast<float>(deltaSeconds), 0.0F, 0.05F) *
                        std::clamp(m_animationSpeed, 0.25F, 4.0F);
    const auto springStep = [](float& value, float& velocity, float target,
                               float stepDelta) {
        constexpr float omega = 26.0F;
        constexpr float damping = 1.0F;
        constexpr float maxStep = 1.0F / 240.0F;
        float remaining = stepDelta;
        while (remaining > 0.0F) {
            const float step = std::min(remaining, maxStep);
            const float acceleration =
                omega * omega * (target - value) -
                2.0F * damping * omega * velocity;
            velocity += acceleration * step;
            value += velocity * step;
            remaining -= step;
        }
    };

    if (targetRow >= 0.0F) {
        if (m_suggestionSelectionRow < 0.0F) {
            m_suggestionSelectionRow = targetRow;
            m_suggestionSelectionRowVelocity = 0.0F;
        } else {
            springStep(m_suggestionSelectionRow, m_suggestionSelectionRowVelocity,
                       targetRow, delta);
        }
    }

    springStep(m_suggestionSelectionOpacity, m_suggestionSelectionOpacityVelocity,
               targetOpacity, delta);
    m_suggestionSelectionOpacity =
        std::clamp(m_suggestionSelectionOpacity, 0.0F, 1.0F);

    if (targetOpacity == 0.0F && m_suggestionSelectionOpacity < 0.01F &&
        std::abs(m_suggestionSelectionOpacityVelocity) < 0.02F) {
        m_suggestionSelectionRow = -1.0F;
        m_suggestionSelectionOpacity = 0.0F;
        m_suggestionSelectionRowVelocity = 0.0F;
        m_suggestionSelectionOpacityVelocity = 0.0F;
    }
}

void Application::refreshMenuViewport() {
    if (m_searchBar.isOpen()) {
        int width = 0, height = 0;
        uiSize(width, height);
        const bool changed = m_menuViewportValid && (width != m_menuViewportWidth || height != m_menuViewportHeight);
        m_menuViewportWidth = width;
        m_menuViewportHeight = height;
        m_menuViewportValid = true;
        if (changed) {
            m_menuReselectionRequired = m_selectedSuggestion >= 0;
            m_selectedSuggestion = -1;
            m_suggestionSelectionRow = -1.0F;
            m_suggestionSelectionOpacity = 0.0F;
            m_suggestionSelectionRowVelocity = 0.0F;
            m_suggestionSelectionOpacityVelocity = 0.0F;
            refreshSuggestions();
        } else if (m_searchBar.kind() == ui::PromptKind::Ai && m_aiReady) {
            refreshSuggestions();
        }
    } else { m_menuViewportValid = false; m_menuReselectionRequired = false; }
}

ui::FrameState Application::makeFrameState(double timeSeconds) {
    updateCompileOutput();
    refreshMenuViewport();
    const double deltaSeconds =
        m_lastTimeSeconds > 0.0
            ? std::clamp(timeSeconds - m_lastTimeSeconds, 0.0, 0.05)
            : 0.0;
    m_lastTimeSeconds = timeSeconds;

    if (m_hasDocument) {
        m_statusPosition = std::to_string(m_document.cursorLine() + 1) + ":" +
                           std::to_string(m_document.cursorColumn() + 1);
    }
    const float statusTarget = m_hasDocument ? 1.0F : 0.0F;
    const float statusStep = 1.0F - std::exp(-9.0F * static_cast<float>(deltaSeconds) *
                                             std::clamp(m_animationSpeed, 0.25F, 4.0F));
    m_statusPositionReveal += (statusTarget - m_statusPositionReveal) * statusStep;
    if (std::abs(statusTarget - m_statusPositionReveal) < 0.001F)
        m_statusPositionReveal = statusTarget;

    bool moreAbove = false;
    bool moreBelow = false;
    if (m_searchBar.isOpen()) {
        if (m_searchBar.kind() == ui::PromptKind::File || m_searchBar.kind() == ui::PromptKind::Project || m_searchBar.kind() == ui::PromptKind::OpenFiles || m_searchBar.kind() == ui::PromptKind::Function || m_searchBar.kind() == ui::PromptKind::SettingsMenu || m_searchBar.kind() == ui::PromptKind::AiSettings) {
            moreAbove = m_suggestionScroll > 0;
            moreBelow = m_suggestionHasMoreBelow;
        } else if (isAdaptivePagedPrompt(m_searchBar.kind())) {
            moreAbove = m_pagedScroll > 0;
            moreBelow = m_suggestionHasMoreBelow;
            for (const auto& suggestion : m_suggestions) {
                moreAbove |= suggestion.path == "< Previous page";
                moreBelow |= suggestion.path == "Next page >";
            }
        } else if (m_searchBar.kind() == ui::PromptKind::Ai && m_aiReady) {
            moreAbove = m_aiScroll > 0;
            moreBelow = m_aiScroll + visibleSuggestionCount() < m_aiLines.size();
        } else {
            for (const auto& suggestion : m_suggestions) {
                moreAbove |= suggestion.path == "< Previous page";
                moreBelow |= suggestion.path == "Next page >";
            }
        }
    }
    const float cueStep = 1.0F - std::exp(-10.0F * static_cast<float>(deltaSeconds) * std::clamp(m_animationSpeed, 0.25F, 4.0F));
    m_suggestionMoreAbove += ((moreAbove ? 1.0F : 0.0F) - m_suggestionMoreAbove) * cueStep;
    m_suggestionMoreBelow += ((moreBelow ? 1.0F : 0.0F) - m_suggestionMoreBelow) * cueStep;

    const float backgroundTarget = m_hasDocument ? 1.0F : 0.0F;
    const float backgroundStep =
        1.0F - std::exp(-10.0F * static_cast<float>(deltaSeconds) *
                        std::clamp(m_animationSpeed, 0.25F, 4.0F));
    m_backgroundDim += (backgroundTarget - m_backgroundDim) * backgroundStep;
    if (std::abs(backgroundTarget - m_backgroundDim) < 0.001F) {
        m_backgroundDim = backgroundTarget;
    }

    constexpr double animationPeriod = 200.0 * 3.14159265358979323846;
    m_animationTimeSeconds +=
        deltaSeconds * std::clamp(m_animationSpeed, 0.25F, 4.0F);
    const float shortcutTarget = m_welcomeShortcutsExpanded ? 1.0F : 0.0F;
    const float shortcutStep = 1.0F - std::exp(-13.0F * static_cast<float>(deltaSeconds) *
                                               std::clamp(m_animationSpeed, 0.25F, 4.0F));
    m_welcomeShortcutProgress += (shortcutTarget - m_welcomeShortcutProgress) * shortcutStep;
    if (std::abs(shortcutTarget - m_welcomeShortcutProgress) < 0.001F)
        m_welcomeShortcutProgress = shortcutTarget;

    ui::FrameState state =
        m_searchBar.frameState(timeSeconds, m_animationSpeed);
    state.animationTime =
        static_cast<float>(std::fmod(m_animationTimeSeconds, animationPeriod));
    switch (m_searchBar.kind()) {
    case ui::PromptKind::Ai: if (m_aiCleanup) state.commandPlaceholder = "Clean selected code with AI..."; break;
    case ui::PromptKind::AiSettings: state.commandPlaceholder = "AI settings: choose a workflow or key..."; break;
    case ui::PromptKind::ProjectSearch: state.commandPlaceholder = "Search project (case-sensitive)..."; break;
    case ui::PromptKind::ProjectSearchResults: state.commandPlaceholder = "Project search results..."; break;
    case ui::PromptKind::ProjectReplace: state.commandPlaceholder = "Replace matches with..."; break;
    case ui::PromptKind::ProjectReplaceReview: state.commandPlaceholder = "Review project replacement..."; break;
    case ui::PromptKind::LanguageLocations: state.commandPlaceholder = "Language locations..."; break;
    case ui::PromptKind::Diagnostics: state.commandPlaceholder = "Diagnostics in current file..."; break;
    case ui::PromptKind::SettingsMenu: state.commandPlaceholder = "Settings: choose an option..."; break;
    case ui::PromptKind::SessionRestore: state.commandPlaceholder = "Restore your last workspace?"; break;
    case ui::PromptKind::AiModelInput: state.commandPlaceholder = "OpenRouter model ID (e.g. provider/model)..."; break;
    case ui::PromptKind::AiKeyInput: state.commandPlaceholder = "Paste OpenRouter API key; Enter saves securely..."; break;
    case ui::PromptKind::DebugMenu: state.commandPlaceholder = "Debugger..."; break;
    case ui::PromptKind::DebugInput: state.commandPlaceholder = m_debugAwaitArguments ? "Arguments as JSON array..." : m_debugAwaitWorkingDirectory ? "Working directory inside project..." : m_debugAwaitEnvironment ? "Environment as JSON object..." : "Executable path inside project..."; break;
    case ui::PromptKind::DebugStack: state.commandPlaceholder = "Debug call stack..."; break;
    case ui::PromptKind::DebugThreads: state.commandPlaceholder = "Debug threads..."; break;
    case ui::PromptKind::DebugVariables: state.commandPlaceholder = "Debug variables..."; break;
    case ui::PromptKind::DebugOutput: state.commandPlaceholder = "Debug output..."; break;
    case ui::PromptKind::RecoveryList: state.commandPlaceholder = "Recovery snapshots..."; break;
    case ui::PromptKind::RecoveryDetail: state.commandPlaceholder = "Review snapshot before restoring..."; break;
    case ui::PromptKind::ModuleList: state.commandPlaceholder = "Modules..."; break;
    case ui::PromptKind::ModuleActions: state.commandPlaceholder = "Module details and permissions..."; break;
    case ui::PromptKind::ModuleCommands: state.commandPlaceholder = "Module commands..."; break;
    case ui::PromptKind::ModuleImport: state.commandPlaceholder = "Module file path or GitHub package..."; break;
    case ui::PromptKind::ModulePreview: state.commandPlaceholder = "Review module before installing..."; break;
    case ui::PromptKind::ModuleCatalog: state.commandPlaceholder = "Search GitHub modules..."; break;
    case ui::PromptKind::GitChanges: state.commandPlaceholder = "Git review: changed files..."; break;
    case ui::PromptKind::GitFile: state.commandPlaceholder = "Git: file actions..."; break;
    case ui::PromptKind::GitDiff: state.commandPlaceholder = "Git review: select a line to open..."; break;
    case ui::PromptKind::GitCommitReview: state.commandPlaceholder = "Review staged files before committing..."; break;
    case ui::PromptKind::GitCommitInput: state.commandPlaceholder = "Commit message; Enter creates a local commit..."; break;
    case ui::PromptKind::GitRemoteReview: state.commandPlaceholder = "Review Git remote operation..."; break;
    case ui::PromptKind::GitHistory: state.commandPlaceholder = "Recent commits..."; break;
    case ui::PromptKind::TaskList: state.commandPlaceholder = "Tasks: search or create..."; break;
    case ui::PromptKind::TaskActions: state.commandPlaceholder = "Task actions..."; break;
    case ui::PromptKind::TaskSubtasks: state.commandPlaceholder = "Subtasks..."; break;
    case ui::PromptKind::TaskHistory: state.commandPlaceholder = "Task activity..."; break;
    case ui::PromptKind::TaskTier: state.commandPlaceholder = "Choose task tier..."; break;
    case ui::PromptKind::TaskInput:
        switch (m_taskInputKind) {
        case TaskInputKind::Identity: state.commandPlaceholder = "Your display name..."; break;
        case TaskInputKind::Title: state.commandPlaceholder = "New task title..."; break;
        case TaskInputKind::Subtask: state.commandPlaceholder = "New subtask title..."; break;
        case TaskInputKind::Assignee: state.commandPlaceholder = "Assign to... (empty clears)"; break;
        case TaskInputKind::Solution: state.commandPlaceholder = "How was it solved?"; break;
        }
        break;
    default: break;
    }

    state.completionVisible = m_completionOpen && !m_searchBar.isOpen();
    if (state.completionVisible) {
        for (const auto& completion : m_completions) state.completions.push_back(completion);
        state.selectedCompletion = m_completionSelected;
    }

    pollAi();
    pollLanguageServer();
    pollProjectSearch();
    pollModules();
    pollProjectIndex();
    pollFunctionIndex();
    pollGitRemote();
    pollCompilerDiagnostics(timeSeconds);
    refreshSettings();
    refreshDiagnostics(timeSeconds);

    state.animationSpeed = m_animationSpeed;
    state.hasProject = m_hasProject;
    state.welcomeShortcutScroll = m_welcomeShortcutScroll;
    state.welcomeShortcutsExpanded = m_welcomeShortcutsExpanded;
    state.welcomeShortcutProgress = m_welcomeShortcutProgress;
    const float entrance = std::clamp(static_cast<float>(m_animationTimeSeconds / 0.8), 0.0F, 1.0F);
    state.startupProgress = entrance * entrance * (3.0F - 2.0F * entrance);
    state.welcomeActionHovered = m_welcomeActionHovered && !state.commandVisible;
    state.projectName = m_projectName;
    state.hasDocument = m_hasDocument;
    state.statusPosition = m_statusPosition;
    state.statusPositionReveal = m_statusPositionReveal;
    state.filePath = m_filePath;
    state.dirty = m_hasDocument && m_document.dirty();
    state.showLineNumbers = m_showLineNumbers;
    state.highlightCurrentLine = m_highlightCurrentLine;
    state.cursorBlink = m_cursorBlink;
    state.cursorStyle = m_cursorStyle;
    state.backgroundMotion = m_backgroundMotion;
    state.backgroundScene = m_settings.backgroundScene;
    state.backgroundNaturalColors = m_settings.backgroundColorMode == "natural";
    // A transient override allows visual smoke tests without changing the
    // user's saved background preference.
    if (const char* sceneOverride = std::getenv("LUCID_BACKGROUND_SCENE");
        m_allowBackgroundSceneOverride && sceneOverride && sceneOverride[0] >= '0' && sceneOverride[0] <= '9' && sceneOverride[1] == '\0')
        state.backgroundScene = sceneOverride[0] - '0';
    state.backgroundScene = availableBackgroundScene(state.backgroundScene);
    state.backgroundDim = m_backgroundDim;

    std::string windowTitle(kAppDisplayName);
    if (m_hasDocument) {
        std::string filename = std::filesystem::path(m_filePath).filename().string();
        if (filename.empty()) {
            filename = m_filePath;
        }
        windowTitle += " - " + filename;
        if (state.dirty) {
            windowTitle += "*";
        }
    }
    if (windowTitle != m_windowTitle) {
        m_windowTitle = std::move(windowTitle);
        m_window.setTitle(m_windowTitle);
    }
    state.windowTitle = m_windowTitle;
#if defined(__APPLE__)
    state.showWindowTitle = true;
#endif

    updateSuggestionAnimation(deltaSeconds);
    updateCompileTerminalAnimation(deltaSeconds);
    state.suggestions = m_suggestions;
    state.selectedSuggestion = m_selectedSuggestion;
    state.suggestionEpoch = m_suggestionEpoch;
    state.suggestionSelectionRow = m_suggestionSelectionRow;
    state.suggestionSelectionOpacity = m_suggestionSelectionOpacity;
    state.suggestionMoreAbove = m_suggestionMoreAbove;
    state.suggestionMoreBelow = m_suggestionMoreBelow;

    state.settingsErrorVisible = !m_settingsError.empty() && !state.commandVisible;
    state.settingsErrorHovered = m_settingsErrorHovered && !state.commandVisible;
    state.settingsErrorText = m_settingsError;

    state.settingsPanelVisible = m_settingsPanelVisible && !state.commandVisible;
    state.settingsHovered =
        state.settingsPanelVisible ? m_settingsHovered : ui::SettingsControl::None;
    state.settingsTheme = m_settings.theme.name;
    state.settingsIconTheme = m_settings.iconTheme;
    state.settingsPrimary = ui::theme::kPrimary;
    state.settingsAnimationSpeed = m_settings.animationSpeed;
    state.settingsZoomFactor = m_settings.zoomFactor;
    state.settingsShowSyntaxErrors = m_settings.showSyntaxErrors;
    state.settingsShowLineNumbers = m_settings.showLineNumbers;
    state.settingsHighlightCurrentLine = m_settings.highlightCurrentLine;
    state.settingsCursorBlink = m_settings.cursorBlink;
    state.settingsCursorStyle = m_settings.cursorStyle;
    state.settingsBackgroundMotion = m_settings.backgroundMotion;
    state.settingsFileAvailable = !m_settingsRoot.empty();

    state.diagnosticsVisible =
        m_showDiagnostics && m_hasDocument && !m_searchBar.visible() &&
        !m_settingsPanelVisible && !m_activeDiagnosticKey.empty();
    state.diagnosticMessage = m_activeDiagnosticMessage;
    state.diagnosticCode = m_activeDiagnosticCode;
    state.diagnosticLabel = m_diagnosticLabel;
    state.diagnosticLine = m_activeDiagnosticLine;
    state.diagnosticColumn = m_activeDiagnosticColumn;
    state.diagnosticLength = m_activeDiagnosticLength;
    state.diagnosticIgnoreHovered = m_diagnosticIgnoreHovered;
    state.diagnosticOffsetX = m_diagnosticOffsetX;
    state.diagnosticOffsetY = m_diagnosticOffsetY;

    state.commandStatus = m_commandStatus;
    state.commandStatusIsError = m_commandStatusIsError;
    state.commandStatusColumns = commandStatusColumns();
    state.commandStatusRows = commandStatusRows();
    state.statusText = m_statusText;
    state.statusIsError = m_statusIsError;

    refreshHighlight();
    refreshCompileScript();
    updateVisibleLines();
    state.visibleLines = m_visibleLines;
    state.visibleTokens = m_visibleTokens;
    state.firstLine = m_firstLine;

    state.debugBreakpointLines.clear();
    if (auto found = m_debugBreakpoints.find(m_filePath); m_hasDocument && found != m_debugBreakpoints.end())
        for (int line : found->second) state.debugBreakpointLines.push_back(line - 1);
    state.debugStoppedLine = -1; m_debugInline.clear();
    if (m_hasProject && m_hasDocument && m_debugSession.state() == debug::State::Stopped && !m_debugSession.stack().empty()) {
        const auto& top = m_debugSession.stack()[std::min(m_debugSession.selectedFrame(), m_debugSession.stack().size() - 1)];
        std::error_code ec; const auto file = std::filesystem::weakly_canonical(m_project.root() / m_filePath, ec);
        if (!ec && file == std::filesystem::weakly_canonical(top.path, ec) && !ec) {
            state.debugStoppedLine = top.line - 1;
            const auto appendInline = [this](std::string_view text) {
                constexpr std::size_t limit = 180;
                if (m_debugInline.size() >= limit) return;
                auto count = std::min(text.size(), limit - m_debugInline.size());
                while (count > 0 && count < text.size() && (static_cast<unsigned char>(text[count]) & 0xc0U) == 0x80U) --count;
                m_debugInline.append(text.substr(0, count));
            };
            for (const auto& variable : m_debugSession.variables()) {
                if (!m_debugInline.empty()) appendInline("  ");
                if (variable.changed) appendInline("* ");
                appendInline(variable.name); appendInline(" = "); appendInline(variable.value);
                if (m_debugInline.size() >= 180) break;
            }
        }
    }
    state.debugInline = m_debugInline;
    state.compileFunctionLines.clear();
    if (m_hasDocument && m_compileScriptValid) {
        const int lastLine = m_firstLine + visibleRowCount();
        for (const editor::CompileFunction& function : m_compileScript.functions) {
            if (function.runsHere() && function.definitionLine >= m_firstLine &&
                function.definitionLine < lastLine) {
                state.compileFunctionLines.push_back(function.definitionLine);
            }
        }
    }

    state.compileTerminalVisible =
        m_compileTerminalVisible && m_compileTerminalWindow == nullptr &&
        !state.commandVisible && !m_settingsPanelVisible &&
        !m_compileTerminalTitle.empty();
    state.compileTerminalFocused = m_compileTerminalFocused;
    state.compileTerminalRunning =
        m_compileTerminalProcess.alive() && !m_compileTerminalProcess.commandsDone();
    state.compileTerminalFailed = std::any_of(m_compileTerminalProcess.exitCodes().begin(), m_compileTerminalProcess.exitCodes().end(), [](int code) { return code != 0; });
    state.compileTerminalCloseHovered = m_compileTerminalCloseHovered;
    state.compileTerminalOpacity =
        (m_compileTerminalFocused ? 1.0F : 0.5F) *
        std::clamp(m_compileTerminalReveal, 0.0F, 1.0F);
    state.compileTerminalReveal = m_compileTerminalReveal;
    state.compileTerminalOffsetX = m_compileTerminalOffsetX;
    state.compileTerminalOffsetY = m_compileTerminalOffsetY;
    state.compileTerminalLine = m_compileTerminalLine;
    state.compileOutputScroll = m_compileOutputScroll;
    state.compileTerminalTitle = m_compileTerminalTitle;
    state.compileTerminalOutput = m_compileTerminalProcess.output();
    if (state.compileTerminalOutput.starts_with(m_compileSelectionSnapshot)) {
        state.compileSelectionStart = std::min(m_compileSelectionAnchor, m_compileSelectionEnd);
        state.compileSelectionEnd = std::max(m_compileSelectionAnchor, m_compileSelectionEnd);
    }
    state.compileTerminalInput = m_compileTerminalInput;

    if (state.diagnosticsVisible && m_activeDiagnosticLine >= m_firstLine) {
        int viewportWidth = 0;
        int viewportHeight = 0;
        uiSize(viewportWidth, viewportHeight);
        const int row = m_activeDiagnosticLine - m_firstLine;
        const int rows = std::max(
            1, static_cast<int>(std::floor(
                   (static_cast<float>(viewportHeight) - ui::theme::kEditorTop -
                    ui::theme::kEditorBottom) /
                   std::max(1.0F, ui::theme::kEditorLineHeight))));
        if (row < rows) {
            state.diagnosticAnchorValid = true;
            state.diagnosticAnchorX = diagnosticAnchorX();
            state.diagnosticLineTop =
                ui::theme::kEditorTop +
                static_cast<float>(row) * ui::theme::kEditorLineHeight;
            state.diagnosticLineBottom =
                state.diagnosticLineTop + ui::theme::kEditorLineHeight;
        }
    }

    if (m_hasDocument) {
        const int cursorLine = static_cast<int>(m_document.cursorLine());
        state.cursorLine = cursorLine;
        state.cursorColumn = static_cast<int>(m_document.cursorColumn());
        state.cursorRow = cursorLine - m_firstLine;
        state.hasSelection = m_document.hasSelection();
        if (state.hasSelection) {
            const editor::TextPosition start = m_document.selectionStart();
            const editor::TextPosition end = m_document.selectionEnd();
            state.selectionStartLine = static_cast<int>(start.line);
            state.selectionStartColumn = static_cast<int>(start.column);
            state.selectionEndLine = static_cast<int>(end.line);
            state.selectionEndColumn = static_cast<int>(end.column);
        }
    }

    return state;
}

std::string Application::commandPath() const {
    if (m_selectedSuggestion >= 0 &&
        m_selectedSuggestion < static_cast<int>(m_suggestions.size())) {
        return std::string(
            m_suggestions[static_cast<std::size_t>(m_selectedSuggestion)].path);
    }
    return normalizeRelative(m_searchBar.query());
}

void Application::setCommandStatus(std::string message, bool error) {
    const auto oldRows = commandStatusRows();
    const auto selectedIndex = m_selectedSuggestion < 0 ? std::optional<std::size_t>{} :
        std::optional<std::size_t>{m_suggestionScroll + static_cast<std::size_t>(m_selectedSuggestion)};
    m_commandStatus = std::move(message);
    m_commandStatusIsError = error;
    if (m_searchBar.isOpen() && oldRows != commandStatusRows()) {
        const auto kind = m_searchBar.kind();
        if (kind == ui::PromptKind::File || kind == ui::PromptKind::Project || kind == ui::PromptKind::OpenFiles ||
            kind == ui::PromptKind::Function || kind == ui::PromptKind::SettingsMenu || kind == ui::PromptKind::AiSettings) {
            if (selectedIndex) selectSuggestionRow(*selectedIndex); else refreshSuggestions();
        } else if (kind == ui::PromptKind::Ai && m_aiReady) refreshSuggestions();
        else if (kind == ui::PromptKind::RecoveryList || kind == ui::PromptKind::RecoveryDetail ||
                 kind == ui::PromptKind::ModuleList || kind == ui::PromptKind::ModuleActions || kind == ui::PromptKind::ModuleCommands ||
                 kind == ui::PromptKind::DebugMenu || kind == ui::PromptKind::DebugStack || kind == ui::PromptKind::DebugThreads || kind == ui::PromptKind::DebugVariables || kind == ui::PromptKind::DebugOutput ||
                 kind == ui::PromptKind::GitChanges || kind == ui::PromptKind::GitFile || kind == ui::PromptKind::GitDiff ||
                 kind == ui::PromptKind::GitCommitReview || kind == ui::PromptKind::GitRemoteReview || kind == ui::PromptKind::GitHistory || isTaskPrompt(kind)) {
            m_selectedSuggestion = -1;
            refreshSuggestions();
        }
    }
}

void Application::setStatus(std::string message, bool error) {
    m_statusText = std::move(message);
    m_statusIsError = error;
}

} // namespace lucid
