#include "app/Application.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <cstdlib>
#include <thread>

#if defined(__APPLE__)
bool triggerCompletionMenuShortcut();
bool triggerReplaceMenuShortcut();
bool triggerSwitchFileShortcut();
bool checkAccessibility(void* nativeWindow, const char* expected, unsigned long start, unsigned long length);
bool sendOpenFileEvent(const char* path);
#endif

namespace lucid {
struct ApplicationInteractionTest {
    static int run() {
        platform::prepareOpenFiles();
        Application app;
        const auto savedSettings = app.m_settings;
        const auto savedSettingsRoot = app.m_settingsRoot;
        const auto settingsTemp = std::filesystem::temp_directory_path() /
            ("lucid-settings-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(settingsTemp);
        app.m_settingsRoot = settingsTemp.string();
        auto accessibilitySettings = app.m_settings; accessibilitySettings.zoomFactor = 1.0F; accessibilitySettings.highContrast = false;
        app.applySettings(accessibilitySettings);
        input::KeyEvent accessibilityKey{}; accessibilityKey.action = input::KeyAction::Press;
#if defined(__APPLE__)
        accessibilityKey.super = true;
#else
        accessibilityKey.control = true;
#endif
        accessibilityKey.key = input::Key::Equal; app.handleKey(accessibilityKey);
        if (app.m_settings.zoomFactor < 1.09F) return 81;
        accessibilityKey.key = input::Key::Zero; app.handleKey(accessibilityKey);
        if (app.m_settings.zoomFactor != 1.0F) return 82;
        accessibilityKey.key = input::Key::U; accessibilityKey.shift = true; app.handleKey(accessibilityKey);
        if (!app.m_settings.highContrast || ui::theme::kBackground.r != 0.0F) return 83;
        app.handleKey(accessibilityKey);
        if (app.m_settings.highContrast) return 84;
        app.m_settingsPanelVisible = true;
        input::KeyEvent keyboardTab{}; keyboardTab.key = input::Key::Tab;
        keyboardTab.action = input::KeyAction::Press;
        app.handleKey(keyboardTab);
        if (app.m_settingsHovered != ui::SettingsControl::ThemeLucidDark) return 40;
        app.handleKey(keyboardTab);
        if (app.m_settingsHovered != ui::SettingsControl::ThemeGraphite) return 41;
        input::KeyEvent select{}; select.key = input::Key::Enter; select.action = input::KeyAction::Press;
        app.handleKey(select);
        if (app.m_settings.theme.name != "graphite") return 42;
        const auto layout = app.settingsPanelLayout();
        app.handleSettingsPanelClick(layout.iconButtons[1].x0 + 1, layout.iconButtons[1].y0 + 1);
        if (app.m_settings.iconTheme != "accent") return 28;
        app.handleSettingsPanelClick(layout.primarySwatches[3].x0 + 1, layout.primarySwatches[3].y0 + 1);
        if (!app.m_settings.theme.primary) return 29;
        app.handleSettingsPanelClick(layout.zoomButtons[2].x0 + 1, layout.zoomButtons[2].y0 + 1);
        if (app.m_settings.zoomFactor != 1.25F) return 30;
        { std::ifstream input(settingsTemp / "settings.json");
          std::string text((std::istreambuf_iterator<char>(input)), {});
          const auto parsed = parseSettings(text);
          if (!parsed.valid || parsed.settings.iconTheme != "accent" || parsed.settings.zoomFactor != 1.25F) return 31; }
        app.activateSettingsControl(ui::SettingsControl::AiSettings);
        if (app.m_searchBar.kind() != ui::PromptKind::AiSettings || app.m_suggestions.size() != 8) return 90;
        app.m_selectedSuggestion = 0; app.applyCommand(false);
        if (app.m_settings.aiAskProvider != "openrouter") return 91;
        app.m_selectedSuggestion = 1; app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::AiModelInput) return 92;
        app.m_searchBar.setQuery("example/model"); app.applyCommand(false);
        if (app.m_settings.aiAskModel != "example/model" || app.m_searchBar.kind() != ui::PromptKind::AiSettings) return 93;
        app.m_selectedSuggestion = 6; app.applyCommand(false); app.handleChar(U's'); app.handleChar(U'k');
        if (app.m_aiSecret != "sk" || app.m_searchBar.query() != "**") return 94;
        input::KeyEvent cancelSecret{}; cancelSecret.action = input::KeyAction::Press; cancelSecret.key = input::Key::Escape;
        app.handleCommandKey(cancelSecret, false);
        if (!app.m_aiSecret.empty() || app.m_searchBar.kind() != ui::PromptKind::AiSettings) return 95;
        app.m_searchBar.close();
        for (int i = 0; i < 120; ++i) (void)app.makeFrameState(1.0 + i / 60.0);
        const auto moduleTemp = settingsTemp / "Modules";
        std::filesystem::create_directories(moduleTemp / "example");
        { std::ofstream out(moduleTemp / "example/module.json");
          out << R"({"name":"Example","version":"1","permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Insert hello","insert":"hello"}]})"; }
        app.m_settingsRoot = (settingsTemp / "Settings").string();
        app.m_settingsPanelVisible = false;
        app.openModules();
        if (app.m_searchBar.kind() != ui::PromptKind::ModuleList || app.m_modules.size() != 1) return 65;
        app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::ModuleActions) return 66;
        app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::ModuleList || !app.m_modules[0].enabled) return 67;
        app.applyCommand(false);
        app.m_hasDocument = true;
        app.m_document = editor::Document::fromUtf8("");
        app.m_selectedSuggestion = 1; app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::ModuleCommands) return 68;
        app.applyCommand(false);
        if (app.m_document.toUtf8() != "hello") return 69;
        app.m_document.undo(); if (!app.m_document.toUtf8().empty()) return 70;
        app.m_hasDocument = false;
        for (int i = 0; i < 120; ++i) (void)app.makeFrameState(1.0 + i / 60.0);
        app.m_settingsPanelVisible = true;
        app.applySettings(savedSettings);
        app.m_settingsRoot = savedSettingsRoot;
        std::filesystem::remove_all(settingsTemp);
        app.handleMouseButton({2.0, 120.0, input::MouseButton::Left, input::KeyAction::Press});
        if (!app.m_settingsPanelVisible) { std::cerr << "settings page dismissed outside controls\n"; return 23; }
        app.m_renderer->drawFrame(app.makeFrameState(0.5));
        app.m_settingsPanelVisible = false;
        app.m_hasProject = true;
        app.m_hasDocument = true;
        app.m_filePath = "example.cpp";
        app.m_document = editor::Document::fromUtf8("hello world\nvoid greet() {}\ngre");
#if defined(__APPLE__)
        app.m_document = editor::Document::fromUtf8("a🙂b");
        app.m_document.setCursor(0, 1); app.m_document.extendSelectionTo(0, 2);
        app.syncAccessibility(app.makeFrameState(0.9));
        if (!checkAccessibility(app.m_window.handle(), "a🙂b", 1, 2)) return 80;
        app.m_document = editor::Document::fromUtf8("hello world\nvoid greet() {}\ngre");
        app.m_accessibilityKey.clear();
#endif
        const double x = ui::theme::kEditorCodeLeft + app.m_charAdvance * 2.0;
        const double y = ui::theme::kEditorTop + ui::theme::kEditorLineHeight * 0.5;
        input::MouseButtonEvent click{x, y, input::MouseButton::Left, input::KeyAction::Press};
        input::MouseButtonEvent release = click;
        release.action = input::KeyAction::Release;
        app.handleMouseButton(click); app.handleMouseButton(release);
        app.handleMouseButton(click);
        if (app.m_document.selectionUtf8() != "hello") { std::cerr << "double click failed\n"; return 1; }
        app.handleMouseMove({ui::theme::kEditorCodeLeft + app.m_charAdvance * 9.0, y});
        if (app.m_document.selectionUtf8() != "hello world") { std::cerr << "word drag failed\n"; return 7; }
        app.handleMouseButton(release);
        app.handleMouseButton(click); app.handleMouseButton(release);
        app.handleMouseButton(click); app.handleMouseButton(release);
        if (app.m_document.selectionUtf8() != "hello world\n") { std::cerr << "line click failed\n"; return 2; }
        app.m_document.setCursor(2, 3);
        input::KeyEvent shortcut{};
        shortcut.key = input::Key::Space; shortcut.action = input::KeyAction::Press; shortcut.shift = true;
#if defined(__APPLE__)
        shortcut.super = true;
#else
        shortcut.control = true;
#endif
#if defined(__APPLE__)
        if (!triggerCompletionMenuShortcut()) { std::cerr << "macOS menu shortcut not handled\n"; return 11; }
#else
        app.handleKey(shortcut);
#endif
        if (!app.m_completionOpen || app.m_completions.empty() || app.m_completions.front() != "greet") {
            std::cerr << "completion failed\n"; return 3;
        }
        const auto completionFrame = app.makeFrameState(1.0);
        if (!completionFrame.completionVisible || completionFrame.completions.size() != 1 ||
            completionFrame.completions.front() != "greet") {
            std::cerr << "completion missing from editor frame\n"; return 8;
        }
        app.m_renderer->drawFrame(completionFrame);
        input::KeyEvent enter{}; enter.key = input::Key::Enter; enter.action = input::KeyAction::Press;
        app.handleKey(enter);
        if (app.m_document.lineUtf8(2) != "greet") { std::cerr << "completion acceptance failed\n"; return 4; }
        app.m_document = editor::Document::fromUtf8("unmatched");
        app.m_document.setCursor(0, 9);
        app.handleKey(shortcut);
        const auto emptyFrame = app.makeFrameState(1.5);
        if (!emptyFrame.completionVisible || !emptyFrame.completions.empty()) {
            std::cerr << "empty completion popup missing\n"; return 9;
        }
        app.m_renderer->drawFrame(emptyFrame);
        input::KeyEvent escape{}; escape.key = input::Key::Escape; escape.action = input::KeyAction::Press;
        app.handleKey(escape);
        if (app.makeFrameState(1.6).completionVisible) return 10;
        app.m_document = editor::Document::fromUtf8("alpha beta alpha\nlast alpha");
        app.m_document.setCursor(0, 0);
        input::KeyEvent findKey{}; findKey.key = input::Key::F; findKey.action = input::KeyAction::Press;
#if defined(__APPLE__)
        findKey.super = true;
#else
        findKey.control = true;
#endif
        app.handleKey(findKey);
        for (char32_t ch : std::u32string_view(U"alpha")) app.handleChar(ch);
        if (app.m_document.selectionStart().column != 0 || app.m_document.selectionUtf8() != "alpha") return 12;
        app.handleKey(enter);
        if (app.m_document.selectionStart().column != 11) return 13;
        auto previous = enter; previous.shift = true; app.handleKey(previous);
        if (app.m_document.selectionStart().column != 0) return 14;
#if defined(__APPLE__)
        if (!triggerReplaceMenuShortcut()) return 22;
#else
        input::KeyEvent replaceKey{}; replaceKey.key = input::Key::H;
        replaceKey.action = input::KeyAction::Press; replaceKey.control = true;
        app.handleKey(replaceKey);
#endif
        app.handleKey(enter);
        if (app.m_searchBar.kind() != ui::PromptKind::Replace) return 16;
        app.handleChar(U'β');
        app.handleKey(enter);
        if (app.m_document.lineUtf8(0) != "β beta alpha" || app.m_document.selectionUtf8() != "alpha") return 17;
        app.m_document.undo();
        if (app.m_document.lineUtf8(0) != "alpha beta alpha") return 18;
        app.openReplace();
        app.handleKey(enter);
        if (app.m_searchBar.kind() != ui::PromptKind::Replace) return 19;
        app.handleKey(enter);
        if (app.m_document.lineUtf8(0) != " beta alpha") return 20;
        app.m_document.undo();
        if (app.m_document.lineUtf8(0) != "alpha beta alpha") return 21;
        app.handleKey(escape);
        for (int i = 0; i < 120; ++i) (void)app.makeFrameState(2.0 + i / 60.0);
        if (app.m_searchBar.visible()) return 15;
        app.m_document = editor::Document::fromUtf8("hello world\nvoid greet() {}\ngreet");
        app.m_document.setCursor(0, 0);
        app.m_document.extendSelectionTo(0, 5);
        app.openAiCleanup();
        if (!app.m_aiCleanup || app.m_searchBar.kind() != ui::PromptKind::Ai ||
            app.makeFrameState(2.0).commandPlaceholder != "Clean selected code with AI...") return 61;
        app.m_aiResponse = "clean";
        app.m_aiLines = {"clean"};
        app.m_aiReady = true;
        const auto beforeCleanup = app.m_document.toUtf8();
        app.refreshSuggestions();
        if (app.m_document.toUtf8() != beforeCleanup) return 62;
        app.applyCommand(false);
        if (app.m_document.lineUtf8(0) != "clean world") return 63;
        app.m_document.undo();
        if (app.m_document.toUtf8() != beforeCleanup) return 64;
        app.m_document.setCursor(0, 0);
        app.m_document.extendSelectionTo(0, 5);
        app.openAiPrompt();
        app.m_aiResponse = "replacement";
        app.m_aiLines = {"replacement"};
        app.m_aiReady = true;
        app.refreshSuggestions();
        app.m_renderer->drawFrame(app.makeFrameState(2.0));
        const auto before = app.m_document.toUtf8();
        input::MouseButtonEvent preview{ui::theme::kSearchBarMargin + 10.0,
            app.suggestionListTop() + ui::theme::kSuggestionLineHeight * 0.5,
            input::MouseButton::Left, input::KeyAction::Press};
        app.handleMouseButton(preview);
        if (app.m_document.toUtf8() != before || !app.m_searchBar.isOpen()) {
            std::cerr << "preview click inserted text\n"; return 5;
        }
        app.applyCommand(false);
        if (app.m_document.lineUtf8(0) != "replacement world") { std::cerr << "AI acceptance failed\n"; return 6; }
        app.m_hasDocument = false;
        app.m_hasProject = false;
        const auto temp = std::filesystem::temp_directory_path() /
            ("lucid-open-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(temp / "nested");
        { std::ofstream out(temp / "nested" / "example.txt"); out << "opened externally"; }
        app.openExternalPath((temp / "nested" / "example.txt").string());
        if (!app.m_hasDocument || app.m_document.toUtf8() != "opened externally") return 24;
#if defined(__APPLE__)
        { std::ofstream out(temp / "nested" / "event.txt"); out << "opened by finder"; }
        if (!sendOpenFileEvent((temp / "nested" / "event.txt").string().c_str())) return 26;
        if (app.m_document.toUtf8() != "opened by finder") return 27;
#endif
        app.openExternalPath((temp / "missing.txt").string());
        if (std::filesystem::exists(temp / "missing.txt") || app.m_document.toUtf8() !=
#if defined(__APPLE__)
            "opened by finder"
#else
            "opened externally"
#endif
            ) return 25;
        app.openFile("example.txt");
        app.m_document.setCursor(0, app.m_document.line(0).size());
        app.m_document.insert(U'!');
        app.openFile("other.txt");
        if (app.m_filePath != "other.txt") return 32;
        { std::ifstream saved(temp / "nested" / "example.txt");
          std::string text((std::istreambuf_iterator<char>(saved)), {});
          if (text != "opened externally!") return 33; }
#if defined(__APPLE__)
        if (!triggerSwitchFileShortcut()) return 43;
#else
        input::KeyEvent switchKey{}; switchKey.key = input::Key::E;
        switchKey.action = input::KeyAction::Press; switchKey.control = true;
        app.handleKey(switchKey);
#endif
        const auto switchFrame = app.makeFrameState(5.0);
        if (switchFrame.commandKind != ui::PromptKind::OpenFiles ||
            switchFrame.suggestions.size() < 2 || switchFrame.suggestions[0].path != "other.txt" ||
            switchFrame.suggestions[1].path != "example.txt") return 34;
        app.applyCommand(false);
        if (app.m_filePath != "example.txt" || app.m_document.cursorColumn() != 18) return 35;
        app.m_document.undo();
        if (app.m_document.toUtf8() != "opened externally") return 36;
        app.openFile("other.txt");
        { std::ofstream changed(temp / "nested" / "example.txt", std::ios::trunc); changed << "external edit"; }
        app.openFile("example.txt");
        if (app.m_document.toUtf8() != "external edit") return 37;
        app.closeDocument();
        if (!app.m_hasDocument || app.m_filePath != "other.txt") return 44;
        app.openFile("example.txt");
        app.openFile("other.txt");
        std::filesystem::remove(temp / "nested" / "example.txt");
        app.openFile("example.txt");
        if (std::filesystem::exists(temp / "nested" / "example.txt") || app.m_filePath != "other.txt") return 38;
        app.closeDocument();
        if (std::filesystem::exists(temp / "nested" / "example.txt")) return 39;
        app.openFile("other.txt");
        app.m_document.setCursor(0, 0);
        app.m_settings.taskIdentity = "Alice";
        app.openTasks();
        if (app.m_searchBar.kind() != ui::PromptKind::TaskList) return 45;
        app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::TaskInput) return 46;
        for (char32_t ch : std::u32string_view(U"Fix build")) app.handleChar(ch);
        app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::TaskTier) return 47;
        app.m_selectedSuggestion = 2;
        app.applyCommand(false);
        if (app.m_taskBoard.tasks.size() != 1 || app.m_taskBoard.tasks[0].tier != 3 ||
            app.m_taskBoard.tasks[0].createdBy != "Alice") return 48;
        auto choose = [&](Application::TaskChoiceKind kind) {
            for (int page = 0; page < 5; ++page) {
                for (std::size_t i = 0; i < app.m_taskChoices.size(); ++i) {
                    if (app.m_taskChoices[i].kind == kind) {
                        app.m_selectedSuggestion = static_cast<int>(i);
                        app.applyCommand(false);
                        return true;
                    }
                }
                ++app.m_taskPage;
                app.refreshSuggestions();
            }
            return false;
        };
        if (!choose(Application::TaskChoiceKind::AddSubtask)) return 49;
        for (char32_t ch : std::u32string_view(U"Check logs")) app.handleChar(ch);
        app.applyCommand(false);
        if (app.m_taskBoard.tasks[0].subtasks.size() != 1) return 50;
        if (app.m_searchBar.kind() != ui::PromptKind::TaskSubtasks) return 51;
        app.m_settings.taskIdentity = "Bob";
        if (!choose(Application::TaskChoiceKind::ToggleSubtask) ||
            app.m_taskBoard.tasks[0].subtasks[0].doneBy != "Bob") return 52;
        app.showTaskView(ui::PromptKind::TaskActions);
        if (!choose(Application::TaskChoiceKind::OpenWorkspace) ||
            app.m_activeTaskId.empty() || app.m_filePath != "other.txt") return 53;
        workspace::TaskBoard diskBoard; std::string taskError;
        if (!workspace::loadTasks(app.m_project, diskBoard, taskError) ||
            diskBoard.tasks[0].subtasks[0].doneBy != "Bob") return 54;
        app.m_activeTaskId.clear();
#if !defined(_WIN32)
        const auto gitCommand = [&](const std::string& arguments) {
            return std::system(("git -C '" + (temp / "nested").string() + "' " + arguments + " >/dev/null 2>&1").c_str());
        };
        if (gitCommand("init -q") || gitCommand("config user.name Alice") ||
            gitCommand("config user.email alice@example.com") || gitCommand("config commit.gpgsign false") || gitCommand("config core.hooksPath /dev/null") || gitCommand("add other.txt") ||
            gitCommand("commit -qm initial")) return 55;
        app.m_document.insert(U'X');
        app.openGitReview();
        if (app.m_searchBar.kind() != ui::PromptKind::GitChanges) return 56;
        auto changed = std::find_if(app.m_gitChanges.begin(), app.m_gitChanges.end(),
            [](const auto& item) { return item.path == "other.txt"; });
        if (changed == app.m_gitChanges.end()) return 57;
        app.m_gitPage = (static_cast<std::size_t>(changed - app.m_gitChanges.begin()) + 1) / 6;
        app.refreshSuggestions();
        app.m_selectedSuggestion = static_cast<int>((static_cast<std::size_t>(changed - app.m_gitChanges.begin()) + 1) % 6);
        app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::GitFile) return 58;
        app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::GitDiff) return 85;
        app.m_renderer->drawFrame(app.makeFrameState(8.0));
        auto line = std::find_if(app.m_gitReview.lines.begin(), app.m_gitReview.lines.end(),
            [](const auto& item) { return !item.text.empty() && item.text[0] == '+' && item.line == 1; });
        if (line == app.m_gitReview.lines.end()) return 59;
        app.m_gitPage = static_cast<std::size_t>(line - app.m_gitReview.lines.begin()) / 6;
        app.refreshSuggestions();
        app.m_selectedSuggestion = static_cast<int>(static_cast<std::size_t>(line - app.m_gitReview.lines.begin()) % 6);
        app.applyCommand(false);
        if (app.m_filePath != "other.txt" || app.m_document.cursorLine() != 0) return 60;
        app.openGitReview();
        changed = std::find_if(app.m_gitChanges.begin(), app.m_gitChanges.end(), [](const auto& item) { return item.path == "other.txt"; });
        if (changed == app.m_gitChanges.end()) return 86;
        app.m_gitPage = (static_cast<std::size_t>(changed - app.m_gitChanges.begin()) + 1) / 6;
        app.refreshSuggestions(); app.m_selectedSuggestion = static_cast<int>((static_cast<std::size_t>(changed - app.m_gitChanges.begin()) + 1) % 6);
        app.applyCommand(false); app.m_selectedSuggestion = 1; app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::GitChanges) return 87;
        app.m_selectedSuggestion = 0; app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::GitCommitReview || app.m_gitCommitPreview.paths != std::vector<std::string>{"other.txt"}) return 88;
        app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::GitCommitInput) return 89;
        for (char32_t c : std::u32string_view(U"Test commit")) app.handleChar(c);
        app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::GitChanges || gitCommand("log -1 --format=%s | grep -q 'Test commit'")) return 90;
        app.m_searchBar.close();
#if defined(__APPLE__)
        if (std::getenv("LUCID_TEST_LLDB")) {
            { std::ofstream source(temp / "nested/debug.cpp"); source << "int main() {\n  volatile int n = 7;\n  n += 1;\n  return n == 8 ? 0 : 1;\n}\n"; }
            const std::string compile = "c++ -g -O0 '" + (app.m_project.root() / "debug.cpp").string() + "' -o '" + (app.m_project.root() / "debug-program").string() + "'";
            if (std::system(compile.c_str()) != 0) return 91;
            app.openFile("debug.cpp"); app.m_activeTaskId = app.m_taskBoard.tasks[0].id;
            app.m_document.setCursor(2, 0); app.toggleBreakpoint();
            if (app.m_taskBoard.tasks[0].debugSource != "debug.cpp" || app.m_taskBoard.tasks[0].debugBreakpoints != std::vector<int>{3}) return 99;
            app.openDebugger(); app.applyCommand(false);
            if (app.m_searchBar.kind() != ui::PromptKind::DebugInput) return 92;
            app.m_searchBar.setQuery("debug-program"); app.applyCommand(false);
            if (!app.m_debugAdapter.alive() || app.m_taskBoard.tasks[0].debugFunction != "debug-program") return 93;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
            bool reached = false;
            while (std::chrono::steady_clock::now() < deadline) {
                app.pollDebugger();
                if (app.m_debugSession.state() == debug::State::Stopped && !app.m_debugSession.stack().empty() && !app.m_debugSession.variables().empty()) { reached = true; break; }
                if (!app.m_debugAdapter.alive()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (!reached) { std::cerr << "debug: " << app.m_debugSession.message() << " " << app.m_statusText << " output=" << app.m_debugSession.output() << " bp=" << app.m_debugBreakpoints["debug.cpp"].size() << "\n"; return 94; }
            const auto debugFrame = app.makeFrameState(9.0);
            if (debugFrame.debugStoppedLine != 2 || debugFrame.debugBreakpointLines.empty() || debugFrame.debugInline.empty()) return 95;
            app.m_renderer->drawFrame(debugFrame);
            app.openDebugger(); app.m_selectedSuggestion = 6; app.applyCommand(false);
            if (app.m_debugPage != 1) return 100;
            app.m_selectedSuggestion = 1; app.applyCommand(false);
            if (!app.m_aiDebugExplanation || app.m_aiDebugOpenRouter || app.m_aiDebugContext.empty()) return 101;
            const auto beforeExplain = app.m_document.toUtf8();
            app.m_aiResponse = "Explanation"; app.m_aiReady = true; app.applyCommand(false);
            if (app.m_document.toUtf8() != beforeExplain) return 102;
            app.m_settings.aiDebugProvider = "openrouter";
            app.openDebugger(); app.m_selectedSuggestion = 6; app.applyCommand(false);
            app.m_selectedSuggestion = 1; app.applyCommand(false);
            if (!app.m_aiDebugExplanation || !app.m_aiDebugOpenRouter || app.m_commandStatus.find("OpenRouter receives") == std::string::npos) return 103;
            app.handleCommandKey(escape, false);
            app.m_settings.aiDebugProvider = "local";
            app.openDebugger(); app.m_selectedSuggestion = 2; app.applyCommand(false);
            if (app.m_debugSession.state() != debug::State::Running) return 96;
            reached = false;
            while (std::chrono::steady_clock::now() < deadline) {
                app.pollDebugger();
                if (app.m_debugSession.state() == debug::State::Stopped && !app.m_debugSession.stack().empty() && app.m_debugSession.stack()[0].line == 4 && !app.m_debugSession.variables().empty()) { reached = true; break; }
                if (!app.m_debugAdapter.alive()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (!reached) return 97;
            if (app.makeFrameState(9.5).debugInline.find("* n =") == std::string_view::npos) return 105;
            app.stopDebugger();
            while (app.m_debugAdapter.alive() && std::chrono::steady_clock::now() < deadline) { app.pollDebugger(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_debugAdapter.alive()) return 98;
            const auto debugTaskId = app.m_activeTaskId;
            app.m_activeTaskId.clear(); app.m_debugBreakpoints.clear();
            if (!app.activateTask(debugTaskId) || app.m_debugBreakpoints["debug.cpp"] != std::vector<int>{3}) return 104;
            app.m_activeTaskId.clear();
            app.openFile("other.txt");
        }
#endif
        const auto baseline = app.m_diskContents;
        app.m_document.insert(U'Y');
        { std::ofstream external(temp / "nested/other.txt", std::ios::trunc); external << "external change"; }
        if (app.saveDocument() || !app.m_document.dirty()) return 71;
        { std::ifstream external(temp / "nested/other.txt"); std::string text((std::istreambuf_iterator<char>(external)), {});
          if (text != "external change") return 72; }
        { std::ofstream restore(temp / "nested/other.txt", std::ios::trunc); restore << baseline; }
        if (!app.saveDocument()) return 73;
        app.m_settingsRoot = (temp / "settings" / "Settings").string();
        app.m_document.insert(U'Z');
        const auto recoveredText = app.m_document.toUtf8();
        app.checkpointRecovery();
        if (!std::filesystem::exists(app.m_recoveryFile)) return 74;
        app.m_recoveryFile.clear();
        if (!app.saveDocument()) return 75;
        { std::ofstream external(temp / "nested/other.txt", std::ios::trunc); external << "newer version"; }
        app.openRecovery();
        if (app.m_searchBar.kind() != ui::PromptKind::RecoveryList || app.m_recoveryFiles.empty()) return 76;
        app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::RecoveryDetail || app.m_recoveryPreview.contents != recoveredText) return 77;
        app.applyCommand(false);
        if (app.m_document.toUtf8() != recoveredText || !app.m_document.dirty() || app.saveDocument()) return 78;
        { std::ifstream external(temp / "nested/other.txt"); std::string text((std::istreambuf_iterator<char>(external)), {});
          if (text != "newer version") return 79; }
        app.m_settingsRoot = savedSettingsRoot;
#endif
        std::filesystem::remove_all(temp);
        return 0;
    }
};
}
int main() { return lucid::ApplicationInteractionTest::run(); }
