#include "app/Application.hpp"

#include "core/Version.hpp"
#include "editor/compile/CompileScript.hpp"
#include "editor/diagnostics/Diagnostics.hpp"
#include "editor/json/Json.hpp"
#include "logging/Log.hpp"
#include "ui/Overlay.hpp"
#include "ui/Welcome.hpp"
#include "ui/theme/Theme.hpp"
#include "workspace/PathSearch.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace lucid {
namespace {

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
    log::info(std::string(kAppName) + " " + std::to_string(kVersionMajor) + "." +
              std::to_string(kVersionMinor) + "." + std::to_string(kVersionPatch));

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

    platform::configureApplicationWindow(m_window.handle(), m_config.window.title);
    platform::installApplicationMenu({
        .openProject = [this]() { openProjectPrompt(); },
        .openFile = [this]() { openFilePrompt(); },
        .save = [this]() { (void)saveDocument(); },
        .closeFile = [this]() { closeDocument(); },
        .deleteFile = [this]() { deleteCurrentFile(); },
        .deleteProject = [this]() { deleteCurrentProject(); },
        .settings = [this]() { openSettings(); },
        .runCompile = [this]() { runCompileAtCursor(); },
        .runFunction = [this]() { openFunctionPrompt(); },
        .closeTerminal = [this]() { closeCompileTerminal(); },
        .undo = [this]() { undoDocument(); },
        .redo = [this]() { redoDocument(); },
        .cut = [this]() { cutSelection(); },
        .copy = [this]() { copySelection(); },
        .paste = [this]() { pasteClipboard(); },
        .selectAll = [this]() { selectAllText(); },
    });

    m_renderer = std::make_unique<Renderer>(m_window, m_config.renderer);
    initializeSettings();
    m_renderer->setUiZoom(m_uiZoom);
    m_charAdvance = m_renderer->logicalCharacterAdvance();

    if (const char* demoProject = std::getenv("LUCID_DEMO_MENU")) {
        openProject(demoProject);
        openFilePrompt();
    }
}

Application::~Application() = default;

void Application::run() {
    while (!m_window.shouldClose()) {
        m_window.pollEvents();

        const double timeSeconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
                .count();
        m_renderer->drawFrame(makeFrameState(timeSeconds));

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

    if (!saveDocument()) {
        log::error("Could not save the open document before exit.");
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
        return;
    }

    if (event.action == input::KeyAction::Press && commandDown) {
        switch (event.key) {
        case input::Key::K:
            if (event.shift && m_hasDocument && !m_searchBar.visible()) {
                m_document.deleteLine();
                m_followCursor = true;
            } else {
                openFilePrompt();
            }
            return;
        case input::Key::N:
            openFilePrompt();
            return;
        case input::Key::P:
        case input::Key::O:
            openProjectPrompt();
            return;
        case input::Key::A:
            selectAllText();
            return;
        case input::Key::C:
            copySelection();
            return;
        case input::Key::D:
            if (m_hasDocument && !m_searchBar.visible()) {
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
            redoDocument();
            return;
        case input::Key::V:
            pasteClipboard();
            return;
        case input::Key::Backspace:
        case input::Key::Delete:
            if (m_hasDocument && !m_searchBar.visible()) {
                if (event.shift && event.alt) {
                    deleteCurrentProject();
                } else if (event.shift) {
                    deleteCurrentFile();
                } else {
                    m_document.deleteLine();
                    m_followCursor = true;
                }
            }
            return;
        case input::Key::Enter:
            if (m_hasDocument && !m_searchBar.visible()) {
                if (event.shift) {
                    m_document.insertLineAbove();
                } else {
                    m_document.insertLineBelow();
                }
                m_followCursor = true;
            }
            return;
        case input::Key::Slash:
            if (m_hasDocument && !m_searchBar.visible()) {
                m_document.toggleLineComment();
                m_followCursor = true;
            }
            return;
        case input::Key::Comma:
            if (!m_searchBar.visible()) {
                openSettings();
            }
            return;
        case input::Key::R:
            if (event.shift) {
                openFunctionPrompt();
            } else if (m_hasDocument && !m_searchBar.visible()) {
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

    if (m_searchBar.visible()) {
        handleCommandKey(event, commandDown);
        return;
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
    }
}

void Application::handleCommandKey(const input::KeyEvent& event, bool commandDown) {
    if (event.action == input::KeyAction::Press && event.key == input::Key::Escape) {
        m_searchBar.close();
        return;
    }

    if (!m_searchBar.isOpen()) {
        return;
    }

    refreshSuggestions();

    if (event.action == input::KeyAction::Press && event.key == input::Key::Enter) {
        applyCommand(commandDown);
        return;
    }

    if (event.action == input::KeyAction::Press && event.key == input::Key::Tab) {
        acceptSuggestion();
        return;
    }

    const bool repeatable = event.action == input::KeyAction::Press ||
                            event.action == input::KeyAction::Repeat;
    if (repeatable && event.key == input::Key::Up) {
        moveSuggestion(-1);
        return;
    }
    if (repeatable && event.key == input::Key::Down) {
        moveSuggestion(1);
        return;
    }
    if (repeatable && event.key == input::Key::Backspace) {
        m_searchBar.backspace();
        onCommandQueryChanged();
    }
}

void Application::handleEditorKey(const input::KeyEvent& event, bool commandDown) {
    if (event.action == input::KeyAction::Release) {
        return;
    }

    m_followCursor = true;

    if (event.alt && event.key == input::Key::Up) {
        m_document.moveLineUp();
        return;
    }
    if (event.alt && event.key == input::Key::Down) {
        m_document.moveLineDown();
        return;
    }

    if (commandDown) {
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
            m_document.insertSpaces(kIndentWidth);
        }
        break;
    case input::Key::Backspace:
        m_document.backspace();
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
        m_document.moveHome(event.shift);
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

    if (m_searchBar.visible()) {
        if (m_searchBar.isOpen()) {
            m_searchBar.append(codepoint);
            onCommandQueryChanged();
        }
        return;
    }

    if (m_hasDocument) {
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
}

void Application::undoDocument() {
    if (m_hasDocument && !m_searchBar.visible()) {
        m_document.undo();
        m_followCursor = true;
    }
}

void Application::redoDocument() {
    if (m_hasDocument && !m_searchBar.visible()) {
        m_document.redo();
        m_followCursor = true;
    }
}

void Application::cutSelection() {
    if (!m_hasDocument || m_searchBar.visible()) {
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
    if (!m_hasDocument || m_searchBar.visible()) {
        return;
    }

    std::string text = m_document.selectionUtf8();
    if (text.empty()) {
        text = m_document.lineUtf8(m_document.cursorLine()) + "\n";
    }
    m_window.setClipboardText(text);
}

void Application::pasteClipboard() {
    if (!m_hasDocument || m_searchBar.visible()) {
        return;
    }

    const std::string text = m_window.clipboardText();
    if (!text.empty()) {
        m_document.pasteUtf8(text);
        m_followCursor = true;
    }
}

void Application::selectAllText() {
    if (m_hasDocument && !m_searchBar.visible()) {
        m_document.selectAll();
        m_followCursor = true;
    }
}

void Application::handleMouseButton(const input::MouseButtonEvent& event) {
    if (event.button == input::MouseButton::Left &&
        event.action == input::KeyAction::Release) {
        m_mouseSelecting = false;
        m_window.endWindowDrag();
        endDrag();
        return;
    }

    if (event.action != input::KeyAction::Press ||
        event.button != input::MouseButton::Left) {
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
        } else {
            closeSettingsPanel();
        }
        return;
    }

    if (!m_searchBar.visible() && m_compileTerminalVisible &&
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
            return;
        }
    }

    if (!m_searchBar.visible()) {
        focusCompileTerminal(false);
    }

    if (!m_searchBar.visible() && !m_settingsError.empty() &&
        settingsErrorLayout().text.contains(event.x, event.y)) {
        openSettingsJson();
        return;
    }

    if (m_hasDocument && m_showDiagnostics && !m_searchBar.visible()) {
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

    if (m_searchBar.visible()) {
        if (m_searchBar.isOpen()) {
            refreshSuggestions();
            const int index = suggestionIndexAt(event.x, event.y);
            if (index >= 0) {
                m_selectedSuggestion = index;
                applyCommand(false);
                return;
            }
            if (commandRegionContains(event.x, event.y)) {
                return;
            }
        }
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
}

void Application::handleMouseMove(const input::MouseMoveEvent& event) {
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

    if (m_mouseSelecting && m_hasDocument && !m_searchBar.visible()) {
        (void)placeCaretAt(event.x, event.y, true);
    }

    if (m_searchBar.visible() && m_searchBar.isOpen()) {
        const int index = suggestionIndexAt(event.x, event.y);
        if (index >= 0 && index != m_selectedSuggestion) {
            m_selectedSuggestion = index;
        }
    }

    updateCompileTerminalHover(event.x, event.y);

    m_welcomeActionHovered =
        !m_searchBar.visible() && welcomeActionContains(event.x, event.y);
    m_settingsErrorHovered =
        !m_searchBar.visible() && !m_settingsError.empty() &&
        settingsErrorLayout().text.contains(event.x, event.y);

    m_diagnosticIgnoreHovered = false;
    if (m_showDiagnostics && !m_searchBar.visible() && !m_activeDiagnosticKey.empty()) {
        const ui::DiagnosticCardLayout cardLayout = diagnosticCardLayout();
        m_diagnosticIgnoreHovered =
            cardLayout.ignoreButton.contains(event.x, event.y);
    }

    updateCursorShape(event.x, event.y);
}

void Application::handleScroll(const input::ScrollEvent& event) {
    if (event.yOffset == 0.0 || m_settingsPanelVisible) {
        return;
    }

    if (m_searchBar.visible()) {
        if (m_searchBar.isOpen() && !m_suggestions.empty()) {
            moveSuggestion(event.yOffset > 0.0 ? -1 : 1);
        }
        return;
    }

    if (!m_hasDocument) {
        return;
    }

    const int steps = static_cast<int>(std::lround(event.yOffset * 3.0));
    if (steps == 0) {
        return;
    }

    m_firstLine =
        std::clamp(m_firstLine - steps, 0, maxFirstLine());
    m_followCursor = false;
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
        const int index = suggestionIndexAt(x, y);
        m_window.setCursorShape(index >= 0 ? CursorShape::Hand : CursorShape::Arrow);
        return;
    }

    if (!m_hasProject) {
        m_window.setCursorShape(welcomeActionContains(x, y) ? CursorShape::Hand
                                                            : CursorShape::Arrow);
        return;
    }

    if (!m_hasDocument) {
        m_window.setCursorShape(CursorShape::Arrow);
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
           ui::theme::kSearchBarHeight + ui::theme::kSuggestionGap +
           (m_commandStatus.empty() ? 0.0F : ui::theme::kSuggestionLineHeight);
}

int Application::suggestionIndexAt(double x, double y) const {
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
    }
    refreshSuggestions();
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
    }
    refreshSuggestions();
}

void Application::applyCommand(bool forceFolder) {
    if (!m_searchBar.isOpen()) {
        return;
    }

    if (m_searchBar.kind() == ui::PromptKind::Function) {
        if (!m_hasProject) {
            setCommandStatus("Open a project first", true);
            return;
        }
        ensureFunctionIndex();
        int selected = -1;
        if (m_selectedSuggestion >= 0 &&
            m_selectedSuggestion < static_cast<int>(m_suggestionFunctionIndices.size())) {
            selected = static_cast<int>(
                m_suggestionFunctionIndices[static_cast<std::size_t>(m_selectedSuggestion)]);
        } else {
            const std::vector<std::size_t> matches = workspace::searchFunctions(
                m_functionIndex.functions, m_searchBar.query(), 1);
            if (!matches.empty()) {
                selected = static_cast<int>(matches.front());
            }
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
        m_project.refresh();
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

void Application::openProject(const std::string& path) {
    if (!saveDocument()) {
        setCommandStatus("Could not save the open file", true);
        return;
    }

    bool created = false;
    std::string error;
    workspace::Project project =
        workspace::Project::openOrCreate(resolveProjectPath(path), created, error);
    if (!error.empty()) {
        setCommandStatus(std::move(error), true);
        return;
    }

    m_project = std::move(project);
    m_projectName = m_project.name();
    m_hasProject = true;
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
    m_dragTarget = DragTarget::None;

    setStatus((created ? "Created project " : "Opened project ") + m_projectName, false);
}

void Application::openFile(const std::string& path) {
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

    closeCompileTerminal();
    m_document = editor::Document::fromUtf8(content);
    m_hasDocument = true;
    m_filePath = relative;
    m_firstLine = 0;
    m_followCursor = true;
    m_syntaxValid = false;
    m_compileScriptValid = false;
    resetDiagnostics();
    if (created) {
        m_project.refresh();
    }
    m_searchBar.close();

    setStatus((created ? "Created " : "Opened ") + relative, false);
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
    if (!m_project.writeFile(m_filePath, m_document.toUtf8(), error)) {
        setStatus(std::move(error), true);
        return false;
    }

    m_document.markSaved();
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

    setStatus("Deleted project " + projectName, false);
}

void Application::closeProject() {
    closeCompileTerminal();
    const std::string root = m_project.root().string();
    m_project = workspace::Project{};
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
    setStatus("Closed " + closed, false);
}

void Application::moveSuggestion(int delta) {
    refreshSuggestions();
    if (m_suggestions.empty()) {
        m_selectedSuggestion = -1;
        return;
    }

    const int count = static_cast<int>(m_suggestions.size());
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
    m_selectedSuggestion = -1;
    m_suggestionEpoch = m_lastTimeSeconds;
    m_suggestionSelectionRow = -1.0F;
    m_suggestionSelectionOpacity = 0.0F;
    m_suggestionSelectionRowVelocity = 0.0F;
    m_suggestionSelectionOpacityVelocity = 0.0F;
    m_commandStatus.clear();
    m_commandStatusIsError = false;
    refreshSuggestions();
}

void Application::refreshSuggestions() {
    m_suggestions.clear();
    m_suggestionPaths.clear();
    m_suggestionDetails.clear();
    m_suggestionFunctionIndices.clear();

    if (!m_searchBar.isOpen()) {
        m_selectedSuggestion = -1;
        return;
    }

    const std::size_t capacity =
        static_cast<std::size_t>(ui::theme::kMaxSuggestions);
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
    const auto remaining = [this]() {
        return m_suggestions.size() < capacity ? capacity - m_suggestions.size() : 0;
    };

    if (m_searchBar.kind() == ui::PromptKind::Function) {
        if (!m_hasProject) {
            m_selectedSuggestion = -1;
            return;
        }
        ensureFunctionIndex();
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
                addSuggestion(typed, ui::SuggestionKind::CreateFile);
                if (remaining() > 0) {
                    addSuggestion(typed + "/", ui::SuggestionKind::CreateDirectory);
                }
            }
        }

        std::string directoryQuery = typed;
        while (!directoryQuery.empty() && directoryQuery.back() == '/') {
            directoryQuery.pop_back();
        }

        const std::size_t directoryBudget = std::min<std::size_t>(remaining(), 4);
        for (const std::string_view match :
             workspace::filterPaths(m_project.directories(), directoryQuery,
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
             workspace::filterPaths(m_project.files(), typed, remaining())) {
            addSuggestion(std::string(match), ui::SuggestionKind::File);
        }
    }

    if (m_selectedSuggestion >= static_cast<int>(m_suggestions.size())) {
        m_selectedSuggestion = -1;
    }
}

void Application::onCommandQueryChanged() {
    m_selectedSuggestion = -1;
    m_suggestionEpoch = m_lastTimeSeconds;
    m_suggestionSelectionRow = -1.0F;
    m_suggestionSelectionOpacity = 0.0F;
    m_suggestionSelectionRowVelocity = 0.0F;
    m_suggestionSelectionOpacityVelocity = 0.0F;
    m_commandStatus.clear();
    m_commandStatusIsError = false;
    refreshSuggestions();
}

void Application::clearCommandFeedback() {
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
    if (!m_hasDocument || !m_compileScriptValid || m_searchBar.visible()) {
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
    if (!m_compileTerminalVisible || m_searchBar.visible() ||
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
        config.title = "Lucid - Compile";
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

void Application::handleCompileTerminalMouseButton(
    const input::MouseButtonEvent& event) {
    if (m_compileTerminalWindow == nullptr ||
        event.button != input::MouseButton::Left) {
        return;
    }
    if (event.action == input::KeyAction::Press) {
        if (event.y <= ui::theme::kCompileTerminalTitleHeight &&
            event.x > 80.0) {
            m_compileTerminalWindow->beginWindowDrag();
        }
    } else {
        m_compileTerminalWindow->endWindowDrag();
    }
}

void Application::handleCompileTerminalMouseMove(
    const input::MouseMoveEvent& /*event*/) {
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
    state.compileTerminalOpacity = 1.0F;
    state.compileTerminalReveal = 1.0F;
    state.compileTerminalLine = 0;
    state.compileTerminalTitle = m_compileTerminalTitle;
    state.compileTerminalOutput = m_compileTerminalProcess.output();
    state.compileTerminalInput = m_compileTerminalInput;
    return state;
}

void Application::closeCompileTerminal() {
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
    if (m_searchBar.visible()) {
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
                          : "unknown"));

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
        commands.push_back(editor::expandCompileVariables(command.text, variables));
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
        m_compileTerminalWindow->setTitle("Lucid - " + function.name);
        m_compileTerminalWindow->focus();
    }
    setStatus("Running compile function " + function.name, false);
    log::info("Running compile function " + function.name + " in " + projectRoot);
}

void Application::ensureFunctionIndex() {
    if (!m_hasProject || (m_functionIndexValid && !m_functionIndexDirty)) {
        return;
    }

    std::string error;
    m_functionIndex = workspace::loadFunctionIndex(m_project, error);
    m_functionIndexValid = true;
    m_functionIndexDirty = false;
    if (!error.empty()) {
        log::warn("Function index cache: " + error);
    }
}

void Application::invalidateFunctionIndex() {
    m_functionIndexDirty = true;
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
        openFile(function.file);
        if (!m_hasDocument || m_filePath != function.file) {
            return;
        }
    }

    if (!m_hasDocument) {
        setStatus("No file is open", true);
        return;
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

    editor::CompileFunction fallback;
    fallback.name = function.name;
    fallback.platform = function.platform;
    fallback.definitionLine = function.line;
    fallback.endLine = function.endLine;
    fallback.directiveColumn = function.directiveColumn;
    fallback.directiveLength = function.directiveLength;
    fallback.commands = function.commands;
    runCompileFunction(std::move(fallback), function.file);
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
    m_settingsRoot = settingsDirectory().generic_string();
    if (m_settingsRoot.empty()) {
        m_settingsError = "Could not locate the settings directory";
        return;
    }
    ensureSettingsFile();
    loadSettingsFromDisk();
}

void Application::ensureSettingsFile() {
    if (m_settingsRoot.empty()) {
        return;
    }

    std::error_code error;
    std::filesystem::create_directories(m_settingsRoot, error);
    if (error) {
        m_settingsError = "Could not create settings project: " + error.message();
        return;
    }

    const std::filesystem::path file =
        std::filesystem::path(m_settingsRoot) / "settings.json";
    if (std::filesystem::exists(file, error)) {
        return;
    }

    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    if (!stream) {
        m_settingsError = "Could not create settings.json";
        return;
    }
    const std::string defaults = defaultSettingsJson();
    stream.write(defaults.data(), static_cast<std::streamsize>(defaults.size()));
    if (!stream) {
        m_settingsError = "Could not write settings.json";
    }
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
    const SettingsResult result = parseSettings(text);
    if (!result.valid) {
        m_settingsError = result.error;
        return;
    }

    m_settingsError.clear();
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

void Application::openSettings() {
    ensureSettingsFile();
    if (m_settingsRoot.empty()) {
        setStatus("Settings are unavailable", true);
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
    m_searchBar.close();
    m_settingsPanelVisible = true;
    m_settingsHovered = ui::SettingsControl::None;
    focusCompileTerminal(false);
    m_welcomeActionHovered = false;
}

void Application::openSettingsJson() {
    ensureSettingsFile();
    if (m_settingsRoot.empty()) {
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

void Application::handleSettingsPanelClick(double x, double y) {
    using ui::SettingsControl;
    const SettingsControl control = settingsControlAt(x, y);
    if (control == SettingsControl::None) {
        return;
    }
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

void Application::persistSettings() {
    if (m_settingsRoot.empty()) {
        setStatus("Settings are unavailable", true);
        return;
    }

    if (m_hasDocument && isSettingsDocument() && !saveDocument()) {
        return;
    }

    const std::filesystem::path file =
        std::filesystem::path(m_settingsRoot) / "settings.json";
    const std::string text = serializeSettings(m_settings);
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    if (!stream) {
        setStatus("Could not write settings.json", true);
        return;
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!stream) {
        setStatus("Could not write settings.json", true);
        return;
    }

    if (m_hasDocument && isSettingsDocument()) {
        m_document = editor::Document::fromUtf8(text);
        m_settingsRevision = m_document.revision();
        m_settingsError.clear();
    }
    m_settingsError.clear();
    setStatus("Settings saved", false);
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
    const float advance = m_charAdvance > 0.0F ? m_charAdvance : 7.0F;
    const float anchorX =
        ui::theme::kEditorCodeLeft +
        static_cast<float>(std::max(0, m_activeDiagnosticColumn - 1)) * advance;
    return ui::offsetDiagnosticCard(
        ui::diagnosticCardLayoutAt(static_cast<float>(width),
                                   static_cast<float>(height), anchorX,
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
        constexpr float damping = 0.42F;
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

ui::FrameState Application::makeFrameState(double timeSeconds) {
    m_compileTerminalProcess.update();
    const double deltaSeconds =
        m_lastTimeSeconds > 0.0
            ? std::clamp(timeSeconds - m_lastTimeSeconds, 0.0, 0.05)
            : 0.0;
    m_lastTimeSeconds = timeSeconds;

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

    ui::FrameState state =
        m_searchBar.frameState(timeSeconds, m_animationSpeed);
    state.animationTime =
        static_cast<float>(std::fmod(m_animationTimeSeconds, animationPeriod));

    pollCompilerDiagnostics(timeSeconds);
    refreshSettings();
    refreshDiagnostics(timeSeconds);

    state.animationSpeed = m_animationSpeed;
    state.hasProject = m_hasProject;
    state.welcomeActionHovered = m_welcomeActionHovered && !state.commandVisible;
    state.projectName = m_projectName;
    state.hasDocument = m_hasDocument;
    state.filePath = m_filePath;
    state.dirty = m_hasDocument && m_document.dirty();
    state.showLineNumbers = m_showLineNumbers;
    state.highlightCurrentLine = m_highlightCurrentLine;
    state.cursorBlink = m_cursorBlink;
    state.cursorStyle = m_cursorStyle;
    state.backgroundMotion = m_backgroundMotion;
    state.backgroundDim = m_backgroundDim;

    std::string windowTitle = "Lucid";
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

    state.settingsErrorVisible = !m_settingsError.empty() && !state.commandVisible;
    state.settingsErrorHovered = m_settingsErrorHovered && !state.commandVisible;
    state.settingsErrorText = m_settingsError;

    state.settingsPanelVisible = m_settingsPanelVisible && !state.commandVisible;
    state.settingsHovered =
        state.settingsPanelVisible ? m_settingsHovered : ui::SettingsControl::None;
    state.settingsTheme = m_settings.theme.name;
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
    state.statusText = m_statusText;
    state.statusIsError = m_statusIsError;

    refreshHighlight();
    refreshCompileScript();
    updateVisibleLines();
    state.visibleLines = m_visibleLines;
    state.visibleTokens = m_visibleTokens;
    state.firstLine = m_firstLine;

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
    state.compileTerminalCloseHovered = m_compileTerminalCloseHovered;
    state.compileTerminalOpacity =
        (m_compileTerminalFocused ? 1.0F : 0.5F) *
        std::clamp(m_compileTerminalReveal, 0.0F, 1.0F);
    state.compileTerminalReveal = m_compileTerminalReveal;
    state.compileTerminalOffsetX = m_compileTerminalOffsetX;
    state.compileTerminalOffsetY = m_compileTerminalOffsetY;
    state.compileTerminalLine = m_compileTerminalLine;
    state.compileTerminalTitle = m_compileTerminalTitle;
    state.compileTerminalOutput = m_compileTerminalProcess.output();
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
            const float advance = m_charAdvance > 0.0F ? m_charAdvance : 7.0F;
            state.diagnosticAnchorValid = true;
            state.diagnosticAnchorX =
                ui::theme::kEditorCodeLeft +
                static_cast<float>(std::max(0, m_activeDiagnosticColumn - 1)) * advance;
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
    m_commandStatus = std::move(message);
    m_commandStatusIsError = error;
}

void Application::setStatus(std::string message, bool error) {
    m_statusText = std::move(message);
    m_statusIsError = error;
}

} // namespace lucid
