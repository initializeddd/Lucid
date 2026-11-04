#include "app/Application.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <thread>

namespace lucid {
struct ApplicationInteractionTest {
    static int run() {
        Application app;
        app.m_uiZoom = 3.0F;
        app.m_searchBar.open(ui::PromptKind::Ai);
        app.m_aiReady = true;
        app.setCommandStatus("Review response", false);
        for (int i = 0; i < 20; ++i) app.m_aiLines.push_back("Line " + std::to_string(i));
        app.refreshSuggestions();
        const auto visible = app.visibleSuggestionCount();
        if (visible >= static_cast<std::size_t>(ui::theme::kMaxSuggestions) ||
            app.m_suggestions.size() != visible || app.m_suggestions.front().path != "Line 0") return 1;
        for (int i = 0; i < 3; ++i) app.handleScroll({0.0, -0.25});
        if (app.m_aiScroll != 0) return 57;
        app.handleScroll({0.0, -0.25});
        if (app.m_aiScroll != 1) return 58;
        app.handleScroll({0.0, 1.0});
        if (app.m_aiScroll != 0) return 59;

        input::KeyEvent down{}; down.key = input::Key::Down;
        for (int i = 0; i < 25; ++i) app.handleCommandKey(down, false);
        if (app.m_aiScroll != app.m_aiLines.size() - visible ||
            app.m_suggestions.back().path != "Line 19") return 2;

        app.handleScroll({0.0, 3.0});
        if (app.m_aiScroll != app.m_aiLines.size() - visible - 3) return 3;
        for (int i = 0; i < 3; ++i) app.handleCommandKey(down, false);
        app.m_uiZoom = 1.0F;
        app.refreshSuggestions();
        if (app.m_aiScroll > app.m_aiLines.size() - app.visibleSuggestionCount() ||
            app.m_suggestions.back().path != "Line 19") return 4;
        app.m_aiResponse = std::string(100, 'x') + "Grüße";
        app.m_aiWrapColumns = 0;
        app.refreshSuggestions();
        const auto wideColumns = app.m_aiWrapColumns;
        app.m_uiZoom = 3.0F;
        app.refreshSuggestions();
        if (app.m_aiWrapColumns >= wideColumns || app.m_aiResponse != std::string(100, 'x') + "Grüße") return 5;
        app.m_aiResponse = std::string(4000, 'x');
        app.m_aiWrapColumns = 0;
        app.m_aiScroll = 0;
        app.refreshSuggestions();
        for (int i = 0; i < 10; ++i) app.handleCommandKey(down, false);
        const auto anchor = app.m_aiLineOffsets[app.m_aiScroll];
        app.m_uiZoom = 1.0F;
        app.refreshSuggestions();
        const auto top = app.m_aiLineOffsets[app.m_aiScroll];
        const auto next = app.m_aiScroll + 1 < app.m_aiLineOffsets.size() ? app.m_aiLineOffsets[app.m_aiScroll + 1] : app.m_aiResponse.size();
        if (top > anchor || next <= anchor) return 6;
        app.m_searchBar.close();
        app.m_hasDocument = true;
        app.m_filePath = "current.cpp";
        app.m_uiZoom = 3.0F;
        for (int i = 0; i < 12; ++i)
            app.m_inactiveDocuments.push_back({"file" + std::to_string(i) + ".cpp", {}, 0, {}});
        app.openDocumentSwitcher();
        if (app.m_suggestions.size() != app.visibleSuggestionCount() || !app.m_suggestionHasMoreBelow) return 7;
        for (int i = 0; i < 10; ++i) app.moveSuggestion(1);
        if (app.m_suggestionScroll == 0 || app.m_selectedSuggestion < 0 ||
            app.m_suggestions[static_cast<std::size_t>(app.m_selectedSuggestion)].path != "file10.cpp") return 8;
        app.m_searchBar.setQuery("file11");
        app.onCommandQueryChanged();
        if (app.m_suggestions.size() != 1 || app.m_suggestions[0].path != "file11.cpp") return 9;
        app.m_hasProject = true;
        app.m_functionIndexValid = true;
        app.m_functionIndexDirty = false;
        for (int i = 0; i < 12; ++i) {
            workspace::ProjectFunction function;
            function.name = "function" + (i < 10 ? std::string("0") : std::string{}) + std::to_string(i);
            function.file = "build.cpp";
            app.m_functionIndex.functions.push_back(std::move(function));
        }
        app.m_searchBar.open(ui::PromptKind::Function);
        app.clearCommandFeedback();
        app.refreshSuggestions();
        for (int i = 0; i < 10; ++i) app.moveSuggestion(1);
        if (app.m_suggestionScroll == 0 || app.m_selectedSuggestion < 0) return 10;
        const auto selected = static_cast<std::size_t>(app.m_selectedSuggestion);
        if (app.m_suggestions[selected].path != "function09" ||
            app.m_functionIndex.functions[app.m_suggestionFunctionIndices[selected]].name != "function09") return 11;
        app.m_searchBar.open(ui::PromptKind::SettingsMenu);
        app.m_settingsMenuPage = 0;
        app.clearCommandFeedback();
        app.setCommandStatus("Choose a setting", false);
        app.refreshSuggestions();
        for (int i = 0; i < 6; ++i) app.moveSuggestion(1);
        const std::string selectedSetting(app.m_suggestions[static_cast<std::size_t>(app.m_selectedSuggestion)].path);
        app.setCommandStatus(std::string(300, 'x'), true);
        if (app.commandStatusRows() != 3 || app.m_selectedSuggestion < 0 ||
            app.m_suggestions.size() > app.visibleSuggestionCount() ||
            app.m_suggestions[static_cast<std::size_t>(app.m_selectedSuggestion)].path != selectedSetting) return 12;
        app.setCommandStatus("Saved", false);
        if (app.commandStatusRows() != 1 || app.m_selectedSuggestion < 0 ||
            app.m_suggestions[static_cast<std::size_t>(app.m_selectedSuggestion)].path != selectedSetting) return 13;
        app.m_searchBar.open(ui::PromptKind::RecoveryDetail);
        app.clearCommandFeedback();
        app.m_recoveryPreview.file = "example.cpp";
        app.m_recoveryPreview.contents = "first\nsecond\nthird\nfourth\nfifth\nsixth\n";
        app.m_recoveryPage = 1;
        app.setCommandStatus(std::string(140, 'x'), true);
        app.refreshSuggestions();
        if (app.m_suggestions.size() > app.visibleSuggestionCount() || app.m_recoveryChoices.size() != app.m_suggestions.size()) return 14;
        if (std::find(app.m_recoveryChoices.begin(), app.m_recoveryChoices.end(), -1) == app.m_recoveryChoices.end() ||
            std::find(app.m_recoveryChoices.begin(), app.m_recoveryChoices.end(), -2) == app.m_recoveryChoices.end()) return 15;
        app.m_searchBar.open(ui::PromptKind::ModuleList);
        app.clearCommandFeedback();
        for (int i = 0; i < 20; ++i) {
            modules::Module module;
            module.id = "module" + std::to_string(i);
            module.name = module.id;
            app.m_modules.push_back(std::move(module));
        }
        app.m_modulePage = 1;
        app.setCommandStatus(std::string(140, 'x'), true);
        app.refreshSuggestions();
        if (app.m_suggestions.size() > app.visibleSuggestionCount() || app.m_moduleChoices.size() != app.m_suggestions.size()) return 16;
        if (std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -1) == app.m_moduleChoices.end() ||
            std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -2) == app.m_moduleChoices.end()) return 17;
        app.m_selectedSuggestion = 0;
        app.m_uiZoom = 1.0F;
        input::KeyEvent enter{}; enter.key = input::Key::Enter;
        app.handleCommandKey(enter, false);
        if (app.m_searchBar.kind() != ui::PromptKind::ModuleList || app.m_selectedSuggestion >= 0 ||
            app.m_commandStatus != "List changed; select an option again") return 18;
        app.m_uiZoom = 3.0F;
        app.m_searchBar.open(ui::PromptKind::SettingsMenu);
        app.m_settingsMenuPage = 0;
        app.clearCommandFeedback();
        app.refreshSuggestions();
        for (int i = 0; i < 8; ++i) app.moveSuggestion(1);
        if (app.m_selectedSuggestion < 0 || app.m_suggestions[static_cast<std::size_t>(app.m_selectedSuggestion)].path != "AI settings") return 19;
        app.m_uiZoom = 1.0F;
        app.handleCommandKey(enter, false);
        if (app.m_searchBar.kind() != ui::PromptKind::SettingsMenu || app.m_selectedSuggestion >= 0 ||
            app.m_commandStatus != "List changed; select an option again") return 20;
        app.m_uiZoom = 3.0F;
        app.m_menuViewportValid = false;
        app.clearCommandFeedback();
        app.refreshSuggestions();
        app.refreshMenuViewport();
        for (int i = 0; i < 8; ++i) app.moveSuggestion(1);
        if (app.m_selectedSuggestion < 0 || app.m_suggestionScroll == 0) return 21;
        app.m_uiZoom = 1.0F;
        app.refreshMenuViewport();
        if (app.m_selectedSuggestion >= 0 || app.m_suggestionScroll != 0 || app.m_suggestions.size() != 9) return 22;
        app.handleCommandKey(enter, false);
        if (app.m_searchBar.kind() != ui::PromptKind::SettingsMenu || app.m_settingsMenuPage != 0 ||
            app.m_commandStatus != "List changed; select an option again") return 23;
        app.handleCommandKey(down, false);
        if (app.m_menuReselectionRequired || app.m_selectedSuggestion < 0) return 24;
        // Model a small window at maximum text size: status wrapping must not
        // push the first choice below the visible area.
        app.m_uiZoom = 6.0F;
        app.setCommandStatus(std::string(300, 'x'), true);
        int width = 0, height = 0;
        app.uiSize(width, height);
        const float firstChoiceBottom = app.suggestionListTop() + ui::theme::kSuggestionLineHeight;
        if (firstChoiceBottom > static_cast<float>(height) - ui::theme::kSearchBarMargin ||
            app.commandStatusRows() != 1) return 25;
        app.m_searchBar.open(ui::PromptKind::RecoveryDetail);
        app.m_pagedLayoutValid = false;
        app.m_recoveryPage = 1;
        app.m_recoveryPreview.contents = "one\ntwo\nthree\nfour\nfive\nsix\n";
        app.refreshSuggestions();
        for (int i = 0; i < 4 && (app.m_selectedSuggestion < 0 ||
             app.m_suggestions[static_cast<std::size_t>(app.m_selectedSuggestion)].path != "Next page >"); ++i)
            app.moveSuggestion(1);
        if (app.m_selectedSuggestion < 0 ||
            app.m_suggestions[static_cast<std::size_t>(app.m_selectedSuggestion)].path != "Next page >" ||
            app.m_pagedScroll == 0) return 26;
        app.applyRecovery();
        if (app.m_recoveryPage != 2 || app.m_pagedScroll != 0 || !app.m_menuReselectionRequired) return 27;
        app.handleCommandKey(enter, false);
        if (app.m_recoveryPage != 2 || app.m_selectedSuggestion >= 0) return 32;
        app.m_pendingSession = workspace::session::State{};
        app.m_pendingSession->project = "/tmp/example";
        app.m_searchBar.open(ui::PromptKind::SessionRestore);
        app.refreshSuggestions();
        for (int i = 0; i < 3; ++i) app.moveSuggestion(1);
        if (app.m_selectedSuggestion < 0 || app.m_pagedScroll == 0 ||
            app.m_suggestions[static_cast<std::size_t>(app.m_selectedSuggestion)].path.find("Don't show again") == std::string_view::npos) return 28;
        input::KeyEvent space{}; space.key = input::Key::Space;
        app.handleCommandKey(space, false);
        if (!app.m_rememberSessionChoice || !app.m_pendingSession) return 29;
        app.m_searchBar.open(ui::PromptKind::ProjectSearchResults);
        app.m_projectSearchPage = 0;
        app.m_projectSearchResults.files = {{"example.cpp", ""}};
        app.m_projectSearchResults.matches.assign(7, {});
        app.refreshSuggestions();
        for (int i = 0; i < 12 && (app.m_selectedSuggestion < 0 ||
             app.m_suggestions[static_cast<std::size_t>(app.m_selectedSuggestion)].path != "Next page >"); ++i)
            app.moveSuggestion(1);
        if (app.m_selectedSuggestion < 0 ||
            app.m_suggestions[static_cast<std::size_t>(app.m_selectedSuggestion)].path != "Next page >") return 30;
        app.applyProjectSearch();
        if (app.m_projectSearchPage != 1 || app.m_pagedScroll != 0 || !app.m_menuReselectionRequired) return 31;
        app.m_searchBar.open(ui::PromptKind::TaskList);
        app.m_taskBoard.tasks.clear();
        for (int i = 0; i < 8; ++i) {
            workspace::Task task;
            task.id = std::to_string(i);
            task.title = "match " + std::to_string(i);
            app.m_taskBoard.tasks.push_back(std::move(task));
        }
        app.m_taskPage = 3;
        app.refreshSuggestions();
        for (int i = 0; i < 2; ++i) app.moveSuggestion(1);
        app.m_searchBar.setQuery("match");
        app.onCommandQueryChanged();
        if (app.m_taskPage != 0 || app.m_pagedScroll != 0 || app.m_selectedSuggestion != -1 ||
            app.m_suggestions.empty() || app.m_suggestions.front().path.find("match 0") == std::string_view::npos) return 33;
        app.m_searchBar.open(ui::PromptKind::RecoveryDetail);
        app.m_recoveryPage = 1;
        app.clearCommandFeedback();
        app.refreshSuggestions();
        app.handleScroll({0.0, -9.0});
        if (app.m_recoveryPage != 1 || app.m_suggestions.empty() ||
            app.m_suggestions.back().path != "Next page >") return 34;
        app.handleScroll({0.0, -1.0});
        if (app.m_recoveryPage != 2 || app.m_pagedScroll != 0) return 35;
        app.m_searchBar.open(ui::PromptKind::SettingsMenu);
        app.m_settingsMenuPage = 0;
        app.clearCommandFeedback();
        app.refreshSuggestions();
        for (int i = 0; i < 7; ++i) app.moveSuggestion(1);
        if (app.m_suggestionScroll == 0 || app.m_selectedSuggestion < 0 ||
            app.m_suggestions[static_cast<std::size_t>(app.m_selectedSuggestion)].path != "Animation") return 36;
        app.handleCommandKey(enter, false);
        if (app.m_settingsMenuPage != 7 || app.m_suggestionScroll != 0 ||
            app.m_suggestions.empty() || app.m_suggestions.front().path != "Half speed") return 37;
        input::KeyEvent escape{}; escape.key = input::Key::Escape;
        app.handleCommandKey(escape, false);
        if (app.m_settingsMenuPage != 0 || app.m_selectedSuggestion < 0 ||
            app.m_suggestions[static_cast<std::size_t>(app.m_selectedSuggestion)].path != "Animation") return 38;
#if !defined(_WIN32)
        std::string processError;
        if (!app.m_compileTerminalProcess.start("/tmp", {"printf 'build output'"}, processError)) return 39;
        for (int i = 0; i < 100 && app.m_compileTerminalProcess.output().find("build output") == std::string_view::npos; ++i) {
            app.m_compileTerminalProcess.update();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (app.m_compileTerminalProcess.output().find("build output") == std::string_view::npos) return 40;
        app.m_pendingSession.reset();
        app.m_searchBar.close();
        app.m_compileTerminalFocused = true;
        app.m_compileTerminalVisible = true;
        input::KeyEvent selectAll{}; selectAll.key = input::Key::A;
#if defined(__APPLE__)
        selectAll.super = true;
#else
        selectAll.control = true;
#endif
        app.handleKey(selectAll);
        if (app.m_compileSelectionAnchor != 0 || app.m_compileSelectionSnapshot.empty() ||
            app.m_compileSelectionEnd != app.m_compileSelectionSnapshot.size()) return 41;
        app.m_compileSelectionEnd = 0;
        app.handleCompileTerminalKey(selectAll);
        if (app.m_compileSelectionEnd != app.m_compileSelectionSnapshot.size()) return 42;
        app.m_compileSelectionSnapshot = "a\tüx";
        app.m_compileOutputScroll = 0;
        const auto outputArea = app.compileTerminalLayout().output;
        const double cell = app.m_charAdvance > 0.0F ? app.m_charAdvance : 7.0F;
        if (app.compileOutputPosition(outputArea.x0 + cell * 2.0, outputArea.y0, false) != 1 ||
            app.compileOutputPosition(outputArea.x0 + cell * 4.0, outputArea.y0, false) != 2 ||
            app.compileOutputPosition(outputArea.x0 + cell * 6.0, outputArea.y0, false) != 4) return 48;
        app.m_compileTerminalProcess.close();
        if (!app.m_compileTerminalProcess.start("/tmp", {"for i in $(seq 1 30); do printf 'line %s\\n' \"$i\"; done; printf 're%s\\n' 'ady'; read answer; printf 'fi%s\\nmore\\n' 'nished'"}, processError)) return 43;
        app.m_compileOutputScroll = 0;
        for (int i = 0; i < 100 && app.m_compileTerminalProcess.output().find("ready") == std::string_view::npos; ++i) {
            app.updateCompileOutput();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (app.m_compileTerminalProcess.output().find("ready") == std::string_view::npos || app.m_compileOutputScroll != 0) return 44;
        app.m_compileOutputScroll = 10000;
        app.scrollCompileOutput(-1.0, false);
        if (app.m_compileOutputScroll < 0 || app.m_compileOutputScroll >= 30) return 62;
        app.m_compileOutputScroll = 0;
        app.m_compileScrollRemainder = 0.0;
        for (int i = 0; i < 4; ++i) app.scrollCompileOutput(0.1, false);
        if (app.m_compileOutputScroll != 1) return 46;
        app.scrollCompileOutput(-1.0, false);
        if (app.m_compileOutputScroll != 0) return 47;
        app.m_compileOutputScroll = 3;
        app.m_compileSelectionSnapshot = std::string(app.m_compileTerminalProcess.output());
        app.m_compileSelectionAnchor = 1;
        app.m_compileSelectionEnd = 3;
        app.m_compileSelecting = true;
        app.m_compileTerminalProcess.write("go\n");
        for (int i = 0; i < 100 && app.m_compileTerminalProcess.output().find("finished") == std::string_view::npos; ++i) {
            app.updateCompileOutput();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (app.m_compileTerminalProcess.output().find("finished") == std::string_view::npos || app.m_compileOutputScroll <= 3 ||
            !app.m_compileSelecting || app.m_compileSelectionSnapshot != app.m_compileTerminalProcess.output() ||
            app.m_compileSelectionAnchor != 1 || app.m_compileSelectionEnd != 3) return 45;
        app.m_compileTerminalProcess.close();
#endif
        app.m_hasDocument = true;
        app.m_document = editor::Document::fromUtf8("\tfoo\n");
        app.m_activeDiagnosticLine = 0;
        app.m_activeDiagnosticColumn = 3;
        const float advance = app.m_charAdvance > 0.0F ? app.m_charAdvance : 7.0F;
        if (std::abs(app.diagnosticAnchorX() - (ui::theme::kEditorCodeLeft + 5.0F * advance)) > 0.01F) return 49;
        app.m_compileTerminalFocused = false;
        app.m_settingsPanelVisible = false;
        app.m_pendingSession.reset();
        app.m_hasProject = true;
        app.m_uiZoom = 1.0F;
        app.m_document = editor::Document::fromUtf8("abc");
        app.m_document.setCursor(0, 3);
        app.m_searchBar.open(ui::PromptKind::File);
        app.m_searchBar.close();
        if (!app.m_searchBar.visible() || app.m_searchBar.isOpen()) return 50;
        app.handleChar(U'd');
        if (app.m_document.toUtf8() != "abcd") return 51;
        input::KeyEvent backspace{}; backspace.key = input::Key::Backspace;
        app.handleKey(backspace);
        if (app.m_document.toUtf8() != "abc") return 52;
        app.handleMouseButton({ui::theme::kEditorCodeLeft, ui::theme::kEditorTop + 4.0,
                               input::MouseButton::Left, input::KeyAction::Press});
        if (app.m_document.cursorColumn() != 0) return 53;
        app.m_document = editor::Document::fromUtf8("a\na\na\na\na\na\na\na\na\na\na\n");
        app.m_firstLine = 5;
        app.m_editorScrollRemainder = 0.0;
        app.m_followCursor = true;
        for (int i = 0; i < 3; ++i) app.handleScroll({0.0, 0.1});
        if (app.m_firstLine != 5 || !app.m_followCursor) return 54;
        app.handleScroll({0.0, 0.1});
        if (app.m_firstLine != 4 || app.m_followCursor) return 55;
        app.m_firstLine = 0;
        app.handleScroll({0.0, 0.1});
        app.handleScroll({0.0, -0.4});
        if (app.m_firstLine != 1) return 56;
        app.m_document = editor::Document::fromUtf8("void grüßen() {}\ngrü");
        app.m_document.setCursor(1, 3);
        app.refreshCompletions();
        const auto unicodeCompletion = std::find(app.m_completions.begin(), app.m_completions.end(), "grüßen");
        if (app.m_completionStart != 0 || unicodeCompletion == app.m_completions.end()) return 60;
        app.m_completionSelected = static_cast<int>(unicodeCompletion - app.m_completions.begin());
        app.m_completionOpen = true;
        app.handleKey(enter);
        if (app.m_document.lineUtf8(1) != "grüßen") return 61;
        app.m_document = editor::Document::fromUtf8("void grüßen() {}\n\xc2\xa0grü");
        app.m_document.setCursor(1, 4);
        app.refreshCompletions();
        const auto spacedCompletion = std::find(app.m_completions.begin(), app.m_completions.end(), "grüßen");
        if (app.m_completionStart != 1 || spacedCompletion == app.m_completions.end()) return 62;
        app.m_completionSelected = static_cast<int>(spacedCompletion - app.m_completions.begin());
        app.m_completionOpen = true;
        app.handleKey(enter);
        if (app.m_document.lineUtf8(1) != "\xc2\xa0grüßen") return 63;
        app.m_document = editor::Document::fromUtf8("foo foo");
        app.m_filePath = "same.cpp";
        app.m_document.setCursor(0, 0);
        app.m_document.extendSelectionTo(0, 3);
        app.m_replaceFile = app.m_filePath;
        app.m_replaceQuery = "foo";
        app.m_replaceRevision = app.m_document.revision();
        app.m_replacePending = true;
        app.m_searchBar.open(ui::PromptKind::Replace);
        app.m_searchBar.setQuery("foo");
        app.applyReplacement();
        if (app.m_document.toUtf8() != "foo foo" || app.m_document.dirty() ||
            app.m_document.revision() != app.m_replaceRevision ||
            app.m_document.selectionStart().column != 4 || app.m_document.selectionUtf8() != "foo") return 64;
        return 0;
    }
};
}

int main() { return lucid::ApplicationInteractionTest::run(); }
