#include "app/Application.hpp"
#include "ai/ModelDownload.hpp"
#include "platform/macos/VulkanEnvironment.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <sqlite3.h>

#if defined(__APPLE__)
bool triggerCompletionMenuShortcut();
bool triggerReplaceMenuShortcut();
bool triggerSwitchFileShortcut();
bool triggerCloseFileShortcut();
bool checkAccessibility(void* nativeWindow, const char* expected, unsigned long start, unsigned long length);
bool accessibilityLabelContains(void* nativeWindow, const char* expected);
bool sendOpenFileEvent(const char* path);
#endif

namespace lucid {
struct ApplicationInteractionTest {
    static int run() {
        platform::configureVulkanDriverEnvironment();
        platform::prepareOpenFiles();
        Application app;
        app.m_hasDocument = true;
        app.m_filePath = "example.py";
        app.m_document = editor::Document::fromUtf8("print(1)");
        input::KeyEvent commentKey{}; commentKey.key = input::Key::Slash; commentKey.action = input::KeyAction::Press;
#if defined(__APPLE__)
        commentKey.super = true;
#else
        commentKey.control = true;
#endif
        app.handleKey(commentKey);
        if (app.m_document.toUtf8() != "# print(1)") return 315;
        app.handleKey(commentKey);
        if (app.m_document.toUtf8() != "print(1)") return 316;
        app.m_filePath = "example.json"; app.handleKey(commentKey);
        if (app.m_document.toUtf8() != "print(1)" || !app.m_statusIsError) return 317;
        app.m_filePath = "example.cpp";
        input::KeyEvent blockKey{}; blockKey.key = input::Key::A; blockKey.action = input::KeyAction::Press; blockKey.alt = true; blockKey.shift = true;
        app.handleKey(blockKey);
        if (app.m_document.toUtf8() != "/*print(1)*/") return 318;
        app.handleKey(blockKey);
        if (app.m_document.toUtf8() != "print(1)") return 319;
        app.m_filePath = "example.py"; app.handleKey(blockKey);
        if (app.m_document.toUtf8() != "print(1)" || !app.m_statusIsError) return 320;
        app.m_filePath = "example.json"; app.m_document = editor::Document::fromUtf8("");
        app.handleChar(U'"');
        input::KeyEvent pairBackspace{}; pairBackspace.key = input::Key::Backspace; pairBackspace.action = input::KeyAction::Press;
        app.handleKey(pairBackspace);
        if (!app.m_document.toUtf8().empty()) return 321;
        app.m_hasDocument = false; app.m_filePath.clear();
        app.m_languageServer.close(); app.m_languageRoot = "test-language-server";
        app.m_languageNavigationQueued = true; app.m_languageFormatRequest = 7;
        app.pollLanguageServer();
        if (!app.m_languageRoot.empty() || app.m_languageNavigationQueued || app.m_languageFormatRequest != 0 ||
            !app.m_statusIsError || app.m_statusText.find("Language server stopped") == std::string::npos) return 188;
        app.m_languageRenameRequest = 11; app.m_languageRenameDeadline = 10.0;
        app.m_languageNavigationRequest = 12; app.m_languageNavigationDeadline = 12.0;
        app.m_languageFormatRequest = 13; app.m_languageFormatDeadline = 14.0;
        app.expireLanguageRequests(11.0);
        if (app.m_languageRenameRequest || !app.m_languageNavigationRequest || !app.m_languageFormatRequest ||
            !app.m_statusIsError || app.m_statusText.find("Rename timed out") == std::string::npos) return 287;
        app.expireLanguageRequests(13.0);
        if (app.m_languageNavigationRequest || !app.m_languageFormatRequest || app.m_statusText.find("navigation timed out") == std::string::npos) return 288;
        app.expireLanguageRequests(15.0);
        if (app.m_languageFormatRequest || app.m_statusText.find("Formatting timed out") == std::string::npos) return 289;
        app.m_searchBar.open(ui::PromptKind::ProjectSearch);
        input::KeyEvent generatedOption{}; generatedOption.key = input::Key::I; generatedOption.action = input::KeyAction::Press; generatedOption.shift = true;
#if defined(__APPLE__)
        generatedOption.super = true;
#else
        generatedOption.control = true;
#endif
        app.handleKey(generatedOption);
        if (!app.m_projectSearchIncludeGenerated) return 322;
        app.handleKey(generatedOption);
        if (app.m_projectSearchIncludeGenerated) return 323;
        app.m_projectSearchResults = {}; app.m_projectSearchResults.skipped = 1;
        app.m_projectSearchResults.skippedFiles.push_back({"unreadable.cpp", "could not read"});
        for (int i = 0; i < 6; ++i) app.m_projectSearchResults.skippedFiles.push_back({"skipped" + std::to_string(i), "binary content"});
        app.m_projectSearchResults.skipped = 7;
        app.m_searchBar.open(ui::PromptKind::ProjectSearchResults); app.m_projectSearchPage = 0; app.refreshSuggestions();
        auto skippedChoice = std::find(app.m_projectSearchChoices.begin(), app.m_projectSearchChoices.end(), -4);
        if (skippedChoice == app.m_projectSearchChoices.end()) return 324;
        app.m_selectedSuggestion = static_cast<int>(skippedChoice - app.m_projectSearchChoices.begin()); app.applyProjectSearch();
        if (!app.m_projectSearchShowingSkipped || std::find(app.m_suggestionPaths.begin(), app.m_suggestionPaths.end(), "unreadable.cpp") == app.m_suggestionPaths.end()) return 325;
        auto nextSkipped = std::find(app.m_projectSearchChoices.begin(), app.m_projectSearchChoices.end(), -2);
        if (nextSkipped == app.m_projectSearchChoices.end()) return 327;
        app.m_selectedSuggestion = static_cast<int>(nextSkipped - app.m_projectSearchChoices.begin()); app.applyProjectSearch();
        if (app.m_projectSearchPage != 1 || std::find(app.m_suggestionPaths.begin(), app.m_suggestionPaths.end(), "skipped5") == app.m_suggestionPaths.end()) return 328;
        input::KeyEvent closeSkipped{}; closeSkipped.key = input::Key::Escape; closeSkipped.action = input::KeyAction::Press;
        app.handleKey(closeSkipped);
        if (app.m_projectSearchShowingSkipped || app.m_searchBar.kind() != ui::PromptKind::ProjectSearchResults) return 326;
        app.m_searchBar.open(ui::PromptKind::ProjectSearch);
        app.m_projectSearchCancel = std::make_shared<std::atomic_bool>(false);
        const auto cancellation = app.m_projectSearchCancel;
        std::promise<workspace::ProjectSearchResults> cancelledSearch;
        app.m_projectSearchFuture = cancelledSearch.get_future();
        input::KeyEvent cancelSearch{}; cancelSearch.key = input::Key::Escape; cancelSearch.action = input::KeyAction::Press;
        app.handleKey(cancelSearch);
        if (!cancellation->load() || app.m_searchBar.isOpen()) return 197;
        cancelledSearch.set_value({}); app.pollProjectSearch();
        if (app.m_projectSearchFuture.valid() || app.m_projectSearchCancel || app.m_searchBar.isOpen()) return 198;
        app.m_searchBar.open(ui::PromptKind::ProjectSearch);
        input::KeyEvent wordOption{}; wordOption.key = input::Key::W; wordOption.shift = true; wordOption.action = input::KeyAction::Press;
#if defined(__APPLE__)
        wordOption.super = true;
#else
        wordOption.control = true;
#endif
        app.handleKey(wordOption);
        if (!app.m_projectSearchWholeWord || app.m_commandStatus.find("Whole words on") == std::string::npos) return 199;
        app.handleKey(wordOption);
        if (app.m_projectSearchWholeWord) return 200;
        auto caseOption = wordOption; caseOption.key = input::Key::C;
        app.handleKey(caseOption);
        if (app.m_projectSearchCaseSensitive || app.m_commandStatus.find("ASCII case insensitive") == std::string::npos ||
            app.makeFrameState(1.0).commandPlaceholder != "Search project (ASCII case-insensitive)...") return 244;
        app.onCommandQueryChanged();
        if (app.m_commandStatus.find("ASCII case insensitive") == std::string::npos || app.m_commandStatus.find("Whole words off") == std::string::npos) return 246;
        app.handleKey(caseOption);
        if (!app.m_projectSearchCaseSensitive) return 245;
        auto filterOption = wordOption; filterOption.key = input::Key::G;
        app.m_searchBar.setQuery("needle"); app.handleKey(filterOption);
        if (!app.m_projectSearchEditingFilter || app.makeFrameState(1.0).commandPlaceholder != "Filter project paths (text)...") return 247;
        app.m_searchBar.setQuery("!"); app.applyProjectSearch();
        if (!app.m_projectSearchEditingFilter || !app.m_commandStatusIsError) return 251;
        app.m_searchBar.setQuery("src/"); app.applyProjectSearch();
        if (app.m_projectSearchEditingFilter || app.m_projectSearchPathFilter != "src/" || app.m_searchBar.query() != "needle") return 248;
        app.onCommandQueryChanged();
        if (app.m_commandStatus.find("Path: src/") == std::string::npos) return 250;
        app.handleKey(filterOption); app.m_searchBar.setQuery("discard");
        input::KeyEvent cancelFilter{}; cancelFilter.key = input::Key::Escape; cancelFilter.action = input::KeyAction::Press;
        app.handleKey(cancelFilter);
        if (app.m_projectSearchEditingFilter || app.m_projectSearchPathFilter != "src/" || app.m_searchBar.query() != "needle") return 249;
        app.m_projectSearchPathFilter.clear();
        app.m_projectSearchHistory = {"first", "second"}; app.m_projectSearchHistoryPosition = 2;
        app.m_searchBar.setQuery("unfinished");
        input::KeyEvent historyKey{}; historyKey.key = input::Key::Up; historyKey.action = input::KeyAction::Press;
#if defined(__APPLE__)
        historyKey.super = true;
#else
        historyKey.control = true;
#endif
        app.handleKey(historyKey);
        if (app.m_searchBar.query() != "second") return 202;
        app.handleKey(historyKey);
        if (app.m_searchBar.query() != "first") return 203;
        historyKey.key = input::Key::Down; app.handleKey(historyKey); app.handleKey(historyKey);
        if (app.m_searchBar.query() != "unfinished") return 204;
        app.m_projectSearchHistory.clear(); app.m_projectSearchHistoryPosition = 0;
        app.m_projectSearchResults.files.push_back({"sample.cpp", "one needle\nneedle"});
        app.m_projectSearchResults.matches.push_back({0, 4, 0, 4, "one needle"});
        app.m_projectSearchResults.matches.push_back({0, 11, 1, 0, "needle"});
        app.m_projectSearchQuery = "needle"; app.m_projectReplacement = "pin";
        app.m_searchBar.open(ui::PromptKind::ProjectReplaceReview); app.refreshSuggestions();
        const auto matchPreview = std::find(app.m_suggestionDetails.begin(), app.m_suggestionDetails.end(), "2/2 selected; first: one needle");
        if (matchPreview == app.m_suggestionDetails.end()) return 201;
        const auto reviewFile = std::find(app.m_projectSearchChoices.begin(), app.m_projectSearchChoices.end(), 1);
        if (reviewFile == app.m_projectSearchChoices.end()) return 207;
        app.m_selectedSuggestion = static_cast<int>(reviewFile - app.m_projectSearchChoices.begin()); app.applyProjectSearch();
        if (!app.m_projectReviewFile ||
            std::find(app.m_suggestionDetails.begin(), app.m_suggestionDetails.end(), "one needle") == app.m_suggestionDetails.end() ||
            std::find(app.m_suggestionDetails.begin(), app.m_suggestionDetails.end(), "one pin") == app.m_suggestionDetails.end() ||
            std::find(app.m_suggestionDetails.begin(), app.m_suggestionDetails.end(), "pin") == app.m_suggestionDetails.end()) return 208;
        const auto toggleMatch = std::find(app.m_projectSearchChoices.begin(), app.m_projectSearchChoices.end(), -100);
        if (toggleMatch == app.m_projectSearchChoices.end()) return 210;
        app.m_selectedSuggestion = static_cast<int>(toggleMatch - app.m_projectSearchChoices.begin()); app.applyProjectSearch();
        if (app.m_projectSearchSelected.size() != 2 || app.m_projectSearchSelected[0] || !app.m_projectSearchSelected[1]) return 211;
        app.handleKey(cancelSearch);
        if (app.m_projectReviewFile || app.m_searchBar.kind() != ui::PromptKind::ProjectReplaceReview) return 209;
        app.m_projectSearchResults = {}; app.m_projectSearchSelected.clear();
        app.m_searchBar.close();
        const auto savedSettings = app.m_settings;
        const auto savedSettingsRoot = app.m_settingsRoot;
        const auto savedSettingsBaselineRoot = app.m_settingsBaselineRoot;
        const auto savedSettingsBaseline = app.m_settingsBaseline;
        const auto settingsTemp = std::filesystem::weakly_canonical(std::filesystem::temp_directory_path()) /
            ("lucid-settings-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(settingsTemp);
        app.m_settingsRoot = settingsTemp.string();
        if (!app.ensureSettingsFile()) return 154;
        app.loadSettingsFromDisk();
        {
            const auto previousSettings = app.m_settings;
            app.m_settings.aiAskProvider = "openrouter"; app.m_settings.aiCleanupProvider = "local"; app.m_settings.aiDebugProvider = "openrouter";
            app.m_searchBar.open(ui::PromptKind::AiSettings); app.m_aiDownloadingModel = true;
            app.m_aiCancel = std::make_shared<std::atomic_bool>(false);
            app.m_aiProgress = std::make_shared<std::atomic<int>>(ai::kModelVerifying);
            std::promise<Application::AiResult> modelResult; app.m_aiFuture = modelResult.get_future();
            app.pollAi();
            if (app.m_commandStatus.find("Verifying") == std::string::npos) return 181;
            *app.m_aiProgress = ai::kModelInstalling; app.pollAi();
            if (app.m_commandStatus.find("Installing") == std::string::npos) return 182;
            modelResult.set_value({});
            app.pollAi();
            if (app.m_settings.aiAskProvider != "openrouter" || app.m_settings.aiCleanupProvider != "local" ||
                app.m_settings.aiDebugProvider != "openrouter" || app.m_commandStatusIsError ||
                app.m_commandStatus.find("ready") == std::string::npos) return 177;
            app.m_settings = previousSettings; app.m_searchBar.close();
        }
        { std::ifstream initial(settingsTemp / "settings.json");
          const std::string existing((std::istreambuf_iterator<char>(initial)), {});
          if (!app.ensureSettingsFile()) return 155;
          std::ifstream after(settingsTemp / "settings.json");
          const std::string retained((std::istreambuf_iterator<char>(after)), {});
          if (existing.empty() || retained != existing) return 153; }
        { std::ofstream(settingsTemp / "not-a-directory") << "occupied"; }
        app.m_settingsRoot = (settingsTemp / "not-a-directory" / "settings").string();
        app.openSettings();
        if (!app.m_statusIsError || app.m_statusText.find("Could not create settings project") == std::string::npos) return 156;
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
        const auto settingsBaseline = *app.m_settingsBaseline;
        { std::ofstream(settingsTemp / "settings.json", std::ios::trunc) << "{\"external\":true}"; }
        if (app.persistSettings()) return 139;
        { std::ifstream input(settingsTemp / "settings.json");
          const std::string text((std::istreambuf_iterator<char>(input)), {});
          if (text != "{\"external\":true}") return 140; }
        { std::ofstream(settingsTemp / "settings.json", std::ios::trunc) << settingsBaseline; }
        if (!app.persistSettings()) return 141;
        { std::ofstream(settingsTemp / "settings.json", std::ios::trunc) << "{\"zoomFactor\":1.5}"; }
        if (app.persistSettings() || app.m_statusText.find("reopen Settings") == std::string::npos) return 142;
        app.openSettings();
        if (app.m_settings.zoomFactor != 1.5F || !app.m_settingsBaseline ||
            *app.m_settingsBaseline != "{\"zoomFactor\":1.5}") return 143;
        if (!app.persistSettings()) return 144;
        app.m_searchBar.close(); app.m_settingsPanelVisible = true;
        app.activateSettingsControl(ui::SettingsControl::AiSettings);
        if (app.m_searchBar.kind() != ui::PromptKind::AiSettings || app.m_suggestions.size() != 9) return 90;
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
        app.openSettingsJson();
        if (!app.isSettingsDocument()) return 145;
        { std::ofstream(settingsTemp / "settings.json", std::ios::trunc) << "{\"zoomFactor\":2}"; }
        app.openFile("settings.json");
        app.m_document.setCursor(0, 1);
        if (app.m_settings.zoomFactor != 2.0F || !app.m_settingsBaseline ||
            *app.m_settingsBaseline != "{\"zoomFactor\":2}" || !app.persistSettings() ||
            app.m_document.cursorColumn() != 1) return 146;
        app.m_document.insert(U' ');
        app.m_document.undo();
        if (!app.persistSettings()) return 147;
        app.m_document.redo();
        if (!app.m_document.dirty()) return 148;
        app.m_document.undo();
        app.closeProject();
        for (int i = 0; i < 120; ++i) (void)app.makeFrameState(1.0 + i / 60.0);
        const auto moduleTemp = settingsTemp / "Modules";
        std::filesystem::create_directories(moduleTemp / "example");
        { std::ofstream out(moduleTemp / "example/module.json");
          out << R"({"name":"Example","version":"1","requires":["snippet-placeholder-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Insert hello","insert":"hello"},{"title":"Wrap selection","insert":"({{selection}}){{cursor}}!","snippet":true},{"title":"Literal markers","insert":"{{cursor}}"},{"title":"Placeholder","insert":"α\n{{placeholder:β\nx}}!","snippet":true}]})"; }
        app.m_settingsRoot = (settingsTemp / "Settings").string();
        app.m_settingsPanelVisible = false;
        app.openModules();
        if (app.m_searchBar.kind() != ui::PromptKind::ModuleList || app.m_modules.size() != 1) return 65;
        app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::ModuleActions) return 66;
        app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::ModuleList || !app.m_modules[0].enabled ||
            app.m_commandStatus.find("Example enabled") == std::string::npos) return 67;
        app.applyCommand(false);
        app.m_hasDocument = true;
        app.m_document = editor::Document::fromUtf8("");
        app.m_selectedSuggestion = 1; app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::ModuleCommands) return 68;
        app.applyCommand(false);
        if (app.m_document.toUtf8() != "hello") return 69;
        app.m_document.undo(); if (!app.m_document.toUtf8().empty()) return 70;
        app.m_document = editor::Document::fromUtf8("β"); app.m_document.selectAll();
        app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions();
        app.m_selectedSuggestion = 1; app.applyModuleCommand();
        if (app.m_document.toUtf8() != "(β)!" || app.m_document.cursorColumn() != 3) return 350;
        app.m_document.undo(); if (app.m_document.toUtf8() != "β") return 351;
        app.m_document = editor::Document::fromUtf8("");
        app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 2; app.applyModuleCommand();
        if (app.m_document.toUtf8() != "{{cursor}}") return 352;
        app.m_document = editor::Document::fromUtf8("prefix "); app.m_document.setCursor(0, 7);
        app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 3; app.applyModuleCommand();
        if (app.m_document.toUtf8() != "prefix α\nβ\nx!" || app.m_document.selectionUtf8() != "β\nx") return 382;
        app.m_document.insert(U'Z');
        if (app.m_document.toUtf8() != "prefix α\nZ!") return 383;
        app.m_document.undo(); app.m_document.undo(); if (app.m_document.toUtf8() != "prefix ") return 384;
        {
            const auto previousRoot = app.m_moduleRoot; auto previousModules = app.m_modules;
            app.m_moduleRoot = settingsTemp / "MultiSnippetModules"; std::string error;
            const std::string manifest = R"({"name":"Fields","version":"1","requires":["snippet-v1","snippet-placeholders-v2"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Fields","insert":"{{placeholder:α}} + {{placeholder:two\nlines}}{{cursor}}!","snippet":true}]})";
            if (!modules::install(app.m_moduleRoot, "fields", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "fields", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 423;
            app.m_document = editor::Document::fromUtf8("prefix "); app.m_document.setCursor(0, 7); app.m_moduleSelected = 0; app.m_modulePage = 0;
            app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (!app.m_snippetSession || app.m_document.selectionUtf8() != "α") return 424;
            app.m_document.pasteUtf8("longer"); app.handleEditorKey({input::Key::Tab, input::KeyAction::Press}, false);
            if (!app.m_snippetSession || app.m_document.selectionUtf8() != "two\nlines") return 425;
            app.handleEditorKey({input::Key::Tab, input::KeyAction::Press, true}, false);
            if (app.m_document.selectionUtf8() != "longer") return 426;
            app.handleEditorKey({input::Key::Tab, input::KeyAction::Press}, false); app.m_document.pasteUtf8("β");
            app.handleEditorKey({input::Key::Tab, input::KeyAction::Press}, false);
            if (app.m_snippetSession || app.m_document.toUtf8() != "prefix longer + β!" || app.m_document.cursorColumn() != 17) return 427;
            app.m_moduleRoot = previousRoot; app.m_modules = previousModules;
        }
        {
            const auto previousRoot = app.m_moduleRoot; auto previousModules = app.m_modules; const auto previousProvider = app.m_settings.aiDebugProvider;
            app.m_moduleRoot = settingsTemp / "AiModules";
            const std::string manifest = R"({"name":"Explain","version":"1","permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Explain","action":"explain-diagnostic-local"},{"title":"Explain debug","action":"explain-debug-local"}]})";
            std::string error;
            if (!modules::install(app.m_moduleRoot, "explain", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "explain", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 367;
            app.m_settings.aiDebugProvider = "openrouter"; app.m_moduleSelected = 0; app.m_modulePage = 0;
            app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0;
            app.m_activeDiagnosticMessage.clear(); app.applyModuleCommand();
            if (!app.m_commandStatusIsError || app.m_searchBar.kind() != ui::PromptKind::ModuleCommands) return 368;
            app.m_activeDiagnosticMessage = "Expected semicolon"; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::Ai || !app.m_aiDebugExplanation || app.m_aiDebugOpenRouter || app.m_aiDebugContext.find("Expected semicolon") == std::string::npos) return 369;
            app.m_searchBar.close(); app.m_activeDiagnosticMessage.clear();
            auto previousSession = app.m_debugSession;
            app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 1; app.applyModuleCommand();
            if (!app.m_commandStatusIsError || app.m_searchBar.kind() != ui::PromptKind::ModuleCommands) return 370;
            app.m_debugSession.transportClosed("adapter error", false); app.m_selectedSuggestion = 1; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::Ai || !app.m_aiDebugExplanation || app.m_aiDebugOpenRouter || app.m_aiDebugContext.find("adapter error") == std::string::npos) return 371;
            app.m_debugSession = std::move(previousSession); app.m_searchBar.close(); app.m_settings.aiDebugProvider = previousProvider;
            app.m_moduleRoot = previousRoot; app.m_modules = std::move(previousModules); app.m_moduleSelected = 0;
        }
        {
            const auto previousRoot = app.m_moduleRoot; auto previousModules = app.m_modules;
            app.m_moduleRoot = settingsTemp / "PersonalModules"; app.m_modules.clear();
            app.m_document = editor::Document::fromUtf8("β {{cursor}}\nline"); app.m_document.selectAll();
            app.m_searchBar.open(ui::PromptKind::ModuleList); app.m_modulePage = 0; app.refreshSuggestions();
            auto save = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -8);
            if (save == app.m_moduleChoices.end()) return 376;
            app.m_selectedSuggestion = static_cast<int>(save - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleSnippetName) return 377;
            input::KeyEvent cancel{}; cancel.key = input::Key::Escape; cancel.action = input::KeyAction::Press; app.handleKey(cancel);
            if (!app.m_personalSnippetDraft.empty() || app.m_searchBar.kind() != ui::PromptKind::ModuleList) return 378;
            save = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -8);
            if (save == app.m_moduleChoices.end()) return 379;
            app.m_selectedSuggestion = static_cast<int>(save - app.m_moduleChoices.begin()); app.applyModuleCommand();
            app.m_searchBar.setQuery("My snippet"); app.applyModuleCommand();
            if (app.m_modules.size() != 1 || !app.m_modules[0].enabled || app.m_modules[0].name != "My snippet") return 380;
            app.m_document = editor::Document::fromUtf8(""); app.m_moduleSelected = 0; app.m_modulePage = 0;
            app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_document.toUtf8() != "β {{cursor}}\nline") return 381;
            app.m_document = editor::Document::fromUtf8("{{placeholder:name}}({{placeholder:args}}){{cursor}}"); app.m_document.selectAll();
            app.m_searchBar.open(ui::PromptKind::ModuleList); app.m_modulePage = 0; app.refreshSuggestions();
            const auto templateChoice = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -61); if (templateChoice == app.m_moduleChoices.end()) return 499;
            app.m_selectedSuggestion = static_cast<int>(templateChoice - app.m_moduleChoices.begin()); app.applyModuleCommand(); app.m_searchBar.setQuery("My template"); app.applyModuleCommand();
            const auto savedTemplate = std::find_if(app.m_modules.begin(), app.m_modules.end(), [](const auto& module) { return module.name == "My template"; });
            if (savedTemplate == app.m_modules.end() || !savedTemplate->enabled || !savedTemplate->commands[0].snippet) return 500;
            app.m_document = editor::Document::fromUtf8(""); app.m_moduleSelected = static_cast<std::size_t>(savedTemplate - app.m_modules.begin()); app.m_modulePage = 0;
            app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (!app.m_snippetSession || app.m_document.selectionUtf8() != "name") return 501;
            app.m_document.pasteUtf8("call"); app.handleEditorKey({input::Key::Tab, input::KeyAction::Press}, false);
            if (app.m_document.selectionUtf8() != "args") return 502;
            app.m_snippetSession.reset();
            app.m_moduleRoot = previousRoot; app.m_modules = std::move(previousModules); app.m_moduleSelected = 0; app.m_searchBar.close();
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; auto oldDocument = app.m_document; const auto oldPath = app.m_filePath;
            app.m_moduleRoot = settingsTemp / "MarkdownModules"; std::string error;
            const std::string manifest = R"({"name":"Markdown","version":"1","permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Outline","action":"markdown-outline"},{"title":"Preview","action":"markdown-preview"}]})";
            if (!modules::install(app.m_moduleRoot, "markdown", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "markdown", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 401;
            app.m_filePath = "notes.md"; app.m_document = editor::Document::fromUtf8("# First\ntext\n## Second\n"); app.m_hasDocument = true; app.m_moduleSelected = 0; app.m_modulePage = 0;
            app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (!app.m_moduleMarkdownShowing || app.m_moduleMarkdownOutline.headings.size() != 2) return 402;
            auto second = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 1);
            if (second == app.m_moduleChoices.end()) return 403;
            app.m_document.setCursor(1, 0); app.m_document.insert(U'X'); app.m_selectedSuggestion = static_cast<int>(second - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (!app.m_commandStatusIsError || !app.m_searchBar.isOpen()) return 404;
            app.m_moduleMarkdownShowing = false; app.m_modulePage = 0; app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            second = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 1);
            if (second == app.m_moduleChoices.end()) return 405;
            app.m_selectedSuggestion = static_cast<int>(second - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_document.cursorLine() != 2 || app.m_searchBar.isOpen()) return 406;
            const auto beforePreview = app.m_document.toUtf8(); const auto beforeRevision = app.m_document.revision();
            app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 1; app.applyModuleCommand();
            if (!app.m_markdownPreviewShowing || !app.m_markdownPreviewFuture.valid()) return 407;
            const auto previewDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_markdownPreviewFuture.valid() && std::chrono::steady_clock::now() < previewDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_markdownPreviewFuture.valid() || app.m_markdownPreviewLines.empty() || app.m_markdownPreviewLines[0].first != "First" ||
                app.m_markdownPreviewLines[0].second != ui::SuggestionKind::MarkdownHeading || app.m_document.toUtf8() != beforePreview || app.m_document.revision() != beforeRevision) return 408;
            input::KeyEvent dismissPreview{}; dismissPreview.key = input::Key::Escape; dismissPreview.action = input::KeyAction::Press; app.handleKey(dismissPreview);
            if (app.m_markdownPreviewShowing || app.m_searchBar.kind() != ui::PromptKind::ModuleCommands) return 409;
            app.m_document = editor::Document::fromUtf8("Intro\n=====\n\nText\n"); app.m_moduleMarkdownShowing = false; app.m_modulePage = 0; app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_moduleMarkdownOutline.headings.size() != 1 || app.m_moduleMarkdownOutline.headings[0].text != "Intro") return 503;
            const auto setext = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 0); if (setext == app.m_moduleChoices.end()) return 504;
            app.m_selectedSuggestion = static_cast<int>(setext - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_document.cursorLine() != 0 || app.m_searchBar.isOpen()) return 505;
            app.m_document = std::move(oldDocument); app.m_filePath = oldPath; app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = 0;
        }
        const auto importDir = settingsTemp / "starter";
        std::filesystem::create_directories(importDir);
        { std::ofstream out(importDir / "module.json");
          out << R"({"name":"Starter","version":"1","description":"Test module","permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Insert code","insert":"code"}]})"; }
        app.openModules();
        const auto addModule = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -3);
        if (addModule == app.m_moduleChoices.end()) return 116;
        app.m_selectedSuggestion = static_cast<int>(addModule - app.m_moduleChoices.begin()); app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::ModuleImport) return 117;
        app.m_searchBar.setQuery("ghcr.io/invalid"); app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::ModuleImport || !app.m_commandStatusIsError || app.m_moduleFetchFuture.valid()) return 123;
        app.m_searchBar.setQuery((importDir / "module.json").string()); app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::ModulePreview || app.m_modulePreview.description != "Test module" || app.m_modulePreviewSource != (importDir / "module.json").string()) return 118;
        app.m_selectedSuggestion = 1;
        app.handleChar(U'x');
        input::KeyEvent menuBackspace{}; menuBackspace.key = input::Key::Backspace; menuBackspace.action = input::KeyAction::Press;
        app.handleKey(menuBackspace);
        if (!app.m_searchBar.query().empty() || app.m_selectedSuggestion != 1) return 129;
        auto installModule = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 100);
        while (installModule == app.m_moduleChoices.end()) {
            const auto nextPage = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -2);
            if (nextPage == app.m_moduleChoices.end()) return 119;
            app.m_selectedSuggestion = static_cast<int>(nextPage - app.m_moduleChoices.begin()); app.applyCommand(false);
            installModule = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 100);
        }
        if (installModule == app.m_moduleChoices.end()) return 119;
        app.m_selectedSuggestion = static_cast<int>(installModule - app.m_moduleChoices.begin()); app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::ModuleList || app.m_modules.size() != 2 || app.m_modules[1].enabled) return 120;
        app.m_searchBar.setQuery(app.m_modules[1].id); app.refreshSuggestions();
        if (std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 1) == app.m_moduleChoices.end() ||
            std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 0) != app.m_moduleChoices.end()) return 162;
        app.m_searchBar.setQuery(""); app.refreshSuggestions();
        app.m_searchBar.open(ui::PromptKind::ModuleCatalog);
        Application::ModuleFetchResult catalogResult;
        catalogResult.catalog = true; catalogResult.generation = ++app.m_moduleFetchGeneration;
        catalogResult.entries.push_back({"remote-starter", "author/starter", "1.0.0", "Starter", "C++ snippets"});
        catalogResult.entries.push_back({"other", "author/other", "1.0.0", "Other", "Python snippets"});
        std::promise<Application::ModuleFetchResult> catalogPromise;
        app.m_moduleFetchFuture = catalogPromise.get_future(); catalogPromise.set_value(std::move(catalogResult));
        app.pollModules();
        if (app.m_moduleCatalog.size() != 2 || app.m_moduleChoices.size() != 3 || app.m_moduleChoices.back() != -5) return 121;
        app.m_searchBar.setQuery("no such module"); app.refreshSuggestions();
        if (app.m_moduleChoices.size() != 1 || app.m_moduleChoices[0] != -6 ||
            app.m_suggestionPaths[0] != "No matching modules") return 160;
        Application::ModuleFetchResult failedCatalog;
        failedCatalog.generation = ++app.m_moduleFetchGeneration; failedCatalog.catalog = true;
        failedCatalog.error = "Catalog unavailable";
        std::promise<Application::ModuleFetchResult> failedCatalogPromise;
        app.m_moduleFetchFuture = failedCatalogPromise.get_future();
        app.refreshSuggestions();
        if (!app.m_moduleChoices.empty()) return 161;
        failedCatalogPromise.set_value(std::move(failedCatalog)); app.pollModules();
        if (!app.m_moduleChoices.empty() || !app.m_commandStatusIsError ||
            app.m_commandStatus != "Catalog unavailable; showing previous catalog" || app.m_moduleCatalog.size() != 2) return 162;
        app.m_searchBar.setQuery("");
        app.handleChar(U'c'); app.handleChar(U'+'); app.handleChar(U'+');
        if (app.m_moduleChoices.size() != 1 || app.m_moduleChoices[0] != 0) return 124;
        Application::ModuleFetchResult remoteResult;
        remoteResult.generation = ++app.m_moduleFetchGeneration; remoteResult.id = "remote-starter";
        remoteResult.source = "ghcr.io/author/starter:1.0.0";
        remoteResult.manifest = R"({"name":"Remote","version":"1","permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Insert","insert":"code"}]})";
        std::promise<Application::ModuleFetchResult> remotePromise;
        app.m_moduleFetchFuture = remotePromise.get_future(); remotePromise.set_value(std::move(remoteResult));
        app.pollModules();
        if (app.m_searchBar.kind() != ui::PromptKind::ModulePreview || app.m_modulePreview.id != "remote-starter" || app.m_modulePreviewSource != "ghcr.io/author/starter:1.0.0") return 122;
        auto remoteInstall = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 100);
        while (remoteInstall == app.m_moduleChoices.end()) {
            const auto nextPage = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -2);
            if (nextPage == app.m_moduleChoices.end()) return 127;
            app.m_selectedSuggestion = static_cast<int>(nextPage - app.m_moduleChoices.begin()); app.applyCommand(false);
            remoteInstall = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 100);
        }
        app.m_selectedSuggestion = static_cast<int>(remoteInstall - app.m_moduleChoices.begin()); app.applyCommand(false);
        const auto remoteInstalled = std::find_if(app.m_modules.begin(), app.m_modules.end(), [](const auto& module) { return module.id == "remote-starter"; });
        if (app.m_searchBar.kind() != ui::PromptKind::ModuleList || remoteInstalled == app.m_modules.end() || remoteInstalled->enabled) return 128;
        app.m_searchBar.open(ui::PromptKind::ModuleCatalog);
        Application::ModuleFetchResult restrictedResult;
        restrictedResult.generation = ++app.m_moduleFetchGeneration; restrictedResult.id = "restricted";
        restrictedResult.source = "ghcr.io/author/restricted:1";
        restrictedResult.manifest = R"({"name":"Restricted","version":"1","permissions":{"project":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"Insert","insert":"code"}]})";
        std::promise<Application::ModuleFetchResult> restrictedPromise;
        app.m_moduleFetchFuture = restrictedPromise.get_future(); restrictedPromise.set_value(std::move(restrictedResult));
        app.pollModules();
        if (app.m_searchBar.kind() != ui::PromptKind::ModulePreview || !app.m_modulePreview.network || !app.m_commandStatusIsError) return 125;
        if (std::filesystem::exists(app.m_moduleRoot / "restricted")) return 126;
        app.m_searchBar.close();
        app.m_hasDocument = false;
        for (int i = 0; i < 120; ++i) (void)app.makeFrameState(1.0 + i / 60.0);
        app.m_settingsPanelVisible = true;
        app.applySettings(savedSettings);
        app.m_settingsRoot = savedSettingsRoot;
        app.m_settingsBaselineRoot = savedSettingsBaselineRoot;
        app.m_settingsBaseline = savedSettingsBaseline;
        std::filesystem::remove_all(settingsTemp);
        app.handleMouseButton({2.0, 120.0, input::MouseButton::Left, input::KeyAction::Press});
        if (!app.m_settingsPanelVisible) { std::cerr << "settings page dismissed outside controls\n"; return 23; }
        app.m_renderer->drawFrame(app.makeFrameState(0.5));
        app.m_settingsPanelVisible = false;
        app.m_hasProject = true;
        app.m_hasDocument = true;
        app.m_filePath = "example.cpp";
        app.m_document = editor::Document::fromUtf8("hello world\nvoid greet() {}\ngre");
        app.m_document.setCursor(0, 0); app.m_document.extendSelectionTo(0, 5);
        app.handleChar(U'(');
        if (app.m_document.lineUtf8(0) != "(hello) world" || app.m_document.selectionUtf8() != "hello") return 189;
        app.m_document.undo();
        if (app.m_document.lineUtf8(0) != "hello world" || app.m_document.selectionUtf8() != "hello") return 190;
        app.m_document.setCursor(0, 0);
#if defined(__APPLE__)
        app.m_document = editor::Document::fromUtf8("a🙂b");
        app.m_document.setCursor(0, 1); app.m_document.extendSelectionTo(0, 2);
        app.syncAccessibility(app.makeFrameState(0.9));
        if (!checkAccessibility(app.m_window.handle(), "a🙂b", 1, 2)) return 80;
        ui::FrameState accessibleCompletion{};
        accessibleCompletion.completionVisible = true;
        accessibleCompletion.completions = {"alpha", "beta"};
        accessibleCompletion.selectedCompletion = 0;
        app.m_searchBar.close();
        app.syncAccessibility(accessibleCompletion);
        if (!accessibilityLabelContains(app.m_window.handle(), "completion 1 of 2: alpha") ||
            !checkAccessibility(app.m_window.handle(), "a🙂b", 1, 2)) return 81;
        accessibleCompletion.selectedCompletion = 1;
        app.syncAccessibility(accessibleCompletion);
        if (!accessibilityLabelContains(app.m_window.handle(), "completion 2 of 2: beta") ||
            !checkAccessibility(app.m_window.handle(), "a🙂b", 1, 2)) return 82;
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
        app.m_document = editor::Document::fromUtf8("a\t");
        app.m_document.setCursor(0, 2);
        input::KeyEvent indentKey{}; indentKey.key = input::Key::Tab; indentKey.action = input::KeyAction::Press;
        app.handleKey(indentKey);
        if (app.m_document.toUtf8() != "a\t   ") return 130;
        app.m_document.undo();
        if (app.m_document.toUtf8() != "a\t") return 131;
        app.m_document = editor::Document::fromUtf8("first\n  second");
        input::KeyEvent joinKey{}; joinKey.key = input::Key::J; joinKey.action = input::KeyAction::Press;
#if defined(__APPLE__)
        joinKey.super = true;
#else
        joinKey.control = true;
#endif
        app.handleKey(joinKey);
        if (app.m_document.toUtf8() != "first second") return 183;
        app.m_document.undo();
        if (app.m_document.toUtf8() != "first\n  second") return 184;
        app.m_document.setCursor(0, 2);
        input::KeyEvent documentEnd{}; documentEnd.action = input::KeyAction::Press; documentEnd.shift = true;
#if defined(__APPLE__)
        documentEnd.super = true; documentEnd.key = input::Key::Down;
#else
        documentEnd.control = true; documentEnd.key = input::Key::End;
#endif
        app.handleKey(documentEnd);
        if (app.m_document.cursorLine() != 1 || app.m_document.selectionUtf8() != "rst\n  second") return 214;
        app.m_document = editor::Document::fromUtf8("first\n\nsecond");
        app.m_diagnostics = {{0, 3, 1, "First issue"}, {2, 2, 1, "Second issue"}};
        app.m_diagnosticsValid = true; app.m_diagnosticsRevision = app.m_document.revision(); app.m_activeDiagnosticKey.clear();
        app.m_document.setCursor(0, 0);
        input::KeyEvent nextDiagnostic{}; nextDiagnostic.key = input::Key::F8; nextDiagnostic.action = input::KeyAction::Press;
        app.handleKey(nextDiagnostic);
        if (app.m_document.cursorLine() != 0 || app.m_document.cursorColumn() != 2 || app.m_activeDiagnosticMessage != "First issue") return 222;
        app.handleKey(nextDiagnostic);
        if (app.m_document.cursorLine() != 2 || app.m_document.cursorColumn() != 1 || app.m_activeDiagnosticMessage != "Second issue") return 223;
        nextDiagnostic.shift = true; app.handleKey(nextDiagnostic);
        if (app.m_document.cursorLine() != 0 || app.m_activeDiagnosticMessage != "First issue") return 224;
        input::KeyEvent listDiagnostics{}; listDiagnostics.key = input::Key::F8; listDiagnostics.action = input::KeyAction::Press;
#if defined(__APPLE__)
        listDiagnostics.super = true;
#else
        listDiagnostics.control = true;
#endif
        app.handleKey(listDiagnostics);
        if (app.m_searchBar.kind() != ui::PromptKind::Diagnostics || app.m_diagnosticListChoices.size() != 2) return 225;
        app.m_selectedSuggestion = 1; app.applyDiagnosticList();
        if (app.m_searchBar.isOpen() || app.m_document.cursorLine() != 2 || app.m_document.cursorColumn() != 1 ||
            app.m_activeDiagnosticMessage != "Second issue") return 226;
        app.resetDiagnostics();
        app.m_document.setCursor(0, 1);
        input::KeyEvent paragraphKey{}; paragraphKey.key = input::Key::Down; paragraphKey.action = input::KeyAction::Press;
#if defined(__APPLE__)
        paragraphKey.super = true; paragraphKey.alt = true;
#else
        paragraphKey.control = true;
#endif
        app.handleKey(paragraphKey);
        if (app.m_document.cursorLine() != 2 || app.m_document.cursorColumn() != 0 || app.m_document.toUtf8() != "first\n\nsecond") return 215;
        app.m_document = editor::Document::fromUtf8("keep this word");
        app.m_document.setCursor(0, 14);
        input::KeyEvent wordDelete{}; wordDelete.key = input::Key::Backspace; wordDelete.action = input::KeyAction::Press;
#if defined(__APPLE__)
        wordDelete.alt = true;
#else
        wordDelete.control = true;
#endif
        app.handleKey(wordDelete);
        if (app.m_document.toUtf8() != "keep this ") return 185;
        app.m_document.undo();
        if (app.m_document.toUtf8() != "keep this word") return 186;
#if defined(__APPLE__)
        app.m_document.setCursor(0, 14);
        wordDelete.alt = false; wordDelete.super = true;
        app.handleKey(wordDelete);
        if (!app.m_document.toUtf8().empty()) return 187;
        app.m_document.undo();
#endif
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
        input::KeyEvent queryHome{}; queryHome.key = input::Key::Home; queryHome.action = input::KeyAction::Press;
        input::KeyEvent queryBackspace{}; queryBackspace.key = input::Key::Backspace; queryBackspace.action = input::KeyAction::Press;
        app.handleKey(queryHome);
        app.handleKey(queryBackspace);
        if (app.m_document.selectionStart().column != 11 || app.m_searchBar.query() != "alpha") return 114;
        queryHome.key = input::Key::End;
        app.handleKey(queryHome);
        queryBackspace.key = input::Key::Delete;
        app.handleKey(queryBackspace);
        app.handleChar(U'\x01');
        if (app.m_document.selectionStart().column != 11 || app.m_searchBar.query() != "alpha") return 115;
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
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            std::string error; app.m_moduleRoot = temp / "serial-modules";
            const std::string manifest = R"({"name":"Serial","version":"1","requires":["microcontroller-serial-v1","serial-continuous-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false,"devices":true},"commands":[{"title":"Monitor","action":"serial-monitor"}]})";
            if (!modules::install(app.m_moduleRoot, "serial", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "serial", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 650;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (app.m_serialScanFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (!app.m_serialShowing || app.m_serialScanFuture.valid() || app.m_serialFuture.valid()) return 651;
            app.m_serialScan.devices = {{"Test port", std::filesystem::path("/not-a-device")}}; app.refreshSuggestions();
            auto findChoice = [&](int value) { auto it = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), value); if (it == app.m_moduleChoices.end()) return false; app.m_selectedSuggestion = static_cast<int>(it - app.m_moduleChoices.begin()); app.applyModuleCommand(); return true; };
            if (!findChoice(0) || app.m_serialStage != 1 || app.m_serialFuture.valid()) return 652;
            app.refreshSuggestions(); if (std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -111) == app.m_moduleChoices.end() || std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -230) == app.m_moduleChoices.end() || app.m_serialFuture.valid()) return 653;
            app.m_serialMonitor=std::make_shared<modules::SerialMonitorBuffer>(4); app.m_serialMonitor->append(std::string_view("abc\0def",7)); app.m_serialStage=11; app.refreshSuggestions();
            bool binaryNotice=false, droppedNotice=false; for(std::size_t page=0;page<5;++page) {app.m_modulePage=page;app.refreshSuggestions();for(const auto& item:app.m_suggestions){binaryNotice |= item.path=="Binary serial output";droppedNotice |= item.path=="Earlier bytes overwritten";}} if(!binaryNotice || !droppedNotice) return 855;
            app.m_modulePage=0; app.refreshSuggestions();
            if (!findChoice(-110) || app.m_serialShowing || app.m_serialFuture.valid()) return 654;
            app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected;
        }
#if !defined(_WIN32)
        if (std::getenv("LUCID_TEST_GIT_BRANCH")) {
            const auto root = temp / "branch-qa"; std::filesystem::create_directories(root);
            const auto git = [&root](const std::string& args) { return std::system(("git -C '" + root.string() + "' " + args + " >/dev/null 2>&1").c_str()); };
            { std::ofstream file(root / "first.cpp"); file << "initial\n"; }
            { std::ofstream file(root / "second.cpp"); file << "keep locally\n"; }
            { std::ofstream file(root / "old-name.cpp"); file << "historic content\n"; }
            if (git("init -q") || git("config user.name Test") || git("config user.email test@example.com") || git("config core.hooksPath /dev/null") || git("add .") || git("commit -qm initial") || git("switch -qc target")) return 302;
            { std::ofstream file(root / "first.cpp"); file << "target branch\n"; }
            std::filesystem::remove(root / "second.cpp");
            if (git("add -A") || git("commit -qm target") || git("switch -q -") || git("mv old-name.cpp new-name.cpp") || git("commit -qm rename")) return 303;
            const auto originalSettings = app.m_settingsRoot; app.m_settingsRoot = (temp / "branch-settings" / "Settings").string();
            if (!app.openProject(root.string())) return 304;
            {
                const auto moduleRoot = std::filesystem::path(app.m_settingsRoot).parent_path() / "Modules";
                const std::string manifest = R"({"name":"Scanner","version":"1","permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Scan","action":"scan-secrets"}]})";
                std::string error;
                if (!modules::install(moduleRoot, "lucid-secret-scanner", manifest, error) || !modules::setEnabled(moduleRoot, "lucid-secret-scanner", true, error)) return 358;
                { std::ofstream file(root / "credential.txt"); file << "ghp_" << std::string(36, 'A') << "\n"; }
                if (git("add credential.txt")) return 359;
                { std::ofstream file(root / "credential.txt"); file << "safe working tree\n"; }
                app.openGitReview(); app.m_gitPage = 0; app.refreshSuggestions();
                auto review = std::find(app.m_gitChoices.begin(), app.m_gitChoices.end(), -10);
                if (review == app.m_gitChoices.end()) return 360;
                app.m_selectedSuggestion = static_cast<int>(review - app.m_gitChoices.begin()); app.applyGitReview();
                if (!app.m_gitSecretRequired || !app.m_gitSecretFuture.valid()) return 361;
                app.m_selectedSuggestion = 0; app.applyGitReview();
                if (app.m_searchBar.kind() != ui::PromptKind::GitCommitReview) return 362;
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
                while (app.m_gitSecretFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollGitSecretScan(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
                if (!app.m_gitSecretFinished || app.m_gitSecretResult.incomplete || app.m_gitSecretResult.findings.size() != 1) return 363;
                app.m_selectedSuggestion = -1; app.applyGitReview();
                if (app.m_searchBar.kind() != ui::PromptKind::GitCommitReview) return 364;
                app.m_selectedSuggestion = 0; app.applyGitReview();
                if (app.m_searchBar.kind() != ui::PromptKind::GitCommitInput || !app.m_gitSecretAcknowledged) return 365;
                app.m_searchBar.close();
                if (git("reset -q HEAD -- credential.txt")) return 366;
                std::filesystem::remove(root / "credential.txt");
            }
            app.m_searchBar.close(); app.openFile("second.cpp", false); app.openFile("first.cpp", false);
            if (app.m_filePath != "first.cpp" || app.m_inactiveDocuments.empty()) return 305;
            {
                const auto oldRoot = app.m_moduleRoot; const auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
                app.m_moduleRoot = temp / "history-modules"; std::string error;
                const std::string manifest = R"({"name":"History","version":"1","requires":["git-file-history-v1","git-range-history-v1"],"permissions":{"project":true,"network":false,"processes":true,"settings":false},"commands":[{"title":"History","action":"git-file-history"},{"title":"Selected lines","action":"git-range-history"}]})";
                if (!modules::install(app.m_moduleRoot, "history", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "history", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 420;
                app.openFile("new-name.cpp", false);
                app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
                while (app.m_moduleHistoryFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
                if (app.m_moduleHistoryFuture.valid() || app.m_commandStatusIsError || app.m_moduleHistoryEntries.size() != 2 || app.m_moduleHistoryEntries[1].path != "old-name.cpp") return 421;
                app.m_selectedSuggestion = 2; app.applyModuleCommand();
                while (app.m_moduleHistoryFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
                if (app.m_moduleHistoryFuture.valid() || app.m_commandStatusIsError || !app.m_moduleHistoryDiffShowing || std::none_of(app.m_moduleHistoryDiff.lines.begin(), app.m_moduleHistoryDiff.lines.end(), [](const auto& line) { return line.text == "+historic content" && line.line == 0; })) return 422;
                app.m_moduleHistoryShowing = false; app.m_searchBar.close(); app.m_document.setCursor(0, 0); app.m_document.extendSelectionTo(0, 8);
                app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.m_modulePage = 0; app.refreshSuggestions();
                const auto rangeChoice = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 1);
                if (rangeChoice == app.m_moduleChoices.end()) return 676;
                app.m_selectedSuggestion = static_cast<int>(rangeChoice - app.m_moduleChoices.begin()); app.applyModuleCommand();
                while (app.m_moduleHistoryFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
                if (app.m_moduleHistoryFuture.valid() || app.m_commandStatusIsError || !app.m_moduleHistoryRange || app.m_moduleRangeHistory.empty()) return 677;
                const auto historicalChoice = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 0);
                if (historicalChoice == app.m_moduleChoices.end()) return 678;
                app.m_selectedSuggestion = static_cast<int>(historicalChoice - app.m_moduleChoices.begin()); app.applyModuleCommand();
                if (!app.m_moduleHistoryDiffShowing || app.m_moduleHistoryDiff.lines.empty()) return 679;
                app.m_moduleHistoryShowing = false; app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = oldModules; app.m_moduleSelected = oldSelected;
                app.openFile("first.cpp", false);
            }
            app.openGitReview(); app.m_gitPage = 5 / app.pagedContentCount(); app.refreshSuggestions();
            auto branches = std::find(app.m_gitChoices.begin(), app.m_gitChoices.end(), -36);
            if (branches == app.m_gitChoices.end()) return 306;
            app.m_selectedSuggestion = static_cast<int>(branches - app.m_gitChoices.begin()); app.applyGitReview();
            const auto target = std::find_if(app.m_gitBranches.branches.begin(), app.m_gitBranches.branches.end(), [](const auto& branch) { return branch.name == "target"; });
            if (app.m_searchBar.kind() != ui::PromptKind::GitBranches || target == app.m_gitBranches.branches.end()) return 307;
            const auto targetIndex = static_cast<std::size_t>(target - app.m_gitBranches.branches.begin());
            app.m_gitPage = targetIndex / app.pagedContentCount(); app.refreshSuggestions();
            const auto targetChoice = std::find(app.m_gitChoices.begin(), app.m_gitChoices.end(), static_cast<int>(targetIndex));
            if (targetChoice == app.m_gitChoices.end()) return 308;
            app.m_selectedSuggestion = static_cast<int>(targetChoice - app.m_gitChoices.begin()); app.applyGitReview();
            if (app.m_searchBar.kind() != ui::PromptKind::GitBranchReview) return 309;
            app.applyGitReview();
            if (app.m_gitBranchFuture.valid() || app.m_selectedSuggestion != -1) return 314;
            app.m_document.insert(U'X'); app.m_selectedSuggestion = 0; app.applyGitReview();
            if (!app.m_commandStatusIsError || app.m_gitBranchFuture.valid()) return 310;
            app.m_document.undo(); app.m_selectedSuggestion = 0; app.applyGitReview();
            if (!app.m_gitBranchFuture.valid()) return 311;
            const auto switchDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_gitBranchFuture.valid() && std::chrono::steady_clock::now() < switchDeadline) { app.pollGitBranch(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            const auto preserved = std::find_if(app.m_inactiveDocuments.begin(), app.m_inactiveDocuments.end(), [](const auto& document) { return document.path == "second.cpp"; });
            if (app.m_gitBranchFuture.valid() || app.m_document.toUtf8() != "target branch\n" || app.m_inactiveDocuments.empty() ||
                preserved == app.m_inactiveDocuments.end() || !preserved->document.dirty() || preserved->document.toUtf8() != "keep locally\n") {
                std::cerr << "branch recovery failed: " << app.m_commandStatus << "\n"; return 312;
            }
            if (!app.openProject((temp / "nested").string())) return 313;
            app.m_settingsRoot = originalSettings; app.m_searchBar.close();
        }
#endif
        { std::ofstream out(temp / "nested" / "example.txt"); out << "opened externally"; }
        app.openExternalPath((temp / "nested" / "example.txt").string());
        if (!app.m_hasDocument || app.m_document.toUtf8() != "opened externally") return 24;
        {
            const auto oldRoot=app.m_moduleRoot; auto oldModules=app.m_modules; const auto oldSelected=app.m_moduleSelected; std::string error;
            {std::ofstream out(temp/"nested"/"usb-boards.json"); out << R"({"boards":[{"name":"Test board","vendorId":"1234","productId":"abcd","profile":"board.cfg"}]})";}
            app.m_moduleRoot=temp/"usb-board-modules";
            const std::string manifest=R"({"name":"USB","version":"1","requires":["usb-board-identification-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false,"devices":true},"commands":[{"title":"Identify","action":"identify-usb-boards"}]})";
            if(!modules::install(app.m_moduleRoot,"usb-board",manifest,error) || !modules::setEnabled(app.m_moduleRoot,"usb-board",true,error) || !modules::list(app.m_moduleRoot,app.m_modules,error)) return 851;
            app.m_moduleSelected=0; app.m_modulePage=0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion=0; app.applyModuleCommand();
            if(!app.m_usbBoardInput || app.m_searchBar.kind()!=ui::PromptKind::ModuleToolchainPath || app.m_usbBoardFuture.valid()) return 852;
            app.m_searchBar.setQuery("usb-boards.json"); app.applyModuleCommand();
            if(!app.m_usbBoardShowing || app.m_usbBoardConfirmed || app.m_usbBoardFuture.valid() || std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-223)==app.m_moduleChoices.end()) return 853;
            // Inject a descriptor result: interaction tests never enumerate real USB devices.
            app.m_usbBoardConfirmed=true; app.m_usbBoardScan.devices={ {"1-1",{0x1234,0xabcd}}, {"IOKit entry 42",{0x1234,0xabcd}}, {"Windows USB entry 1",{0x1234,0xabcd}} }; app.refreshSuggestions();
            bool linuxVisible=false, macVisible=false, windowsVisible=false; for(std::size_t page=0;page<5;++page){app.m_modulePage=page;app.refreshSuggestions();for(const auto& item:app.m_suggestions) {const bool mapped=item.detail.find("Test board")!=std::string_view::npos && item.detail.find("unverified")!=std::string_view::npos; linuxVisible |= item.path=="1-1" && mapped; macVisible |= item.path=="IOKit entry 42" && mapped; windowsVisible |= item.path=="Windows USB entry 1" && mapped;}} if(!linuxVisible || !macVisible || !windowsVisible) return 854;
            app.m_usbBoardShowing=false; app.m_usbBoardMappings={}; app.m_usbBoardScan={}; app.m_searchBar.close(); app.m_moduleRoot=oldRoot; app.m_modules=std::move(oldModules); app.m_moduleSelected=oldSelected; std::filesystem::remove(temp/"nested"/"usb-boards.json");
        }
#if !defined(_WIN32)
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            { std::ofstream file(temp / "nested" / "board.cfg"); file << "source interface/a.cfg\nsource target/b.cfg\nfirmware firmware.elf\n"; }
            { std::ofstream file(temp / "nested" / "firmware.elf"); file << "reviewed firmware"; }
            const auto executable = temp / "openocd";
            { std::ofstream file(executable); file << "#!/bin/sh\nprintf invoked > \"$0.ran\"\nexit 0\n"; }
            std::filesystem::permissions(executable, std::filesystem::perms::owner_all);
            app.m_project.refresh(); app.openFile("board.cfg", false); app.m_moduleRoot = temp / "flash-modules"; std::string error;
            const std::string manifest = R"({"name":"Flash","version":"1","requires":["openocd-flash-v1","openocd-bin-v1"],"permissions":{"project":true,"network":false,"processes":true,"settings":false,"devices":true},"commands":[{"title":"Flash","action":"flash-firmware"}]})";
            if (!modules::install(app.m_moduleRoot, "flash", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "flash", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 680;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_flashPrepareFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            const auto marker = std::filesystem::path(executable.string() + ".ran");
            if (app.m_flashPrepareFuture.valid() || app.m_searchBar.kind() != ui::PromptKind::ModuleFlashExecutable || app.m_flashSnapshot.bytes != "reviewed firmware" || std::filesystem::exists(marker)) return 681;
            { std::ofstream file(temp / "nested" / "firmware.elf", std::ios::trunc); file << "changed after review"; }
            app.m_searchBar.setQuery(executable.string()); app.applyModuleCommand();
            auto confirm = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -170);
            if (confirm == app.m_moduleChoices.end() || app.m_flashSnapshot.bytes != "reviewed firmware" || std::filesystem::exists(marker)) return 682;
            app.m_selectedSuggestion = static_cast<int>(confirm - app.m_moduleChoices.begin()); app.applyModuleCommand();
            while (app.m_flashFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_flashFuture.valid() || app.m_serialStage != 9 || app.m_commandStatusIsError || !std::filesystem::exists(marker)) return 683;
            app.m_serialShowing=false; app.m_searchBar.close(); std::filesystem::remove(marker);
            {std::ofstream file(temp / "nested" / "board.cfg",std::ios::trunc); file << "source interface/a.cfg\nsource target/b.cfg\nfirmware firmware.bin\n";}
            {std::ofstream file(temp / "nested" / "firmware.bin",std::ios::binary); file << "rawdata";}
            app.openFile("board.cfg",false); app.m_searchBar.open(ui::PromptKind::ModuleCommands);app.m_modulePage=0;app.refreshSuggestions();app.m_selectedSuggestion=0;app.applyModuleCommand();
            if(app.m_serialStage!=12 || app.m_flashPrepareFuture.valid() || app.m_searchBar.kind()!=ui::PromptKind::ModuleFlashExecutable || std::filesystem::exists(marker)) return 856;
            app.m_searchBar.setQuery("");app.applyModuleCommand();if(app.m_serialStage!=12 || !app.m_commandStatusIsError || app.m_flashPrepareFuture.valid()) return 857;
            app.m_searchBar.setQuery("0x08000000");app.applyModuleCommand(); const auto rawDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
            while(app.m_flashPrepareFuture.valid() && std::chrono::steady_clock::now()<rawDeadline){app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(app.m_flashPrepareFuture.valid() || !app.m_flashSnapshot.binaryAddress || *app.m_flashSnapshot.binaryAddress!=0x08000000 || app.m_flashSnapshot.bytes!="rawdata" || std::filesystem::exists(marker)) return 858;
            app.m_searchBar.setQuery(executable.string());app.applyModuleCommand(); bool sawRange=false; for(std::size_t page=0;page<5;++page){app.m_modulePage=page;app.refreshSuggestions();sawRange |= std::any_of(app.m_suggestions.begin(),app.m_suggestions.end(),[](const auto& item){return item.path=="Raw BIN address range" && item.detail.find("0x8000000")!=std::string_view::npos && item.detail.find("7 bytes")!=std::string_view::npos;});} if(!sawRange || std::filesystem::exists(marker)) return 859;
            app.m_serialShowing = false; app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected; app.openFile("example.txt", false);
            std::filesystem::remove(temp / "nested" / "firmware.bin"); std::filesystem::remove(temp / "nested" / "board.cfg"); std::filesystem::remove(temp / "nested" / "firmware.elf"); std::filesystem::remove(executable); std::filesystem::remove(marker);
        }
#endif
#if defined(__APPLE__)
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            { std::ofstream file(temp / "nested" / "web-preview.json"); file << R"({"entry":"index.html","assets":["index.html"]})"; }
            { std::ofstream file(temp / "nested" / "index.html"); file << "<html><body>Preview</body></html>"; }
            app.openFile("web-preview.json", false);
            std::string error; app.m_moduleRoot = temp / "web-modules";
            const std::string manifest = R"({"name":"Web","version":"1","requires":["web-component-preview-macos-v1"],"permissions":{"project":true,"network":true,"processes":false,"settings":false},"commands":[{"title":"Preview","action":"web-preview"}]})";
            if (!modules::install(app.m_moduleRoot, "web", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "web", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 700;
            app.m_moduleSelected = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (!app.m_webShowing || app.m_webStarted || app.m_webBundleFuture.valid()) return 701;
            auto confirm = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -121); if (confirm == app.m_moduleChoices.end()) return 704;
            app.m_selectedSuggestion = static_cast<int>(confirm - app.m_moduleChoices.begin()); app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (!app.m_webReady && app.m_webError.empty() && std::chrono::steady_clock::now() < deadline) { app.m_window.pollEvents(); app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (!app.m_webReady || !app.m_webError.empty()) { std::cerr << "web preview failed: " << app.m_webError << "\n"; return 705; }
            auto cancel = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -120); if (cancel == app.m_moduleChoices.end()) return 702;
            app.m_selectedSuggestion = static_cast<int>(cancel - app.m_moduleChoices.begin()); app.applyModuleCommand(); if (app.m_webShowing || app.m_webBundleFuture.valid()) return 703;
            app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected;
            app.openFile("example.txt", false);
        }
#endif
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            std::string error; app.m_moduleRoot = temp / "pull-request-modules";
            const std::string manifest = R"({"name":"PR","version":"1","requires":["pull-request-companion-v1","pull-request-comment-v1","pull-request-review-v1"],"permissions":{"project":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"Review","action":"fetch-pull-request"}]})";
            if (!modules::install(app.m_moduleRoot, "pull-request", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "pull-request", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 610;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModulePullRequestUrl) return 611;
            app.m_searchBar.setQuery("https://example.com/a/b/pull/1"); app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModulePullRequestUrl || app.m_pullRequestFuture.valid()) return 612;
            app.m_searchBar.setQuery("https://github.com/owner/repo/pull/1"); app.applyModuleCommand();
            if (!app.m_pullRequestShowing || app.m_pullRequestFuture.valid() || app.m_pullRequestFetched) return 613;
            app.refreshSuggestions();
            if (std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -102) == app.m_moduleChoices.end()) return 614;
            app.m_pullRequestData.number = "1"; app.m_pullRequestData.title = "Sample"; modules::PullRequestFile file; file.path = "src/a.cpp"; file.status = "modified"; file.hasPatch = true; file.patch = "@@ -1 +1 @@\n-old\n+new\n"; app.m_pullRequestData.files.push_back(std::move(file)); app.m_pullRequestFetched = true; app.refreshSuggestions();
            app.m_pullRequestData.conversation.push_back({"reviewer", "Please explain", "2026-01-02"}); app.refreshSuggestions();
            if (std::none_of(app.m_suggestions.begin(), app.m_suggestions.end(), [](const auto& item) { return item.path == "Conversation: reviewer · 2026-01-02" && item.detail == "Please explain" && item.kind == ui::SuggestionKind::ReadOnlyText; })) return 910;
            auto fileChoice = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 0); if (fileChoice == app.m_moduleChoices.end()) return 615;
            app.m_selectedSuggestion = static_cast<int>(fileChoice - app.m_moduleChoices.begin()); app.applyModuleCommand();
            const auto patchDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (app.m_pullPatchFuture.valid() && std::chrono::steady_clock::now() < patchDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
            if (!app.m_pullPatchSelected || !app.m_pullPatchError.empty() || app.m_pullPatchHunks.size() != 1 || app.m_pullPatchHunks[0].lines.size() != 2) return 616;
            app.m_pullRequestData.headSha = std::string(40, 'a'); app.refreshSuggestions();
            auto lineChoice = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 1); if (lineChoice == app.m_moduleChoices.end()) return 714;
            app.m_selectedSuggestion = static_cast<int>(lineChoice - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModulePullRequestComment || app.m_pullCommentFuture.valid()) return 715;
            app.m_searchBar.setQuery("Please clarify this line"); app.applyModuleCommand();
            if (!app.m_pullCommentConfirm || app.m_pullCommentFuture.valid()) return 716;
            auto cancelComment = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -106); if (cancelComment == app.m_moduleChoices.end()) return 717;
            app.m_selectedSuggestion = static_cast<int>(cancelComment - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_pullCommentConfirm || app.m_pullCommentFuture.valid()) return 718;
            app.handleKey({input::Key::Escape, input::KeyAction::Press});
            if (app.m_pullPatchSelected || !app.m_pullRequestShowing) return 617;
            app.m_pullRequestData.files[0].patch = "@@ -1,2 +1,2 @@\n one\n"; app.refreshSuggestions();
            fileChoice = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 0); if (fileChoice == app.m_moduleChoices.end()) return 618;
            app.m_selectedSuggestion = static_cast<int>(fileChoice - app.m_moduleChoices.begin()); app.applyModuleCommand();
            while (app.m_pullPatchFuture.valid() && std::chrono::steady_clock::now() < patchDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
            if (app.m_pullPatchError.empty() || !app.m_pullPatchHunks.empty()) return 619;
            app.m_pullPatchSelected.reset(); app.m_pullPatchError.clear();
            app.m_pullRequestData.state = "open"; app.m_modulePage = 0; app.refreshSuggestions();
            auto approve = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -208); if (approve == app.m_moduleChoices.end()) return 834;
            app.m_selectedSuggestion = static_cast<int>(approve - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_pullReviewMode != 1 || app.m_pullCommentFuture.valid()) return 835;
            app.m_searchBar.setQuery(""); app.applyModuleCommand();
            if (!app.m_pullCommentConfirm || app.m_pullCommentFuture.valid() || std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -211) == app.m_moduleChoices.end()) return 836;
            const auto cancelReview = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -212); if (cancelReview == app.m_moduleChoices.end()) return 837;
            app.m_selectedSuggestion = static_cast<int>(cancelReview - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_pullReviewMode || app.m_pullCommentConfirm || app.m_pullCommentFuture.valid()) return 838;
            app.refreshSuggestions();
            const auto requestChanges = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -209); if (requestChanges == app.m_moduleChoices.end()) return 1007;
            app.m_selectedSuggestion = static_cast<int>(requestChanges - app.m_moduleChoices.begin()); app.applyModuleCommand();
            app.m_searchBar.setQuery("   \t "); app.applyModuleCommand();
            if (app.m_pullCommentConfirm || app.m_pullCommentFuture.valid() || app.m_pullReviewMode != 2) return 1008;
            app.handleKey({input::Key::Escape, input::KeyAction::Press});
            app.m_pullRequestData.partial = true; app.refreshSuggestions(); if (std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -208) != app.m_moduleChoices.end()) return 839;
            app.m_pullRequestShowing = false; app.m_pullRequestData = {}; app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected;
        }
#if !defined(_WIN32)
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            const auto compiler = temp / "glslangValidator"; const auto marker = temp / "shader-ran";
            { std::ofstream script(compiler); script << "#!/bin/sh\ntouch '" << marker.string() << "'\nprintf 'ERROR: %s:2: broken shader\\n' \"$6\"\nexit 1\n"; }
            std::filesystem::permissions(compiler, std::filesystem::perms::owner_all);
            { std::ofstream shader(temp / "nested" / "sample.vert"); shader << "#version 450\ninvalid\n#include \"common.glsl\"\n"; }
            { std::ofstream include(temp / "nested" / "common.glsl"); include << "float example;\n"; }
            app.openFile("sample.vert", false);
            if (!app.m_hasDocument || app.m_filePath != "sample.vert") return 630;
            std::string error; app.m_moduleRoot = temp / "shader-modules";
            const std::string manifest = R"({"name":"Shader","version":"1","requires":["shader-compile-v1","shader-artifact-v1","shader-includes-v1","shader-spirv-inspection-v1","shader-render-preview-v1"],"permissions":{"project":true,"network":false,"processes":true,"settings":false},"commands":[{"title":"Compile","action":"compile-shader"}]})";
            if (!modules::install(app.m_moduleRoot, "shader", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "shader", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 631;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleShaderCompiler) return 632;
            app.m_searchBar.setQuery(compiler.string()); app.applyModuleCommand();
            if (!app.m_shaderShowing || app.m_shaderFuture.valid() || std::filesystem::exists(marker)) return 633;
            app.refreshSuggestions(); auto select = [&](int value) { auto it = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), value); if (it == app.m_moduleChoices.end()) return false; app.m_selectedSuggestion = static_cast<int>(it - app.m_moduleChoices.begin()); app.applyModuleCommand(); return true; };
            if (!select(-218)) return 851;
            const auto includesDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (app.m_shaderIncludesFuture.valid() && std::chrono::steady_clock::now() < includesDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_shaderIncludesFuture.valid() || !app.m_shaderIncludesShowing || app.m_shaderIncludes.edges.size() != 1 || std::filesystem::exists(marker)) return 852;
            { std::ofstream changed(temp / "nested" / "sample.vert", std::ios::trunc); changed << "changed\n"; }
            if (!select(0) || !app.m_shaderIncludesShowing || !app.m_commandStatusIsError) return 853;
            { std::ofstream restored(temp / "nested" / "sample.vert", std::ios::trunc); restored << "#version 450\ninvalid\n#include \"common.glsl\"\n"; }
            if (!select(-219)) return 854;
            if (!select(-106)) return 634;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (app.m_shaderFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_shaderFuture.valid() || !app.m_shaderCompleted || app.m_shaderResult.succeeded || app.m_shaderResult.diagnostics.diagnostics.size() != 1 || !std::filesystem::exists(marker)) return 635;
            if (!select(0) || app.m_shaderShowing || app.m_document.cursorLine() != 1) return 636;
            { std::ofstream script(compiler); script << "#!/bin/sh\nprintf '\\003\\002\\043\\007\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000' > \"$5\"\n"; }
            app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            app.m_searchBar.setQuery(compiler.string()); app.applyModuleCommand();
            if (!select(-124) || app.m_searchBar.kind() != ui::PromptKind::ModuleShaderOutput) return 637;
            app.m_searchBar.setQuery("sample.spv"); app.applyModuleCommand();
            if (!app.m_shaderArtifact || app.m_shaderExpectedOutput || std::filesystem::exists(temp / "nested" / "sample.spv")) return 638;
            if (!select(-106)) return 639;
            const auto artifactDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (app.m_shaderFuture.valid() && std::chrono::steady_clock::now() < artifactDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_shaderFuture.valid() || !app.m_shaderResult.succeeded || std::filesystem::file_size(temp / "nested" / "sample.spv") != 20) return 640;
            { std::ofstream artifact(temp / "nested" / "sample.spv", std::ios::binary | std::ios::trunc); for (auto word : {0x07230203U,0x00010600U,0U,8U,0U,0x00020011U,1U,0x0003000eU,0U,1U,0x0006000fU,4U,1U,0x6e69616dU,0U,2U,0x00030010U,1U,7U,0x00040047U,2U,30U,0U,0x00030016U,3U,32U,0x00040017U,4U,3U,4U,0x00040020U,5U,3U,4U,0x0004003bU,5U,2U,3U}) for (unsigned shift=0; shift<32; shift+=8) artifact.put(static_cast<char>(word>>shift)); }
            if (!select(-220)) return 855;
            const auto spirvDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (app.m_spirvFuture.valid() && std::chrono::steady_clock::now() < spirvDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_spirvFuture.valid() || !app.m_spirvShowing || !app.m_spirvResult.error.empty() || app.m_spirvResult.inspection.entryPoints.size()!=1 || app.m_spirvResult.inspection.entryPoints[0].name!="main") return 856;
            if (std::none_of(app.m_suggestions.begin(), app.m_suggestions.end(), [](const auto& item) { return item.path == "SPIR-V metadata" && item.detail.find("Not a rendered preview") != std::string::npos; }) || !select(-221)) return 857;
            if (!select(-222) || app.m_shaderPreviewStage != 1 || app.m_shaderPreviewFuture.valid() || std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-223)==app.m_moduleChoices.end()) return 858;
            if (!select(-225) || app.m_shaderPreviewStage || app.m_shaderPreviewFuture.valid()) return 859;
            app.m_shaderShowing = false; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            app.m_searchBar.setQuery(compiler.string()); app.applyModuleCommand();
            if (!select(-124)) return 641;
            app.m_searchBar.setQuery("sample.spv"); app.applyModuleCommand();
            if (!app.m_shaderExpectedOutput) return 642;
            { std::ofstream changed(temp / "nested" / "sample.spv", std::ios::binary | std::ios::trunc); changed << "external"; }
            if (!select(-106)) return 643;
            const auto conflictDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (app.m_shaderFuture.valid() && std::chrono::steady_clock::now() < conflictDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            std::ifstream preserved(temp / "nested" / "sample.spv", std::ios::binary); std::string contents((std::istreambuf_iterator<char>(preserved)), {});
            if (app.m_shaderFuture.valid() || app.m_shaderResult.succeeded || contents != "external") return 644;
            app.m_shaderShowing = false;
            app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected;
            app.openFile("example.txt", false);
            std::filesystem::remove(temp / "nested" / "common.glsl");
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            const auto runtime = temp / "docker"; const auto marker = temp / "container-ran";
            { std::ofstream script(runtime); script << "#!/bin/sh\nif [ \"$1\" = ps ]; then printf '%s\\n' \"$@\" > '" << marker.string() << "'; printf '%s\\n' '{\"ID\":\"abcdef123456\",\"Image\":\"demo\",\"Names\":\"sample\",\"Status\":\"Up\"}'; else printf '%s %s' \"$1\" \"$2\" > '" << marker.string() << "'; printf 'hello log\\n'; fi\n"; }
            std::filesystem::permissions(runtime, std::filesystem::perms::owner_all);
            std::string error; app.m_moduleRoot = temp / "container-modules";
            const std::string manifest = R"({"name":"Container","version":"1","requires":["container-inspection-v1","container-lifecycle-v1"],"permissions":{"project":false,"network":true,"processes":true,"settings":false},"commands":[{"title":"Inspect","action":"inspect-containers"}]})";
            if (!modules::install(app.m_moduleRoot, "container", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "container", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 600;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleContainerPath) return 601;
            app.m_searchBar.setQuery(runtime.string()); app.applyModuleCommand();
            if (!app.m_containerShowing || app.m_containerStage != 0 || std::filesystem::exists(marker)) return 602;
            auto select = [&](int choice) { app.refreshSuggestions(); auto it = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), choice); if (it == app.m_moduleChoices.end()) return false; app.m_selectedSuggestion = static_cast<int>(it - app.m_moduleChoices.begin()); app.applyModuleCommand(); return true; };
            if (!select(-97)) return 603;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_containerFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_containerFuture.valid() || app.m_containerStage != 1 || app.m_containerListing.containers.size() != 1 || !std::filesystem::exists(marker)) return 604;
            std::filesystem::remove(marker);
            if (!select(0) || app.m_containerStage != 2 || std::filesystem::exists(marker) || !select(-98)) return 605;
            while (app.m_containerFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_containerFuture.valid() || app.m_containerStage != 3 || app.m_containerOutput.find("hello log") == std::string::npos || !std::filesystem::exists(marker)) return 606;
            std::filesystem::remove(marker);
            if (!select(-95) || !select(0) || !select(-131) || app.m_containerStage != 4 || std::filesystem::exists(marker)) return 650;
            if (!select(-134) || app.m_containerStage != 2 || std::filesystem::exists(marker)) return 651;
            if (!select(-132) || !select(-133)) return 652;
            const auto actionDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_containerFuture.valid() && std::chrono::steady_clock::now() < actionDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            std::ifstream action(marker); std::string called((std::istreambuf_iterator<char>(action)), {});
            if (app.m_containerFuture.valid() || app.m_containerStage != 0 || called != "restart abcdef123456") return 653;
            action.close(); std::filesystem::remove(marker);
            if (!app.m_hasProject || !select(-135)) return 654;
            const auto projectDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_containerFuture.valid() && std::chrono::steady_clock::now() < projectDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            std::ifstream projectCall(marker); std::string arguments((std::istreambuf_iterator<char>(projectCall)), {});
            const auto nativeRoot = app.m_project.root().u8string();
            const auto expected = std::string("ps\n--all\n--format\njson\n--filter\nlabel=com.docker.compose.project.working_dir=") + std::string(nativeRoot.begin(), nativeRoot.end()) + "\n";
            if (app.m_containerFuture.valid() || app.m_containerStage != 1 || arguments != expected || app.m_containerProjectRoot != app.m_project.root().generic_string()) return 655;
            app.m_containerShowing = false; app.m_containerOutput.clear(); app.m_containerListing = {}; app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected;
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            if (!app.m_hasProject) return 960;
            const auto runtime = temp / "docker", marker = temp / "container-ran"; std::filesystem::remove(marker);
            const auto config = app.m_project.root() / "lucid-test-actions.json", compose = app.m_project.root() / "lucid-test-compose.yaml";
            { std::ofstream file(config); file << R"({"version":1,"actions":[{"name":"Build","operation":"build","composeFile":"lucid-test-compose.yaml","services":["web"]}]})"; }
            { std::ofstream file(compose); file << "services: {}\n"; }
            std::string error; app.m_moduleRoot = temp / "container-actions-modules";
            const std::string manifest = R"({"name":"Project actions","version":"1","requires":["container-project-actions-v1"],"permissions":{"project":true,"network":true,"processes":true,"settings":false},"commands":[{"title":"Actions","action":"configured-container-actions"}]})";
            if (!modules::install(app.m_moduleRoot,"actions",manifest,error) || !modules::setEnabled(app.m_moduleRoot,"actions",true,error) || !modules::list(app.m_moduleRoot,app.m_modules,error)) return 961;
            app.m_moduleSelected=0; app.m_modulePage=0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion=0; app.applyModuleCommand();
            if (app.m_searchBar.kind()!=ui::PromptKind::ModuleContainerPath) return 962;
            app.m_searchBar.setQuery(runtime.string()); app.applyModuleCommand(); if (app.m_searchBar.kind()!=ui::PromptKind::ModuleContainerActionPath) return 963;
            app.m_searchBar.setQuery("lucid-test-actions.json"); app.applyModuleCommand();
            auto selectAction = [&](int value) { app.refreshSuggestions(); const auto it=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),value); if (it==app.m_moduleChoices.end()) return false; app.m_selectedSuggestion=static_cast<int>(it-app.m_moduleChoices.begin()); app.applyModuleCommand(); return true; };
            if (app.m_containerStage!=5 || !selectAction(0) || std::filesystem::exists(marker)) return 964;
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
            while (app.m_containerActionPrepareFuture.valid() && std::chrono::steady_clock::now()<deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_containerStage!=6 || std::filesystem::exists(marker)) return 965;
            if (std::none_of(app.m_suggestions.begin(),app.m_suggestions.end(),[](const auto& item) { return item.path.find("Confirm build")!=std::string_view::npos && item.detail.find("remote host")!=std::string_view::npos; })) return 966;
            { std::ofstream file(compose); file << "changed\n"; }
            if (!selectAction(-261)) return 967;
            while (app.m_containerFuture.valid() && std::chrono::steady_clock::now()<deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_containerStage!=8 || !app.m_commandStatusIsError || std::filesystem::exists(marker)) return 968;
            { std::ofstream file(compose); file << "services: {}\n"; }
            if (!selectAction(-262) || !selectAction(0)) return 969;
            while (app.m_containerActionPrepareFuture.valid() && std::chrono::steady_clock::now()<deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_containerStage!=6 || !selectAction(-261)) return 970;
            while (app.m_containerFuture.valid() && std::chrono::steady_clock::now()<deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_containerStage!=8 || !std::filesystem::exists(marker) || app.m_commandStatusIsError) return 971;
            app.m_containerShowing=false; app.m_containerActionMode=false; app.m_containerProjectRoot.clear(); app.m_containerActionReview={}; app.m_containerProjectActions.clear(); app.m_containerActionConfigSnapshot.clear(); app.m_searchBar.close(); app.m_moduleRoot=oldRoot; app.m_modules=std::move(oldModules); app.m_moduleSelected=oldSelected;
            std::filesystem::remove(config); std::filesystem::remove(compose); std::filesystem::remove(marker);
        }
#endif
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected; auto oldDocument = app.m_document; const auto oldPath = app.m_filePath;
            std::string error; app.m_moduleRoot = temp / "dependency-modules";
            const std::string manifest = R"({"name":"Dependency","version":"1","requires":["dependency-risk-review-v1","dependency-risk-go-v1","dependency-risk-cargo-v1","dependency-provenance-v1"],"permissions":{"project":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"Review","action":"review-dependency-risk"}]})";
            if (!modules::install(app.m_moduleRoot, "dependency", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "dependency", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 560;
            app.m_document = editor::Document::fromUtf8(R"({"lockfileVersion":3,"packages":{"":{},"node_modules/foo":{"version":"1.2.3","resolved":"https://registry.npmjs.org/foo/-/foo-1.2.3.tgz"}}})");
            app.m_filePath = "package-lock.json"; app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_dependencyFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (!app.m_dependencyShowing || !app.m_dependencyReady || app.m_dependencyQueried || app.m_dependencyReport.packages.size() != 1) return 561;
            auto provenance = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -230);
            if (provenance == app.m_moduleChoices.end()) return 850;
            app.m_selectedSuggestion = static_cast<int>(provenance - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (!app.m_dependencyProvenanceShowing || app.m_dependencyFuture.valid()) return 851;
            auto packageChoice = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 0);
            if (packageChoice == app.m_moduleChoices.end()) return 852;
            app.m_selectedSuggestion = static_cast<int>(packageChoice - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (!app.m_dependencyProvenanceSelected) return 853;
            bool sourceVisible = false, verificationVisible = false;
            for (std::size_t page = 0; page < 5; ++page) { app.m_modulePage = page; app.refreshSuggestions();
                sourceVisible |= std::any_of(app.m_suggestions.begin(), app.m_suggestions.end(), [](const auto& item) { return item.path == "Declared source" && item.detail.find("registry.npmjs.org") != std::string_view::npos; });
                verificationVisible |= std::any_of(app.m_suggestions.begin(), app.m_suggestions.end(), [](const auto& item) { return item.path == "Verification" && item.detail.find("no package contents were checked") != std::string_view::npos; });
            }
            if (!sourceVisible || !verificationVisible) return 854;
            app.m_modulePage = 0; app.refreshSuggestions();
            auto back = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -231); if (back == app.m_moduleChoices.end()) return 855;
            app.m_selectedSuggestion = static_cast<int>(back - app.m_moduleChoices.begin()); app.applyModuleCommand();
            back = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -231); if (back == app.m_moduleChoices.end()) return 856;
            app.m_selectedSuggestion = static_cast<int>(back - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_dependencyProvenanceShowing) return 857;
            app.m_dependencyReport.packages[0].vulnerabilities = {"GHSA-test"}; app.m_dependencyQueried = true; app.refreshSuggestions();
            auto advisory = app.m_suggestions.end();
            for (std::size_t page = 0; page < 10; ++page) { app.m_modulePage = page; app.refreshSuggestions(); advisory = std::find_if(app.m_suggestions.begin(), app.m_suggestions.end(), [](const auto& item) { return item.path.find("GHSA-test") != std::string_view::npos && item.path.find("foo") == std::string_view::npos; }); if (advisory != app.m_suggestions.end()) break; }
            if (advisory == app.m_suggestions.end()) return 564;
            app.m_selectedSuggestion = static_cast<int>(advisory - app.m_suggestions.begin()); app.applyModuleCommand();
            if (app.m_dependencySelectedId != "GHSA-test" || app.m_dependencyDetailFuture.valid()) return 565;
            app.m_dependencySelectedId.clear(); app.m_dependencyQueried = false; app.m_dependencyReport.packages[0].vulnerabilities.clear(); app.m_modulePage = 0; app.refreshSuggestions();
            app.m_document.pasteUtf8(" "); app.m_selectedSuggestion = 1; app.applyModuleCommand();
            if (app.m_dependencyFuture.valid() || app.m_dependencyQueried || !app.m_commandStatusIsError) return 562;
            app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_dependencyShowing) return 563;
            app.m_document = editor::Document::fromUtf8("example.com/lib v1.2.3 h1:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\nexample.com/lib v1.2.3/go.mod h1:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\n");
            app.m_filePath = "go.sum"; app.m_modulePage = 0; app.m_selectedSuggestion = 0; app.applyModuleCommand();
            const auto goDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_dependencyFuture.valid() && std::chrono::steady_clock::now() < goDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (!app.m_dependencyShowing || !app.m_dependencyReady || app.m_dependencyQueried || app.m_dependencyReport.packages.size() != 1 || app.m_dependencyReport.packages[0].ecosystem != "Go") return 830;
            if (app.m_dependencyReport.limitations.empty() || app.m_dependencyReport.limitations[0].find("not the selected build list") == std::string::npos) return 831;
            app.m_modulePage = 0; app.refreshSuggestions();
            if (std::none_of(app.m_suggestions.begin(), app.m_suggestions.end(), [](const auto& item) { return item.path.find("Go packages with OSV") != std::string_view::npos; })) return 832;
            app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_dependencyShowing || app.m_dependencyFuture.valid()) return 833;
            const std::string cargo = "version = 4\n[[package]]\nname = \"serde\"\nversion = \"1.0.200\"\nsource = \"registry+https://github.com/rust-lang/crates.io-index\"\nchecksum = \"" + std::string(64, 'a') + "\"\n";
            app.m_document = editor::Document::fromUtf8(cargo); app.m_filePath = "Cargo.lock"; app.m_modulePage = 0; app.m_selectedSuggestion = 0; app.applyModuleCommand();
            const auto cargoDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_dependencyFuture.valid() && std::chrono::steady_clock::now() < cargoDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (!app.m_dependencyShowing || !app.m_dependencyReady || app.m_dependencyQueried || app.m_dependencyReport.packages.size() != 1 || app.m_dependencyReport.packages[0].ecosystem != "crates.io") return 840;
            if (app.m_dependencyReport.limitations.empty() || app.m_dependencyReport.limitations[0].find("workspace, Git, alternate registry") == std::string::npos) return 841;
            app.m_modulePage = 0; app.refreshSuggestions();
            if (std::none_of(app.m_suggestions.begin(), app.m_suggestions.end(), [](const auto& item) { return item.path.find("crates.io packages with OSV") != std::string_view::npos; })) return 842;
            app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_dependencyShowing || app.m_dependencyFuture.valid()) return 843;
            app.m_document = std::move(oldDocument); app.m_filePath = oldPath; app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected; app.m_searchBar.close();
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            const auto dir = app.m_project.root() / "artifact-risk"; std::filesystem::create_directories(dir);
            const std::string lock = R"({"lockfileVersion":3,"packages":{"":{},"node_modules/foo":{"version":"1.2.3","resolved":"https://registry.npmjs.org/foo/-/foo-1.2.3.tgz","integrity":"sha512-3a81oZNherrMQXNJriBBMRLm+k6JqX6iCp7u5ktV05ohkpkqJ0/BqDa6PCOj/uu9RU1EI2Q86A4qmslPpUyknw=="}}})";
            { std::ofstream out(dir / "package-lock.json"); out << lock; } { std::ofstream out(dir / "package.tgz",std::ios::binary); out << "abc"; }
            app.openFile("artifact-risk/package-lock.json",false); if (app.m_filePath!="artifact-risk/package-lock.json") return 990;
            std::string error; app.m_moduleRoot=temp/"artifact-risk-modules";
            const std::string manifest = R"({"name":"Artifacts","version":"1","requires":["dependency-risk-review-v1","dependency-provenance-v1","dependency-artifact-verify-v1","dependency-artifact-sha2-v2","dependency-risk-go-v1","dependency-artifact-go-v1"],"permissions":{"project":true,"network":true,"processes":false,"settings":false},"commands":[{"title":"Review","action":"review-dependency-risk"}]})";
            if (!modules::install(app.m_moduleRoot,"artifact",manifest,error) || !modules::setEnabled(app.m_moduleRoot,"artifact",true,error) || !modules::list(app.m_moduleRoot,app.m_modules,error)) return 991;
            app.m_moduleSelected=0; app.m_modulePage=0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion=0; app.applyModuleCommand();
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
            while (app.m_dependencyFuture.valid() && std::chrono::steady_clock::now()<deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            auto selectArtifact = [&](int code) { for (std::size_t page=0;page<10;++page) { app.m_modulePage=page; app.refreshSuggestions(); auto it=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),code); if (it!=app.m_moduleChoices.end()) { app.m_selectedSuggestion=static_cast<int>(it-app.m_moduleChoices.begin()); app.applyModuleCommand(); return true; } } return false; };
            if (!app.m_dependencyReady || !selectArtifact(-230) || !selectArtifact(0) || !selectArtifact(-275) || app.m_searchBar.kind()!=ui::PromptKind::ModuleDependencyArtifactPath) return 992;
            app.m_searchBar.setQuery("artifact-risk/package.tgz"); app.applyModuleCommand();
            if (app.m_dependencyArtifactStage!=1 || app.m_dependencyArtifactFuture.valid()) return 993;
            { std::ofstream out(dir / "package-lock.json",std::ios::trunc); out << "changed"; }
            if (!selectArtifact(-276) || !app.m_commandStatusIsError || app.m_dependencyArtifactFuture.valid()) return 994;
            { std::ofstream out(dir / "package-lock.json",std::ios::trunc); out << lock; }
            if (!selectArtifact(-276)) return 995;
            while (app.m_dependencyArtifactFuture.valid() && std::chrono::steady_clock::now()<deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_dependencyArtifactFuture.valid() || app.m_dependencyArtifactStage!=3 || app.m_dependencyArtifactResult.status!=modules::DependencyArtifactStatus::Matched) return 996;
            bool warning=false; for (std::size_t page=0; page<5; ++page) { app.m_modulePage=page; app.refreshSuggestions(); warning |= std::any_of(app.m_suggestions.begin(),app.m_suggestions.end(),[](const auto& item) { return item.path=="Checksum matches lockfile" && item.detail.find("not verified")!=std::string_view::npos; }); } if (!warning) return 997;
            app.m_dependencyShowing=false; app.m_dependencyArtifactStage=0; app.m_dependencyProvenanceShowing=false; app.m_dependencyProvenanceSelected.reset(); app.m_searchBar.close();
            const auto fixture=std::filesystem::path(__FILE__).parent_path().parent_path()/"fixtures/go-module-uppercase-v3.2.1.zip";
            std::filesystem::copy_file(fixture,dir/"module.zip",std::filesystem::copy_options::overwrite_existing);
            { std::ofstream out(dir/"go.sum"); out << "github.com/Masterminds/semver/v3 v3.2.1 h1:RN9w6+7QoMeJVGyfmbcgs28Br8cvmnucEXnY0rYXWg0=\n"; }
            app.openFile("artifact-risk/go.sum",false); if (app.m_filePath!="artifact-risk/go.sum") return 998;
            app.m_modulePage=0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion=0; app.applyModuleCommand();
            const auto goDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
            while (app.m_dependencyFuture.valid() && std::chrono::steady_clock::now()<goDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (!app.m_dependencyReady || !selectArtifact(-230) || !selectArtifact(0) || !selectArtifact(-275)) return 999;
            app.m_searchBar.setQuery("artifact-risk/module.zip"); app.applyModuleCommand(); if (app.m_dependencyArtifactStage!=1 || !selectArtifact(-276)) return 1000;
            while (app.m_dependencyArtifactFuture.valid() && std::chrono::steady_clock::now()<goDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_dependencyArtifactFuture.valid() || app.m_dependencyArtifactStage!=3 || app.m_dependencyArtifactResult.status!=modules::DependencyArtifactStatus::Matched) return 1001;
            app.m_dependencyShowing=false; app.m_dependencyArtifactStage=0; app.m_dependencyProvenanceShowing=false; app.m_dependencyProvenanceSelected.reset(); app.m_searchBar.close(); app.m_moduleRoot=oldRoot; app.m_modules=std::move(oldModules); app.m_moduleSelected=oldSelected; app.openFile("example.txt",false); std::filesystem::remove_all(dir);
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected; auto oldDocument = app.m_document; const auto oldPath = app.m_filePath;
            std::string error; app.m_moduleRoot = temp / "api-template-modules";
            const std::string manifest = R"({"name":"API editor","version":"1","requires":["api-collection-edit-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Edit","action":"edit-api-collection"}]})";
            if (!modules::install(app.m_moduleRoot, "api-editor", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "api-editor", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 870;
            const std::string original = R"({"variables":{"TOKEN":"secret"},"requests":[{"name":"old","url":"https://x/{{TOKEN}}"}]})";
            app.m_document = editor::Document::fromUtf8(original); app.m_filePath = "requests.json"; app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            auto select = [&](int code) { const auto it = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), code); if (it == app.m_moduleChoices.end()) return false; app.m_selectedSuggestion = static_cast<int>(it - app.m_moduleChoices.begin()); app.applyModuleCommand(); return true; };
            if (!app.m_apiEditMode || !select(-245) || app.m_searchBar.kind() != ui::PromptKind::ModuleApiTemplateName) return 871;
            app.m_searchBar.setQuery("new"); app.applyModuleCommand(); if (app.m_apiEditNames.size() != 2 || app.m_apiEditNames[1] != "new" || app.m_document.toUtf8().find("secret") == std::string::npos) return 872;
            if (!select(1) || !select(-246)) return 873; app.m_searchBar.setQuery("renamed"); app.applyModuleCommand(); if (app.m_apiEditNames[1] != "renamed") return 874;
            if (!select(1) || !select(-247) || !app.m_apiEditConfirm || app.m_apiEditNames.size() != 2) return 875;
            if (!select(-248) || app.m_apiEditNames.size() != 1) return 876;
            app.m_document.undo(); if (app.m_document.toUtf8().find("renamed") == std::string::npos) return 877;
            app.m_apiShowing = false; app.m_apiEditMode = false; app.m_apiEditNames.clear(); app.m_apiEditBaseline.clear(); app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (!select(1) || !select(-247)) return 878;
            app.m_document.pasteUtf8(" "); const auto changed = app.m_document.toUtf8();
            if (!select(-248) || !app.m_commandStatusIsError || app.m_document.toUtf8() != changed) return 879;
            app.m_apiShowing = false; app.m_apiEditMode = false; app.m_apiEditNames.clear(); app.m_apiEditBaseline.clear(); app.m_document = std::move(oldDocument); app.m_filePath = oldPath; app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected; app.m_searchBar.close();
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected; auto oldDocument = app.m_document; const auto oldPath = app.m_filePath;
            std::string error; app.m_moduleRoot = temp / "api-modules";
            const std::string manifest = R"({"name":"API","version":"1","requires":["api-request-studio-v1"],"permissions":{"project":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"Requests","action":"open-api-requests"}]})";
            if (!modules::install(app.m_moduleRoot, "api", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "api", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 540;
            app.m_document = editor::Document::fromUtf8(R"({"variables":{"PORT":"1"},"requests":[{"name":"Local","method":"GET","url":"http://127.0.0.1:{{PORT}}/private?token=secret"}]})");
            app.m_filePath = "requests.json"; app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (!app.m_apiShowing || app.m_apiFuture.valid() || app.m_apiRequests.size() != 1) return 541;
            app.m_selectedSuggestion = 1; app.applyModuleCommand();
            if (!app.m_apiSelected || app.m_apiFuture.valid() || app.m_apiConfirmed) return 542;
            app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_apiShowing || app.m_apiFuture.valid() || !app.m_apiRequests.empty()) return 543;
            app.m_selectedSuggestion = 0; app.applyModuleCommand(); app.m_selectedSuggestion = 1; app.applyModuleCommand();
            app.m_selectedSuggestion = 1; app.applyModuleCommand();
            if (!app.m_apiConfirmed || !app.m_apiFuture.valid() || !app.m_apiRequests.empty()) return 544;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (app.m_apiFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_apiFuture.valid() || app.m_apiResponse.error.empty()) return 545;
            app.m_apiResponse={}; app.m_apiResponse.status=200; app.m_apiResponse.elapsedMilliseconds=123; app.m_apiResponse.headers="X-Test: header-value\n"; app.m_apiResponse.body="body-value\n"; app.m_apiResponseView=0; app.refreshSuggestions();
            bool durationVisible=false; for (std::size_t page=0; page<5 && !durationVisible; ++page) { app.m_modulePage=page; app.refreshSuggestions(); durationVisible=std::any_of(app.m_suggestions.begin(),app.m_suggestions.end(),[](const auto& item) { return item.path == "Response size" && item.detail.find("123 ms") != std::string_view::npos; }); } if (!durationVisible) return 824;
            app.m_modulePage=0; app.refreshSuggestions();
            auto headers=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-199); if (headers==app.m_moduleChoices.end()) return 820;
            app.m_selectedSuggestion=static_cast<int>(headers-app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_apiResponseView!=1 || std::none_of(app.m_suggestions.begin(),app.m_suggestions.end(),[](const auto& item) { return item.path.find("header-value")!=std::string_view::npos; }) || std::any_of(app.m_suggestions.begin(),app.m_suggestions.end(),[](const auto& item) { return item.path.find("body-value")!=std::string_view::npos; })) return 821;
            auto body=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-200); if (body==app.m_moduleChoices.end()) return 822;
            app.m_selectedSuggestion=static_cast<int>(body-app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_apiResponseView!=2 || std::none_of(app.m_suggestions.begin(),app.m_suggestions.end(),[](const auto& item) { return item.path.find("body-value")!=std::string_view::npos; }) || std::any_of(app.m_suggestions.begin(),app.m_suggestions.end(),[](const auto& item) { return item.path.find("header-value")!=std::string_view::npos; })) return 823;
            app.m_selectedSuggestion = 0; app.applyModuleCommand();
            { std::ofstream env(temp / "nested" / "api-env.json"); env << R"({"environments":[{"name":"local","variables":{"PORT":{"type":"number","value":"4444"},"TOKEN":{"type":"string","value":"private-test-token","secret":true}}}]})"; }
            app.m_moduleRoot = temp / "api-env-modules";
            const std::string environmentManifest = R"({"name":"API Profiles","version":"1","requires":["api-environments-v1","api-response-save-v1","api-request-studio-v1","api-response-assertions-v1","api-environment-preflight-v1"],"permissions":{"project":true,"network":true,"processes":false,"settings":false},"commands":[{"title":"Profiles","action":"open-api-environment"}]})";
            if (!modules::install(app.m_moduleRoot, "api-profile", environmentManifest, error) || !modules::setEnabled(app.m_moduleRoot, "api-profile", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 546;
            app.m_document = editor::Document::fromUtf8(R"({"requests":[{"name":"Profile request","method":"GET","url":"http://127.0.0.1:{{PORT}}/","headers":{"Authorization":"{{TOKEN}}"}}]})");
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleApiEnvironmentPath || app.m_apiFuture.valid()) return 547;
            app.m_searchBar.setQuery("api-env.json"); app.applyModuleCommand();
            if (!app.m_apiChoosingEnvironment || app.m_apiEnvironments.size() != 1 || app.m_apiFuture.valid()) return 548;
            if (std::any_of(app.m_suggestions.begin(), app.m_suggestions.end(), [](const auto& item) { return item.path.find("private-test-token") != std::string::npos || item.detail.find("private-test-token") != std::string::npos; })) return 549;
            auto checkProfile=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-5000);if(checkProfile==app.m_moduleChoices.end())return 1083;app.m_selectedSuggestion=static_cast<int>(checkProfile-app.m_moduleChoices.begin());app.applyModuleCommand();
            const auto preflightDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);while(app.m_apiPreflightFuture.valid() && std::chrono::steady_clock::now()<preflightDeadline){app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(app.m_apiPreflightFuture.valid() || !app.m_apiPreflightShowing || app.m_apiPreflight.requests.size()!=1 || app.m_apiPreflight.requests[0].references.size()!=2 || app.m_apiFuture.valid())return 1084;
            auto requestRow=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),0);if(requestRow==app.m_moduleChoices.end())return 1085;app.m_selectedSuggestion=static_cast<int>(requestRow-app.m_moduleChoices.begin());app.applyModuleCommand();
            bool sawSecretName=false;for(std::size_t page=0;page<10;++page){app.m_modulePage=page;app.refreshSuggestions();for(const auto& item:app.m_suggestions){sawSecretName |= item.path=="TOKEN" && item.detail.find("Secret-marked")!=std::string_view::npos;if(item.path.find("private-test-token")!=std::string_view::npos || item.detail.find("private-test-token")!=std::string_view::npos)return 1086;}}if(!sawSecretName)return 1087;
            app.m_modulePage=0;app.refreshSuggestions();auto backProfile=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-317);if(backProfile==app.m_moduleChoices.end())return 1088;app.m_selectedSuggestion=static_cast<int>(backProfile-app.m_moduleChoices.begin());app.applyModuleCommand();backProfile=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-317);if(backProfile==app.m_moduleChoices.end())return 1089;app.m_selectedSuggestion=static_cast<int>(backProfile-app.m_moduleChoices.begin());app.applyModuleCommand();if(app.m_apiPreflightShowing || !app.m_apiChoosingEnvironment)return 1090;
            app.m_selectedSuggestion = 2; app.applyModuleCommand();
            if (app.m_apiChoosingEnvironment || app.m_apiRequests.size() != 1 || app.m_apiRequests[0].headers.at("Authorization") != "private-test-token" || app.m_apiFuture.valid()) return 550;
            app.m_selectedSuggestion = 1; app.applyModuleCommand();
            if (!app.m_apiSelected || app.m_apiConfirmed || app.m_apiFuture.valid() || std::any_of(app.m_suggestions.begin(), app.m_suggestions.end(), [](const auto& item) { return item.path.find("private-test-token") != std::string::npos || item.detail.find("private-test-token") != std::string::npos; })) return 551;
            app.m_apiConfirmed=true; app.m_apiResponse={}; app.m_apiResponse.status=200; app.m_apiResponse.body=std::string("body\0data",9); app.m_apiResponseView=0; app.m_apiSaveStage=0; app.refreshSuggestions();
            { std::ofstream assertions(temp/"nested"/"checks.json"); assertions << R"({"assertions":[{"type":"status","value":200},{"type":"body_contains","value":"body"},{"type":"body_contains","value":"private-expected"}]})"; }
            auto assertChoice=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-303); if (assertChoice==app.m_moduleChoices.end()) return 1020;
            app.m_selectedSuggestion=static_cast<int>(assertChoice-app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_searchBar.kind()!=ui::PromptKind::ModuleApiSavePath || app.m_apiAssertionStage!=1) return 1021;
            app.m_searchBar.setQuery("checks.json"); app.applyModuleCommand();
            const auto assertDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
            while (app.m_apiAssertionFuture.valid() && std::chrono::steady_clock::now()<assertDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_apiAssertionFuture.valid() || app.m_apiAssertionStage!=3 || !app.m_apiAssertions.error.empty() || app.m_apiAssertions.results.size()!=3 || !app.m_apiAssertions.results[0].passed || !app.m_apiAssertions.results[1].passed || app.m_apiAssertions.results[2].passed) return 1022;
            for (std::size_t page=0;page<10;++page) { app.m_modulePage=page; app.refreshSuggestions(); for (const auto& item:app.m_suggestions) if (item.path.find("private-expected")!=std::string_view::npos || item.detail.find("private-expected")!=std::string_view::npos || item.path.find(std::string_view("body\0data",9))!=std::string_view::npos || item.detail.find(std::string_view("body\0data",9))!=std::string_view::npos) return 1023; }
            std::filesystem::remove(temp/"nested"/"checks.json"); app.m_apiAssertionStage=0; app.m_apiAssertions={}; app.m_modulePage=0; app.refreshSuggestions();
            auto saveResponse=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-202); if (saveResponse==app.m_moduleChoices.end()) return 824;
            app.m_selectedSuggestion=static_cast<int>(saveResponse-app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_searchBar.kind()!=ui::PromptKind::ModuleApiSavePath) return 825;
            app.m_searchBar.setQuery("response.bin"); app.applyModuleCommand(); auto confirmSave=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-201);
            if (confirmSave==app.m_moduleChoices.end() || std::filesystem::exists(temp/"nested"/"response.bin")) return 826;
            app.m_selectedSuggestion=static_cast<int>(confirmSave-app.m_moduleChoices.begin()); app.applyModuleCommand();
            while (app.m_apiSaveFuture.valid() && std::chrono::steady_clock::now()<deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            std::ifstream responseFile(temp/"nested"/"response.bin",std::ios::binary); const std::string responseBytes((std::istreambuf_iterator<char>(responseFile)),{});
            if (app.m_apiSaveFuture.valid() || app.m_apiSaveStage!=4 || responseBytes!=std::string("body\0data",9)) return 827;
            std::filesystem::remove(temp/"nested"/"response.bin");
            app.m_selectedSuggestion = 0; app.applyModuleCommand(); std::filesystem::remove(temp / "nested" / "api-env.json");
            app.m_document = std::move(oldDocument); app.m_filePath = oldPath; app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected; app.m_searchBar.close();
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected; auto oldDocument = app.m_document; const auto oldPath = app.m_filePath; auto oldProject = app.m_project; const auto oldHasProject = app.m_hasProject;
            bool created = false; std::string error; app.m_project = workspace::Project::openOrCreate(temp / "sql-project", created, error); app.m_hasProject = true;
            sqlite3* database = nullptr;
            if (sqlite3_open((app.m_project.root() / "sample.db").string().c_str(), &database) != SQLITE_OK) return 530;
            if (sqlite3_exec(database, "CREATE TABLE items(name TEXT); INSERT INTO items VALUES('one'), ('two');", nullptr, nullptr, nullptr) != SQLITE_OK) return 531;
            sqlite3_close(database);
            app.m_moduleRoot = temp / "sql-modules";
            const std::string manifest = R"({"name":"SQL","version":"1","requires":["sql-snapshot-v1","sql-parameters-v1","sql-columns-v1","sql-change-copy-v1","sql-change-bound-v1","sql-change-row-diff-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Run","action":"sql-query"},{"title":"Plan","action":"sql-explain"},{"title":"Bound","action":"sql-query-bound"},{"title":"Change copy","action":"sql-change-preview"},{"title":"Bound change","action":"sql-change-bound"}]})";
            if (!modules::install(app.m_moduleRoot, "sql", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "sql", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 532;
            app.m_document = editor::Document::fromUtf8("SELECT name FROM items ORDER BY name"); app.m_filePath = "query.sql"; app.m_moduleSelected = 0; app.m_modulePage = 0;
            app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleSqlPath) return 533;
            app.m_searchBar.setQuery("sample.db"); app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_sqlFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (!app.m_sqlShowing || !app.m_sqlResult.error.empty() || app.m_sqlResult.rows.size() != 2 || app.m_sqlResult.rows[0][0].value != "one") return 534;
            if (sqlite3_open((app.m_project.root()/"sample.db").string().c_str(),&database)!=SQLITE_OK) return 936;
            if (sqlite3_exec(database,"CREATE TABLE added_after_query(x)",nullptr,nullptr,nullptr)!=SQLITE_OK) return 937;
            sqlite3_close(database);
            auto schemaChoice=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-240);
            if (schemaChoice==app.m_moduleChoices.end()) return 930;
            app.m_selectedSuggestion=static_cast<int>(schemaChoice-app.m_moduleChoices.begin()); app.applyModuleCommand();
            while (app.m_sqlSchemaFuture.valid() && std::chrono::steady_clock::now()<deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_sqlSchemaFuture.valid() || !app.m_sqlSchemaShowing || !app.m_sqlSchema.error.empty() || app.m_sqlSchema.objects.size()!=1 || app.m_sqlSchema.objects[0].name!="items") return 931;
            auto schemaObject=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),0);
            if (schemaObject==app.m_moduleChoices.end()) return 932;
            app.m_selectedSuggestion=static_cast<int>(schemaObject-app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (!app.m_sqlSchemaSelected || app.m_sqlSchema.objects[*app.m_sqlSchemaSelected].definition.find("CREATE TABLE items")==std::string::npos) return 933;
            while (app.m_sqlColumnsFuture.valid() && std::chrono::steady_clock::now()<deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_sqlColumnsFuture.valid() || !app.m_sqlColumns.error.empty() || app.m_sqlColumns.columns.size()!=1 || app.m_sqlColumns.columns[0].name!="name" || app.m_sqlColumns.columns[0].declaredType!="TEXT") return 950;
            if (std::none_of(app.m_suggestions.begin(),app.m_suggestions.end(),[](const auto& item) { return item.path=="name" && item.detail.find("TEXT")!=std::string_view::npos; })) return 951;
            app.m_selectedSuggestion=0; app.applyModuleCommand();
            if (app.m_sqlSchemaSelected || !app.m_sqlSchemaShowing) return 934;
            schemaObject=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),0); if (schemaObject==app.m_moduleChoices.end()) return 952;
            app.m_selectedSuggestion=static_cast<int>(schemaObject-app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (!app.m_sqlSchemaSelected || !app.m_sqlColumnsFuture.valid()) return 953;
            app.m_selectedSuggestion=0; app.applyModuleCommand();
            while (app.m_sqlColumnsFuture.valid() && std::chrono::steady_clock::now()<deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_sqlColumnsFuture.valid() || app.m_sqlSchemaSelected || !app.m_sqlSchemaShowing) return 954;
            app.m_selectedSuggestion=0; app.applyModuleCommand();
            if (app.m_sqlSchemaShowing || !app.m_sqlShowing) return 935;
            app.m_sqlResult.truncated = true; app.m_sqlResult.rows[0][0].value = "=2+2"; app.refreshSuggestions();
            auto exportChoice = std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-175); if (exportChoice==app.m_moduleChoices.end() || app.m_suggestions[static_cast<std::size_t>(exportChoice-app.m_moduleChoices.begin())].path!="Export partial results to CSV") return 710;
            app.m_selectedSuggestion=static_cast<int>(exportChoice-app.m_moduleChoices.begin()); app.applyModuleCommand();
            while (app.m_sqlCsvFuture.valid() && std::chrono::steady_clock::now()<deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_sqlCsvFuture.valid() || app.m_searchBar.kind()!=ui::PromptKind::ModuleSqlExportPath || !app.m_sqlCsv.partial) return 711;
            app.m_searchBar.setQuery("results.csv"); app.applyModuleCommand();
            auto confirmCsv=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-176);
            if (confirmCsv==app.m_moduleChoices.end() || std::filesystem::exists(app.m_project.root()/"results.csv")) return 712;
            app.m_selectedSuggestion=static_cast<int>(confirmCsv-app.m_moduleChoices.begin()); app.applyModuleCommand();
            while (app.m_sqlCsvSaveFuture.valid() && std::chrono::steady_clock::now()<deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            std::ifstream csv(app.m_project.root()/"results.csv",std::ios::binary); std::string exported((std::istreambuf_iterator<char>(csv)),{});
            if (app.m_sqlCsvSaveFuture.valid() || app.m_sqlCsvStage!=5 || exported.find("'=2+2")==std::string::npos) return 713;
            std::filesystem::remove(app.m_project.root()/"results.csv");
            app.m_selectedSuggestion = 0; app.applyModuleCommand();
            app.m_selectedSuggestion = 1; app.applyModuleCommand(); app.m_searchBar.setQuery("sample.db"); app.applyModuleCommand();
            while (app.m_sqlFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (!app.m_sqlShowing || !app.m_sqlExplain || !app.m_sqlResult.error.empty() || app.m_sqlResult.rows.empty()) return 535;
            app.m_sqlShowing = false; app.m_document = editor::Document::fromUtf8("SELECT name FROM items WHERE name = :name");
            std::ofstream(app.m_project.root() / "bindings.json") << R"({":name":{"type":"text","value":"two"}})";
            app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 2; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleSqlPath || !app.m_sqlBound) return 536;
            app.m_searchBar.setQuery("sample.db"); app.applyModuleCommand();
            if (app.m_sqlDatabase != "sample.db" || app.m_sqlFuture.valid()) return 537;
            app.m_searchBar.setQuery("bindings.json"); app.applyModuleCommand();
            while (app.m_sqlFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (!app.m_sqlShowing || !app.m_sqlResult.error.empty() || app.m_sqlResult.rows.size() != 1 || app.m_sqlResult.rows[0][0].value != "two") return 538;
            app.m_sqlShowing=false; app.m_document=editor::Document::fromUtf8("UPDATE items SET name='changed' WHERE name='one'");
            app.m_modulePage=0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion=3; app.applyModuleCommand();
            if (app.m_searchBar.kind()!=ui::PromptKind::ModuleSqlPath || !app.m_sqlChangeMode) return 1010;
            app.m_searchBar.setQuery("sample.db"); app.applyModuleCommand();
            const auto changeDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
            while (app.m_sqlChangeFuture.valid() && std::chrono::steady_clock::now()<changeDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_sqlChangeFuture.valid() || !app.m_sqlShowing || !app.m_sqlChange.error.empty() || app.m_sqlChange.affected!=1) return 1011;
            bool rowWarning=false, observedRow=false;
            for (std::size_t page=0; page<10; ++page) { app.m_modulePage=page; app.refreshSuggestions(); for (const auto& item : app.m_suggestions) { rowWarning |= item.path=="Observed rows (incomplete)" && item.detail.find("sensitive")!=std::string_view::npos; observedRow |= item.path=="Update · items" && item.detail.find("rowid 1")!=std::string_view::npos; } }
            if (!rowWarning || !observedRow || app.m_sqlRowShowing || !app.m_sqlRowDiff.cells.empty()) return 1019;
            bool selectedRow=false; for (std::size_t page=0;page<10 && !selectedRow;++page) { app.m_modulePage=page; app.refreshSuggestions(); auto row=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-4000); if(row!=app.m_moduleChoices.end()) { app.m_selectedSuggestion=static_cast<int>(row-app.m_moduleChoices.begin()); app.applyModuleCommand(); selectedRow=true; } }
            if(!selectedRow || !app.m_sqlRowShowing) return 1031;
            while(app.m_sqlRowFuture.valid() && std::chrono::steady_clock::now()<changeDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if(app.m_sqlRowFuture.valid() || !app.m_sqlRowDiff.error.empty() || app.m_sqlRowDiff.cells.size()!=1 || app.m_sqlRowDiff.cells[0].before.value!="one" || app.m_sqlRowDiff.cells[0].after.value!="changed") return 1032;
            bool sensitive=false; for(std::size_t page=0;page<10;++page) { app.m_modulePage=page; app.refreshSuggestions(); sensitive |= std::any_of(app.m_suggestions.begin(),app.m_suggestions.end(),[](const auto& item) { return item.path=="Sensitive row values"; }); } if(!sensitive) return 1033;
            app.m_modulePage=0; app.refreshSuggestions(); app.m_selectedSuggestion=0; app.applyModuleCommand();
            if(app.m_sqlRowShowing || !app.m_sqlRowDiff.cells.empty() || !app.m_sqlShowing) return 1034;
            app.m_modulePage=0; app.refreshSuggestions(); app.m_selectedSuggestion=0; app.applyModuleCommand();
            if (app.m_sqlShowing || app.m_sqlChangeMode || !app.m_sqlChange.original.empty() || !app.m_sqlChange.changed.empty()) return 1016;
            app.m_modulePage=0; app.refreshSuggestions(); app.m_selectedSuggestion=3; app.applyModuleCommand(); app.m_searchBar.setQuery("sample.db"); app.applyModuleCommand();
            while (app.m_sqlChangeFuture.valid() && std::chrono::steady_clock::now()<changeDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_sqlChangeFuture.valid() || !app.m_sqlShowing || !app.m_sqlChange.error.empty() || app.m_sqlChange.affected!=1) return 1017;
            auto choose=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-301); if (choose==app.m_moduleChoices.end()) return 1012;
            app.m_selectedSuggestion=static_cast<int>(choose-app.m_moduleChoices.begin()); app.applyModuleCommand(); app.m_searchBar.setQuery("changed.db"); app.applyModuleCommand();
            auto confirm=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-302); if (confirm==app.m_moduleChoices.end() || std::filesystem::exists(app.m_project.root()/"changed.db")) return 1013;
            app.m_selectedSuggestion=static_cast<int>(confirm-app.m_moduleChoices.begin()); app.applyModuleCommand();
            while (app.m_sqlChangeSaveFuture.valid() && std::chrono::steady_clock::now()<changeDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_sqlChangeSaveFuture.valid() || app.m_sqlChangeStage!=5 || !std::filesystem::exists(app.m_project.root()/"changed.db")) return 1014;
            std::filesystem::remove(app.m_project.root()/"changed.db");
            std::promise<std::string> pendingCopy; app.m_sqlChangeSaveFuture=pendingCopy.get_future(); app.m_sqlChangeCancel=std::make_shared<std::atomic_bool>(false); app.m_sqlChangeStage=4;
            app.m_modulePage=0; app.refreshSuggestions(); app.m_selectedSuggestion=0; app.applyModuleCommand();
            if (!app.m_sqlShowing || !app.m_sqlChangeCancel->load() || app.m_sqlChangeStage!=4) return 1018;
            pendingCopy.set_value("cancelled"); app.pollModules();
            app.m_selectedSuggestion=0; app.applyModuleCommand();
            if (app.m_sqlShowing || app.m_sqlChangeMode || !app.m_sqlChange.original.empty() || !app.m_sqlChange.changed.empty()) return 1015;
            app.m_document=editor::Document::fromUtf8("UPDATE items SET name=:value WHERE name=@original");
            { std::ofstream f(app.m_project.root()/"change-bindings.json"); f << R"({":value":{"type":"text","value":"private-bound"},"@original":{"type":"text","value":"one"}})"; }
            app.m_modulePage=0; app.refreshSuggestions(); app.m_selectedSuggestion=4; app.applyModuleCommand();
            if (app.m_searchBar.kind()!=ui::PromptKind::ModuleSqlPath || !app.m_sqlBound || !app.m_sqlChangeMode) return 1024;
            app.m_searchBar.setQuery("sample.db"); app.applyModuleCommand(); app.m_searchBar.setQuery("change-bindings.json"); app.applyModuleCommand();
            while (app.m_sqlChangeFuture.valid() && std::chrono::steady_clock::now()<changeDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_sqlChangeFuture.valid() || !app.m_sqlShowing || !app.m_sqlChange.error.empty() || app.m_sqlChange.affected!=1) return 1025;
            bool hiddenLabel=false; for (std::size_t page=0;page<10;++page) { app.m_modulePage=page; app.refreshSuggestions(); for (const auto& item:app.m_suggestions) { hiddenLabel |= item.path=="Bound values hidden"; if(item.path.find("private-bound")!=std::string_view::npos || item.detail.find("private-bound")!=std::string_view::npos) return 1026; } }
            if (!hiddenLabel) return 1027;
            app.m_modulePage=0; app.refreshSuggestions(); auto pickBound=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-301); if(pickBound==app.m_moduleChoices.end()) return 1028;
            app.m_selectedSuggestion=static_cast<int>(pickBound-app.m_moduleChoices.begin()); app.applyModuleCommand(); app.m_searchBar.setQuery("bound-copy.db"); app.applyModuleCommand();
            auto confirmBound=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-302); if(confirmBound==app.m_moduleChoices.end()) return 1029;
            { std::ofstream f(app.m_project.root()/"change-bindings.json",std::ios::trunc); f << "changed"; }
            app.m_selectedSuggestion=static_cast<int>(confirmBound-app.m_moduleChoices.begin()); app.applyModuleCommand();
            while (app.m_sqlChangeSaveFuture.valid() && std::chrono::steady_clock::now()<changeDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if(app.m_sqlChangeSaveFuture.valid() || app.m_sqlChangeStage!=0 || std::filesystem::exists(app.m_project.root()/"bound-copy.db") || !app.m_commandStatusIsError) return 1030;
            app.m_modulePage=0; app.refreshSuggestions(); app.m_selectedSuggestion=0; app.applyModuleCommand(); std::filesystem::remove(app.m_project.root()/"change-bindings.json");
            app.m_document=editor::Document::fromUtf8("SELECT name FROM items WHERE name = :name");
            // A module may expose only the bound command; result validation must
            // check that command rather than requiring the unbound variant.
            const auto boundOnly = temp / "sql-bound-only";
            const std::string boundManifest = R"({"name":"Bound SQL","version":"1","requires":["sql-parameters-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Bound","action":"sql-query-bound"}]})";
            if (!modules::install(boundOnly, "bound", boundManifest, error) || !modules::setEnabled(boundOnly, "bound", true, error) || !modules::list(boundOnly, app.m_modules, error)) return 539;
            app.m_moduleRoot = boundOnly; app.m_moduleSelected = 0; app.m_sqlShowing = false;
            app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            app.m_searchBar.setQuery("sample.db"); app.applyModuleCommand(); app.m_searchBar.setQuery("bindings.json"); app.applyModuleCommand();
            const auto boundDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_sqlFuture.valid() && std::chrono::steady_clock::now() < boundDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (!app.m_sqlShowing || !app.m_sqlResult.error.empty() || app.m_sqlResult.rows.size() != 1 || app.m_sqlResult.rows[0][0].value != "two") return 540;
            app.m_sqlShowing = false; app.m_document = std::move(oldDocument); app.m_filePath = oldPath; app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected; app.m_project = std::move(oldProject); app.m_hasProject = oldHasProject; app.m_searchBar.close();
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected; auto oldProject = app.m_project; const auto oldHasProject = app.m_hasProject; auto oldDocument = app.m_document; const auto oldPath = app.m_filePath;
            bool created = false; std::string error; app.m_project = workspace::Project::openOrCreate(temp / "visual-project", created, error); app.m_hasProject = true;
            const unsigned char red[] = {0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x08,0x06,0x00,0x00,0x00,0x1f,0x15,0xc4,0x89,0x00,0x00,0x00,0x0d,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0xf8,0xcf,0xc0,0xf0,0x1f,0x00,0x05,0x00,0x01,0xff,0x89,0x99,0x3d,0x1d,0x00,0x00,0x00,0x00,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82};
            const unsigned char blue[] = {0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x08,0x06,0x00,0x00,0x00,0x1f,0x15,0xc4,0x89,0x00,0x00,0x00,0x0d,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0x60,0x60,0xf8,0xff,0x1f,0x00,0x03,0x02,0x01,0xff,0xe6,0x77,0x0b,0xae,0x00,0x00,0x00,0x00,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82};
            std::ofstream(app.m_project.root() / "baseline.png", std::ios::binary).write(reinterpret_cast<const char*>(red), sizeof red);
            std::ofstream(app.m_project.root() / "candidate.png", std::ios::binary).write(reinterpret_cast<const char*>(blue), sizeof blue);
            app.m_moduleRoot = temp / "visual-modules";
            const std::string manifest = R"({"name":"Visual","version":"1","requires":["visual-regression-review-v1","visual-regression-config-v1","visual-review-crop-v1","visual-config-edit-v1","visual-mask-draw-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Compare","action":"compare-visual-images"},{"title":"Compare with config","action":"compare-visual-configured"}]})";
            if (!modules::install(app.m_moduleRoot, "visual", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "visual", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 570;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleVisualPath) return 571;
            app.m_searchBar.setQuery("baseline.png"); app.applyModuleCommand(); app.m_searchBar.setQuery("candidate.png"); app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_visualFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (!app.m_visualReview || app.m_visualReview->comparison().changedPixels != 1 || app.m_visualPreview.baseline.rgba.empty()) return 572;
            app.refreshSuggestions(); app.m_selectedSuggestion = 2; app.applyModuleCommand();
            if (!app.m_visualVisible || !app.makeFrameState(0).visualPreview) return 573;
            app.m_visualVisible = false; app.refreshSuggestions();
            auto cropChoice=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-305); if(cropChoice==app.m_moduleChoices.end()) return 1035;
            app.m_selectedSuggestion=static_cast<int>(cropChoice-app.m_moduleChoices.begin()); app.applyModuleCommand();
            while(app.m_visualCropFuture.valid() && std::chrono::steady_clock::now()<deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if(app.m_visualCropFuture.valid() || !app.m_visualVisible || !app.m_visualCropMode || app.m_visualCropPreview.baseline.width!=1) return 1036;
            input::KeyEvent overview{}; overview.action=input::KeyAction::Press; overview.key=input::Key::Zero; app.handleKey(overview);
            if(!app.m_visualVisible || app.m_visualCropMode || app.makeFrameState(0).visualPreview!=&app.m_visualPreview) return 1037;
            app.m_visualVisible=false; app.refreshSuggestions(); auto approve=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-92); if(approve==app.m_moduleChoices.end()) return 1038; app.m_selectedSuggestion=static_cast<int>(approve-app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (!app.m_visualConfirm) return 574;
            app.refreshSuggestions(); auto confirm=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-94); if(confirm==app.m_moduleChoices.end()) return 1039; app.m_selectedSuggestion=static_cast<int>(confirm-app.m_moduleChoices.begin()); app.applyModuleCommand();
            while (app.m_visualApprovalFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            std::string saved; if (!app.m_project.readFile("baseline.png", saved, error) || saved != std::string(reinterpret_cast<const char*>(blue), sizeof blue)) return 575;
            std::ofstream(app.m_project.root() / "baseline.png", std::ios::binary | std::ios::trunc).write(reinterpret_cast<const char*>(red), sizeof red);
            std::ofstream(app.m_project.root() / "visual.json") << R"({"version":1,"threshold":0,"masks":[{"x":0,"y":0,"width":1,"height":1}]})";
            app.m_visualShowing = false; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 1; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleVisualPath || !app.m_visualConfigured) return 576;
            app.m_searchBar.setQuery("visual.json"); app.applyModuleCommand(); app.m_searchBar.setQuery("baseline.png"); app.applyModuleCommand(); app.m_searchBar.setQuery("candidate.png"); app.applyModuleCommand();
            const auto configuredDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_visualFuture.valid() && std::chrono::steady_clock::now() < configuredDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (!app.m_visualReview || app.m_visualReview->comparison().maskedPixels != 1 || app.m_visualReview->comparison().changedPixels != 0) return 577;
            const std::string originalConfig=R"({"version":1,"threshold":0,"masks":[{"x":0,"y":0,"width":1,"height":1}]})";
            app.m_document=editor::Document::fromUtf8(originalConfig); app.m_filePath="visual.json"; app.refreshSuggestions();
            auto editConfig=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-306); if(editConfig==app.m_moduleChoices.end()) return 1040;
            app.m_selectedSuggestion=static_cast<int>(editConfig-app.m_moduleChoices.begin()); app.applyModuleCommand();
            auto drawChoice=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-313);if(drawChoice==app.m_moduleChoices.end())return 1070;app.m_selectedSuggestion=static_cast<int>(drawChoice-app.m_moduleChoices.begin());app.applyModuleCommand();
            if(!app.m_visualMaskMode || !app.m_visualVisible)return 1071;
            const float originalZoom=app.m_uiZoom; app.m_uiZoom=1.5F;
            int uiWidth=0,uiHeight=0;app.uiSize(uiWidth,uiHeight);const auto pane=ui::visualReviewPaneRect(uiWidth,uiHeight,1,app.m_visualPreview.candidate.width,app.m_visualPreview.candidate.height);if(pane.width<=0 || pane.height<=0)return 1072;
            input::MouseButtonEvent maskClick{};maskClick.button=input::MouseButton::Left;maskClick.action=input::KeyAction::Press;maskClick.x=pane.x+pane.width/2;maskClick.y=pane.y+pane.height/2;app.handleMouseButton(maskClick);maskClick.action=input::KeyAction::Release;app.handleMouseButton(maskClick);app.m_uiZoom=originalZoom;
            if(!app.m_visualMaskSelected || app.m_visualMask.width!=1 || app.m_visualMask.height!=1 || !app.makeFrameState(0).visualMaskSelected)return 1073;
            input::KeyEvent maskEnter{};maskEnter.action=input::KeyAction::Press;maskEnter.key=input::Key::Enter;app.handleKey(maskEnter);while(app.m_visualConfigFuture.valid() && std::chrono::steady_clock::now()<configuredDeadline){app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(app.m_visualConfigFuture.valid() || app.m_visualConfigStage!=4 || app.m_visualConfigDraft.after.masks.size()!=2 || app.m_document.toUtf8()!=originalConfig)return 1074;
            bool cancelledMask=false;for(std::size_t page=0;page<10 && !cancelledMask;++page){app.m_modulePage=page;app.refreshSuggestions();auto cancel=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-311);if(cancel!=app.m_moduleChoices.end()){app.m_selectedSuggestion=static_cast<int>(cancel-app.m_moduleChoices.begin());app.applyModuleCommand();cancelledMask=true;}}if(!cancelledMask || app.m_document.toUtf8()!=originalConfig)return 1075;
            app.m_modulePage=0;app.refreshSuggestions();editConfig=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-306);if(editConfig==app.m_moduleChoices.end())return 1076;app.m_selectedSuggestion=static_cast<int>(editConfig-app.m_moduleChoices.begin());app.applyModuleCommand();
            auto thresholdChoice=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-309); if(thresholdChoice==app.m_moduleChoices.end()) return 1041;
            app.m_selectedSuggestion=static_cast<int>(thresholdChoice-app.m_moduleChoices.begin()); app.applyModuleCommand(); app.m_searchBar.setQuery("1"); app.applyModuleCommand();
            while(app.m_visualConfigFuture.valid() && std::chrono::steady_clock::now()<configuredDeadline) {app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(app.m_visualConfigFuture.valid() || app.m_visualConfigStage!=4 || app.m_visualConfigDraft.after.threshold!=1 || app.m_document.toUtf8()!=originalConfig) return 1042;
            bool appliedConfig=false; for(std::size_t page=0;page<10 && !appliedConfig;++page) {app.m_modulePage=page;app.refreshSuggestions();auto applyConfig=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-310);if(applyConfig!=app.m_moduleChoices.end()){app.m_selectedSuggestion=static_cast<int>(applyConfig-app.m_moduleChoices.begin());app.applyModuleCommand();appliedConfig=true;}} if(!appliedConfig) return 1043;
            while(app.m_visualConfigApplyFuture.valid() && std::chrono::steady_clock::now()<configuredDeadline) {app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(app.m_visualConfigApplyFuture.valid() || app.m_visualShowing || app.m_visualConfigReview || app.m_document.toUtf8()==originalConfig || app.m_document.toUtf8().find("\"threshold\":1")==std::string::npos) return 1044;
            app.m_document.undo(); if(app.m_document.toUtf8()!=originalConfig) return 1045;
            app.m_visualShowing = false; app.m_visualVisible = false; app.m_document = std::move(oldDocument); app.m_filePath = oldPath; app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected; app.m_project = std::move(oldProject); app.m_hasProject = oldHasProject; app.m_searchBar.close();
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected; auto oldDocument = app.m_document; const auto oldPath = app.m_filePath; auto oldProject = app.m_project; const auto oldHasProject = app.m_hasProject;
            bool created = false; std::string error; app.m_project = workspace::Project::openOrCreate(temp / "release-notes-project", created, error); app.m_hasProject = true;
            workspace::TaskBoard board; std::string id;
            if (!workspace::loadTasks(app.m_project, board, error) || !workspace::createTask(board, "Improve release testing", 1, "tester", id) || !workspace::saveTasks(app.m_project, board, error)) return 520;
            app.m_moduleRoot = temp / "release-notes-modules";
            const std::string manifest = R"({"name":"Release notes","version":"1","requires":["release-note-builder-v1"],"permissions":{"project":true,"network":false,"processes":true,"settings":false},"commands":[{"title":"Draft","action":"release-note-builder"}]})";
            if (!modules::install(app.m_moduleRoot, "release-notes", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "release-notes", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 521;
            app.m_document = editor::Document::fromUtf8(""); app.m_filePath = "CHANGELOG.md"; app.m_moduleSelected = 0; app.m_modulePage = 0;
            app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_releaseNotesFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (!app.m_releaseNotesShowing || app.m_releaseNotesEntries.size() != 1 || app.m_releaseNotesEntries[0].selected) return 522;
            const auto chooseReleaseRow = [&](int choice) { for (std::size_t page=0;page<8;++page) { app.m_modulePage=page; app.refreshSuggestions(); auto row=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),choice); if (row!=app.m_moduleChoices.end()) { app.m_selectedSuggestion=static_cast<int>(row-app.m_moduleChoices.begin()); app.applyModuleCommand(); return true; } } return false; };
            if (!chooseReleaseRow(0)) return 523;
            if (!app.m_releaseNotesEntries[0].selected) return 523;
            if (!chooseReleaseRow(-197) || app.m_releaseNotesFilter!=1 || !chooseReleaseRow(-195) || !app.m_releaseNotesPreview || app.m_releaseNotesDraft.markdown.find("Improve release testing")==std::string::npos || !app.m_document.toUtf8().empty()) return 790;
            const auto reviewedDraft=app.m_releaseNotesDraft.markdown;
            if (!chooseReleaseRow(-73)) return 791;
            if (app.m_searchBar.isOpen() || app.m_document.toUtf8().find("Improve release testing") == std::string::npos || app.m_document.toUtf8().find("# Release Draft") == std::string::npos) return 524;
            if (app.m_document.toUtf8()!=reviewedDraft) return 792;
            app.m_document = std::move(oldDocument); app.m_filePath = oldPath; app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected; app.m_project = std::move(oldProject); app.m_hasProject = oldHasProject; app.m_searchBar.close();
        }
        {
            const auto oldRoot=app.m_moduleRoot;auto oldModules=app.m_modules;const auto oldSelected=app.m_moduleSelected;auto oldProject=app.m_project;const auto oldHasProject=app.m_hasProject;
            bool created=false;std::string error;app.m_project=workspace::Project::openOrCreate(temp/"junit-project",created,error);app.m_hasProject=true;
            {std::ofstream out(app.m_project.root()/"current.xml");out << R"(<testsuite><testcase classname="A" name="x"><failure message="bad">details</failure></testcase><testcase classname="B" name="x"/></testsuite>)";}
            {std::ofstream out(app.m_project.root()/"previous.xml");out << R"(<testsuite><testcase classname="A" name="x"/><testcase classname="B" name="x"/></testsuite>)";}
            app.m_moduleRoot=temp/"junit-modules";const std::string manifest=R"({"name":"JUnit","version":"1","requires":["junit-import-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Import","action":"import-junit"}]})";
            if(!modules::install(app.m_moduleRoot,"junit",manifest,error)||!modules::setEnabled(app.m_moduleRoot,"junit",true,error)||!modules::list(app.m_moduleRoot,app.m_modules,error))return 1076;
            app.m_moduleSelected=0;app.m_modulePage=0;app.m_searchBar.open(ui::PromptKind::ModuleCommands);app.refreshSuggestions();app.m_selectedSuggestion=0;app.applyModuleCommand();if(app.m_searchBar.kind()!=ui::PromptKind::ModuleCTestInput || app.m_junitInputMode!=1)return 1077;
            app.m_searchBar.setQuery("current.xml");app.applyModuleCommand();const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);while(app.m_junitFuture.valid() && std::chrono::steady_clock::now()<deadline){app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(app.m_junitFuture.valid() || !app.m_junitShowing || app.m_junitCurrent.summary.failed!=1 || app.m_junitCurrent.summary.passed!=1 || app.m_junitSnapshot.empty())return 1078;
            auto row=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),0);if(row==app.m_moduleChoices.end())return 1079;app.m_selectedSuggestion=static_cast<int>(row-app.m_moduleChoices.begin());app.applyModuleCommand();if(!app.m_junitSelected || *app.m_junitSelected!=0)return 1080;
            app.m_selectedSuggestion=0;app.applyModuleCommand();auto compare=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-314);if(compare==app.m_moduleChoices.end())return 1081;app.m_selectedSuggestion=static_cast<int>(compare-app.m_moduleChoices.begin());app.applyModuleCommand();app.m_searchBar.setQuery("previous.xml");app.applyModuleCommand();
            while(app.m_junitFuture.valid() && std::chrono::steady_clock::now()<deadline){app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}if(app.m_junitFuture.valid() || !app.m_junitComparing || app.m_junitComparison.size()!=2 || app.m_junitComparison[0].status!=modules::TestHistoryStatus::Changed || app.m_junitComparison[1].status!=modules::TestHistoryStatus::Unchanged || app.m_junitPreviousSnapshot.empty())return 1082;
            app.m_junitShowing=false;app.m_junitCurrent={};app.m_junitPrevious={};app.m_junitSnapshot.clear();app.m_junitPreviousSnapshot.clear();app.m_searchBar.close();app.m_moduleRoot=oldRoot;app.m_modules=std::move(oldModules);app.m_moduleSelected=oldSelected;app.m_project=std::move(oldProject);app.m_hasProject=oldHasProject;
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected; auto oldDocument = app.m_document; const auto oldPath = app.m_filePath;
            app.m_moduleRoot = temp / "diagram-modules"; std::string error;
            const std::string manifest = R"({"name":"Diagram","version":"1","requires":["diagram-preview-v1","diagram-flow-cycles-v1","diagram-flow-chains-v1","diagram-flow-bidirectional-v1","diagram-flow-branches-v1","diagram-flow-rounded-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Preview","action":"diagram-preview"}]})";
            if (!modules::install(app.m_moduleRoot, "diagram", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "diagram", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 506;
            app.m_document = editor::Document::fromUtf8("flowchart LR\nA[Start] -->|approved| B[End]\n"); app.m_filePath = "diagram.mmd"; const auto original = app.m_document.toUtf8();
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_diagramPreviewFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            const auto frame = app.makeFrameState(1.0);
            if (app.m_diagramPreviewFuture.valid() || !app.m_diagramPreviewVisible || frame.diagramNodes.size() != 2 || frame.diagramEdges.size() != 1 || frame.diagramEdges[0].label != "approved" || app.m_document.toUtf8() != original) return 507;
            input::KeyEvent diagramKey{}; diagramKey.action=input::KeyAction::Press; app.handleChar(U'+');
            diagramKey.key=input::Key::Right; app.handleKey(diagramKey);
            if (app.m_diagramZoom<=1.0 || app.m_diagramPanX<=0.0 || app.makeFrameState(1.0).diagramZoom!=app.m_diagramZoom) return 810;
            input::ScrollEvent diagramScroll{}; diagramScroll.yOffset=1; const auto beforeZoom=app.m_diagramZoom; app.handleScroll(diagramScroll); if (app.m_diagramZoom<=beforeZoom) return 811;
            input::MouseButtonEvent drag{}; drag.button=input::MouseButton::Left; drag.action=input::KeyAction::Press; drag.x=100; drag.y=100; app.handleMouseButton(drag);
            input::MouseMoveEvent move{}; move.x=200; move.y=150; const auto panBefore=app.m_diagramPanX; app.handleMouseMove(move); drag.action=input::KeyAction::Release; app.handleMouseButton(drag);
            if (app.m_diagramPanX<=panBefore || app.m_diagramPanY<=0 || app.m_diagramDragging) return 812;
            for (int i=0;i<100;++i) app.handleChar(U'+'); if (app.m_diagramZoom!=8.0) return 813;
            app.handleChar(U'0'); if (app.m_diagramZoom!=1.0 || app.m_diagramPanX!=0 || app.m_diagramPanY!=0) return 814;
            input::KeyEvent escape{}; escape.key = input::Key::Escape; escape.action = input::KeyAction::Press; app.handleKey(escape);
            if (app.m_diagramPreviewVisible) return 508;
            app.m_document=editor::Document::fromUtf8("flowchart LR\nA(Round) & B([Capsule]) <--> |first| C -->|second| A --> A\n");
            app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion=0; app.applyModuleCommand();
            const auto cycleDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(15); while(app.m_diagramPreviewFuture.valid() && std::chrono::steady_clock::now()<cycleDeadline) {app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            const auto cycleFrame=app.makeFrameState(1.0); if(app.m_diagramPreviewFuture.valid() || !app.m_diagramPreviewVisible || cycleFrame.diagramNodes.size()!=3 || cycleFrame.diagramNodes[0].shape!=modules::DiagramNodeShape::Rounded || cycleFrame.diagramNodes[1].shape!=modules::DiagramNodeShape::Stadium || cycleFrame.diagramEdges.size()!=4 || cycleFrame.diagramEdges[3].points.size()!=6 || !cycleFrame.diagramEdges[0].bidirectional || !cycleFrame.diagramEdges[1].bidirectional || cycleFrame.diagramEdges[2].bidirectional || cycleFrame.diagramEdges[0].label!="first" || cycleFrame.diagramEdges[1].label!="first" || cycleFrame.diagramEdges[2].label!="second") return 1054;
            app.handleKey(escape); if(app.m_diagramPreviewVisible) return 1055; app.m_document=editor::Document::fromUtf8(original);
            app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            app.m_searchBar.close(); app.m_searchBar.open(ui::PromptKind::ModuleCommands);
            while (app.m_diagramPreviewFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_diagramPreviewFuture.valid() || app.m_diagramPreviewVisible) return 509;
            app.m_document = std::move(oldDocument); app.m_filePath = oldPath; app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected; app.m_searchBar.close();
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected; auto oldDocument = app.m_document; const auto oldPath = app.m_filePath;
            app.m_moduleRoot = temp / "accessibility-modules"; std::string error;
            const std::string manifest = R"({"name":"Accessibility","version":"1","requires":["accessibility-audit-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Audit","action":"accessibility-audit"}]})";
            if (!modules::install(app.m_moduleRoot, "accessibility", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "accessibility", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 510;
            app.m_document = editor::Document::fromUtf8("<html lang='en'>\nπ <img src='x'>\n</html>"); app.m_filePath = "index.html";
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_accessibilityFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (!app.m_accessibilityShowing || app.m_accessibilityAudit.findings.size() != 1 || app.m_commandStatusIsError) return 511;
            app.m_selectedSuggestion = 2; app.applyModuleCommand();
            if (app.m_document.cursorLine() != 1 || app.m_document.cursorColumn() != 2 || app.m_searchBar.isOpen()) return 512;
            app.m_document = editor::Document::fromUtf8("<html lang='en'>\n<div id='name'></div>\nπ <span id='name'></span>\n</html>");
            app.m_accessibilityShowing = false; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            while (app.m_accessibilityFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_accessibilityFuture.valid() || !app.m_accessibilityShowing || app.m_accessibilityAudit.findings.size() != 1 || app.m_accessibilityAudit.findings[0].rule != "duplicate-id") return 513;
            app.m_selectedSuggestion = 2; app.applyModuleCommand();
            if (app.m_document.cursorLine() != 2 || app.m_document.cursorColumn() != 2 || app.m_searchBar.isOpen()) return 514;
            app.m_document = std::move(oldDocument); app.m_filePath = oldPath; app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected; app.m_searchBar.close();
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            { std::ofstream out(temp / "nested" / "sample.rs"); out << "  let x = thing;\n  use_it(x);\n"; }
            { std::ofstream out(temp / "outside.rs"); out << "outside\n"; }
            { std::ofstream out(temp / "nested" / "rust.json"); out << R"({"code":{"code":"E0382"},"level":"error","message":"borrow of moved value","spans":[{"file_name":"sample.rs","line_start":1,"column_start":3,"line_end":1,"column_end":4,"is_primary":true,"text":[{"text":"  let x = thing;","highlight_start":3,"highlight_end":4}]},{"file_name":"../outside.rs","line_start":1,"column_start":1,"line_end":1,"column_end":2,"is_primary":false}],"children":[{"level":"note","message":"value used here","spans":[{"file_name":"sample.rs","line_start":1,"column_start":3,"line_end":2,"column_end":4,"is_primary":false,"text":[{"text":"  let x = thing;","highlight_start":3,"highlight_end":17},{"text":"  use_it(x);","highlight_start":1,"highlight_end":4}]}],"children":[]}]})"; }
            app.m_moduleRoot = temp / "rust-modules"; std::string error;
            const std::string manifest = R"({"name":"Rust","version":"1","requires":["rust-ownership-lens-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Review","action":"review-rust-ownership"}]})";
            if (!modules::install(app.m_moduleRoot, "rust", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "rust", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 493;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleRustPath) return 494;
            app.m_searchBar.setQuery("rust.json"); app.applyModuleCommand(); const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_rustOwnershipFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_rustOwnershipFuture.valid() || app.m_commandStatusIsError || app.m_rustOwnershipDiagnostics.size() != 1) return 495;
            app.m_selectedSuggestion = 1; app.applyModuleCommand();
            const auto chooseSpan = [&](int span) { for (std::size_t page = 0; page < 8; ++page) { app.m_modulePage = page; app.refreshSuggestions(); const auto row = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), span); if (row != app.m_moduleChoices.end()) { app.m_selectedSuggestion = static_cast<int>(row - app.m_moduleChoices.begin()); app.applyModuleCommand(); return true; } } return false; };
            if (!chooseSpan(1) || !app.m_commandStatusIsError || app.m_filePath != "example.txt") return 496;
            { std::ofstream out(temp / "nested" / "sample.rs",std::ios::trunc); out << "  let x = thing;\n  changed(x);\n"; }
            if (!chooseSpan(2) || !app.m_commandStatusIsError || app.m_filePath!="example.txt") return 770;
            { std::ofstream out(temp / "nested" / "sample.rs",std::ios::trunc); out << "  let x = thing;\n  use_it(x);\n"; }
            if (!chooseSpan(2) || app.m_filePath!="sample.rs" || app.m_document.cursorLine()!=0 || app.m_document.cursorColumn()!=2) return 771;
            app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.m_modulePage=0; app.refreshSuggestions();
            if (!chooseSpan(0) || app.m_filePath != "sample.rs" || app.m_document.cursorLine() != 0 || app.m_document.cursorColumn() != 2) return 497;
            { std::ofstream out(temp / "nested" / "sample.rs", std::ios::trunc); out << "changed source\n"; }
            app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.m_modulePage = 0; app.refreshSuggestions();
            if (!chooseSpan(0) || !app.m_commandStatusIsError || app.m_commandStatus.find("fresh diagnostics") == std::string::npos) return 498;
            app.m_rustOwnershipShowing = false; app.m_rustOwnershipSelected.reset(); app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected;
            app.openFile("example.txt", false); std::filesystem::remove(temp / "nested" / "sample.rs"); std::filesystem::remove(temp / "nested" / "rust.json"); std::filesystem::remove(temp / "outside.rs");
        }
#if !defined(_WIN32)
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            { std::ofstream out(temp / "nested" / "Cargo.toml"); out << "[package]\nname=\"fixture\"\nversion=\"0.1.0\"\n"; }
            app.m_moduleRoot = temp / "rust-run-modules"; std::string error;
            const std::string manifest = R"({"name":"Rust Run","version":"1","requires":["rust-diagnostic-run-v1"],"permissions":{"project":true,"network":false,"processes":true,"settings":false},"commands":[{"title":"Run","action":"run-rust-ownership"}]})";
            if (!modules::install(app.m_moduleRoot, "rust-run", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "rust-run", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 1100;
            app.m_moduleSelected=0; app.m_modulePage=0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion=0; app.applyModuleCommand();
            if (app.m_rustRunStage!=1 || app.m_searchBar.kind()!=ui::PromptKind::ModuleRustPath || app.m_rustRunFuture.valid()) return 1101;
            app.m_searchBar.setQuery("/usr/bin/true"); app.applyModuleCommand();
            if (app.m_rustRunStage!=2 || app.m_rustRunFuture.valid() || app.m_rustRunPlan.arguments.size()!=6 || app.m_rustRunPlan.arguments[1]!="--locked" || app.m_rustRunPlan.arguments[2]!="--offline") return 1102;
            bool confirm=false; for (std::size_t page=0; page<12; ++page) { app.m_modulePage=page; app.refreshSuggestions(); if (std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-403)!=app.m_moduleChoices.end()) {confirm=true;break;} }
            if (!confirm || app.m_rustRunFuture.valid()) return 1103;
            app.m_rustOwnershipShowing=false; app.m_rustRunStage=0; app.m_searchBar.close(); app.m_moduleRoot=oldRoot; app.m_modules=std::move(oldModules); app.m_moduleSelected=oldSelected; std::filesystem::remove(temp / "nested" / "Cargo.toml");
        }
#endif
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            std::string dump(32 + 24 + 52 + 168, '\0');
            const auto put = [&](std::size_t p, std::uint64_t value, unsigned n) { for (unsigned i = 0; i < n; ++i) dump[p + i] = static_cast<char>(value >> (8 * i)); };
            dump.replace(0, 4, "MDMP"); put(4, 0xa793, 4); put(8, 2, 4); put(12, 32, 4);
            put(32, 3, 4); put(36, 52, 4); put(40, 56, 4); put(44, 6, 4); put(48, 168, 4); put(52, 108, 4);
            put(56, 1, 4); put(60, 42, 4); put(84, 0x1234, 8); put(108, 42, 4); put(116, 0xc0000005, 4); put(132, 0xdeadbeef, 8);
            { std::ofstream out(temp / "nested" / "crash.dmp", std::ios::binary); out.write(dump.data(), static_cast<std::streamsize>(dump.size())); }
            app.m_moduleRoot = temp / "crash-modules"; std::string error;
            const std::string manifest = R"({"name":"Crash","version":"1","requires":["crash-dump-reader-v1","crash-gdb-text-v1","crash-lldb-text-v1","crash-apple-text-v1","crash-apple-ips-v1","crash-ips-source-map-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Inspect","action":"crash-dump-reader"}]})";
            if (!modules::install(app.m_moduleRoot, "crash", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "crash", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 490;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleCrashPath) return 491;
            app.m_searchBar.setQuery("crash.dmp"); app.applyModuleCommand(); const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_crashFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_crashFuture.valid() || app.m_commandStatusIsError || !app.m_crashDump.exception.present || app.m_crashDump.exception.threadId != 42 || app.m_crashDump.threads.size() != 1) return 492;
            app.m_crashDump.threads[0].framesIncomplete = true;
            app.m_crashDump.threads[0].frames = {{0x1234, 0x5678, modules::CrashFrameSource::Context}, {0x9876, 0x5432, modules::CrashFrameSource::FramePointerHeuristic}};
            bool contextSeen = false, heuristicSeen = false, incompleteSeen = false;
            for (std::size_t page = 0; page < 4; ++page) {
                app.m_modulePage = page; app.refreshSuggestions();
                for (const auto& item : app.m_suggestions) {
                    contextSeen |= item.path == "Context frame" && item.detail.find("0x1234") != std::string_view::npos;
                    heuristicSeen |= item.path == "Heuristic frame candidate" && item.detail.find("0x9876") != std::string_view::npos;
                    incompleteSeen |= item.path == "Incomplete frame information";
                }
            }
            if (!contextSeen || !heuristicSeen || !incompleteSeen) return 493;
            { std::ofstream out(temp / "nested" / "crash-map.tsv"); out << "app.exe\t200\t300\tcrash-source.cpp\t2\tmain\n"; }
            { std::ofstream out(temp / "nested" / "crash-source.cpp"); out << "first\nsecond\n"; }
            app.m_crashDump.modules = {{"app.exe", 0x1000, 0x1000}}; app.m_modulePage = 0; app.refreshSuggestions();
            auto importMap = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -220); if (importMap == app.m_moduleChoices.end()) return 830;
            app.m_selectedSuggestion = static_cast<int>(importMap - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleCrashPath || !app.m_crashMapInput) return 831;
            app.m_searchBar.setQuery("crash-map.tsv"); app.applyModuleCommand(); if (app.m_crashMapInput || app.m_crashSourceMap.ranges.size()!=1) return 832;
            { std::ofstream out(temp / "nested" / "crash.sym"); out << "MODULE windows x86_64 ABC123 app.exe\nFILE 0 crash-source.cpp\nFUNC 200 100 0 main\n200 100 2 0\n"; }
            app.m_modulePage=0; app.refreshSuggestions(); importMap=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-220); if (importMap==app.m_moduleChoices.end()) return 836;
            app.m_selectedSuggestion=static_cast<int>(importMap-app.m_moduleChoices.begin()); app.applyModuleCommand(); app.m_searchBar.setQuery("crash.sym");
            app.m_crashDump.architecture="x86-64"; app.m_crashDump.modules[0].debugId="WRONG"; app.applyModuleCommand(); if (!app.m_crashMapInput || !app.m_commandStatusIsError) return 838;
            app.m_crashDump.modules[0].debugId="ABC123"; app.m_crashDump.modules.push_back(app.m_crashDump.modules[0]); app.applyModuleCommand(); if (!app.m_crashMapInput || !app.m_commandStatusIsError) return 839;
            app.m_crashDump.modules.pop_back(); app.m_crashDump.modules[0].name="C:\\build\\app.exe"; app.applyModuleCommand();
            if (app.m_crashMapInput || app.m_crashSourceMap.ranges.size()!=1 || app.m_crashSourceMap.ranges[0].symbol!="main" || app.m_crashSourceMap.ranges[0].module!=app.m_crashDump.modules[0].name || app.m_crashMapIdentity.find("metadata matches")==std::string::npos) return 837;
            // Establish a saved baseline so undo returns the document to a clean state.
            app.openFile("example.txt", false); if (!app.m_hasDocument || app.m_document.dirty()) return 840;
            app.m_searchBar.open(ui::PromptKind::ModuleCommands);
            bool foundFrame=false; for (std::size_t page=0; page<5 && !foundFrame; ++page) { app.m_modulePage=page; app.refreshSuggestions(); auto row=std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 0); if (row!=app.m_moduleChoices.end()) { app.m_selectedSuggestion=static_cast<int>(row-app.m_moduleChoices.begin()); foundFrame=true; } }
            if (!foundFrame) return 833;
            app.m_document.insert(U'X'); app.applyModuleCommand(); if (!app.m_commandStatusIsError || app.m_filePath=="crash-source.cpp") return 835; app.m_document.undo();
            foundFrame=false; for (std::size_t page=0; page<5 && !foundFrame; ++page) { app.m_modulePage=page; app.refreshSuggestions(); auto row=std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 0); if (row!=app.m_moduleChoices.end()) { app.m_selectedSuggestion=static_cast<int>(row-app.m_moduleChoices.begin()); foundFrame=true; } } if (!foundFrame) return 841;
            app.applyModuleCommand(); if (app.m_filePath!="crash-source.cpp" || app.m_document.cursorLine()!=1 || app.m_searchBar.isOpen()) return 834;
            app.m_crashShowing=false; app.m_modulePage=0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion=0; app.applyModuleCommand();
            { std::ofstream out(temp / "nested" / "trace.txt"); out << "Thread 1 (Thread 0x1 (LWP 42)):\n#0  0x1234 in main () at crash-source.cpp:2\nunknown record\n"; }
            app.m_searchBar.setQuery("trace.txt"); app.applyModuleCommand(); const auto gdbDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
            while(app.m_crashFuture.valid() && std::chrono::steady_clock::now()<gdbDeadline){app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(app.m_crashFuture.valid() || !app.m_crashIsGdb || !app.m_crashGdb.incomplete || app.m_crashGdb.threads.size()!=1) return 1104;
            bool sawGdb=false,sawGdbFrame=false,sawGdbIncomplete=false,sawMap=false;for(std::size_t page=0;page<5;++page){app.m_modulePage=page;app.refreshSuggestions();for(const auto& row:app.m_suggestions){sawGdb|=row.path=="GDB backtrace text";sawGdbFrame|=row.path=="Frame #0" && row.detail.find("unverified")!=std::string_view::npos;sawGdbIncomplete|=row.path=="Incomplete backtrace";}sawMap|=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-220)!=app.m_moduleChoices.end();}
            if(!sawGdb || !sawGdbFrame || !sawGdbIncomplete || sawMap) return 1105;
            app.m_crashShowing=false; app.m_modulePage=0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion=0; app.applyModuleCommand();
            { std::ofstream out(temp / "nested" / "lldb.txt"); out << "(lldb) thread backtrace all\n* thread #1, name = 'main'\n  * frame #0: 0x1234 app`main + 4\n"; }
            app.m_searchBar.setQuery("lldb.txt"); app.applyModuleCommand(); const auto lldbDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
            while(app.m_crashFuture.valid() && std::chrono::steady_clock::now()<lldbDeadline){app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(app.m_crashFuture.valid() || !app.m_crashIsLldb || app.m_crashIsGdb || app.m_crashGdb.incomplete || app.m_crashGdb.threads.size()!=1) return 1301;
            bool sawLldb=false,sawLldbFrame=false;for(std::size_t page=0;page<5;++page){app.m_modulePage=page;app.refreshSuggestions();for(const auto& row:app.m_suggestions){sawLldb|=row.path=="LLDB backtrace text";sawLldbFrame|=row.path=="Frame #0" && row.detail.find("unverified")!=std::string_view::npos;}} if(!sawLldb || !sawLldbFrame) return 1302;
            app.m_crashShowing=false; app.m_modulePage=0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion=0; app.applyModuleCommand();
            { std::ofstream out(temp / "nested" / "apple.crash"); out << "Code Type: ARM-64 (Native)\nException Type: EXC_BAD_ACCESS\nThread 0 Crashed:: Dispatch queue: main\n0 MyApp 0x1234 main + 2\nBinary Images:\n"; }
            app.m_searchBar.setQuery("apple.crash"); app.applyModuleCommand(); const auto appleDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
            while(app.m_crashFuture.valid() && std::chrono::steady_clock::now()<appleDeadline){app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(app.m_crashFuture.valid() || !app.m_crashIsApple || !app.m_crashApple.incomplete || app.m_crashApple.threads.size()!=1) return 1106;
            bool sawApple=false,sawAppleFrame=false,sawAppleIncomplete=false,sawAppleMap=false;for(std::size_t page=0;page<5;++page){app.m_modulePage=page;app.refreshSuggestions();for(const auto& row:app.m_suggestions){sawApple|=row.path=="Apple crash report text";sawAppleFrame|=row.path=="Frame #0" && row.detail.find("unverified")!=std::string_view::npos;sawAppleIncomplete|=row.path=="Incomplete crash report";}sawAppleMap|=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-220)!=app.m_moduleChoices.end();}
            if(!sawApple || !sawAppleFrame || !sawAppleIncomplete || sawAppleMap) return 1107;
            app.m_crashShowing=false; app.m_modulePage=0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion=0; app.applyModuleCommand();
            { std::ofstream out(temp / "nested" / "report.ips"); out << R"({"bug_type":"309"})" << "\n" << R"({"cpuType":"ARM-64","usedImages":[{"name":"App","base":4096,"size":256,"path":"/secret"}],"threads":[{"triggered":true,"frames":[{"imageIndex":0,"imageOffset":12,"symbol":"main"},{"imageIndex":9,"imageOffset":0}]}]})"; }
            app.m_searchBar.setQuery("report.ips"); app.applyModuleCommand(); const auto ipsDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);while(app.m_crashFuture.valid() && std::chrono::steady_clock::now()<ipsDeadline){app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(app.m_crashFuture.valid() || !app.m_crashIsIps || !app.m_crashIps.incomplete || app.m_crashIps.threads.size()!=1) return 1108;
            bool sawIps=false,sawIpsFrame=false,sawIpsIncomplete=false,sawIpsMap=false,sawSecret=false;for(std::size_t page=0;page<5;++page){app.m_modulePage=page;app.refreshSuggestions();for(const auto& row:app.m_suggestions){sawIps|=row.path=="Apple JSON crash report" && row.detail.find("sensitive")!=std::string_view::npos;sawIpsFrame|=row.path=="Reported frame" && row.detail.find("unverified")!=std::string_view::npos;sawIpsIncomplete|=row.path=="Incomplete crash report";sawSecret|=row.detail.find("/secret")!=std::string_view::npos;}sawIpsMap|=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-220)!=app.m_moduleChoices.end();}
            if(!sawIps || !sawIpsFrame || !sawIpsIncomplete || !sawIpsMap || sawSecret) return 1109;
            { std::ofstream out(temp / "nested" / "ips-map.tsv"); out << "App\t0\t100\tcrash-source.cpp\t2\tmain\n"; }
            bool foundIpsMap=false;for(std::size_t page=0;page<5 && !foundIpsMap;++page){app.m_modulePage=page;app.refreshSuggestions();auto row=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-220);if(row!=app.m_moduleChoices.end()){app.m_selectedSuggestion=static_cast<int>(row-app.m_moduleChoices.begin());foundIpsMap=true;}}if(!foundIpsMap)return 1110;
            app.applyModuleCommand();app.m_searchBar.setQuery("ips-map.tsv");app.applyModuleCommand();if(app.m_crashMapInput || app.m_crashSourceMap.ranges.size()!=1)return 1111;
            app.m_modulePage=0;app.refreshSuggestions();if(std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),0)!=app.m_moduleChoices.end())return 1112;
            app.m_crashIps.incomplete=false; app.openFile("example.txt",false);app.m_searchBar.open(ui::PromptKind::ModuleCommands);
            const auto chooseIpsFrame=[&](){for(std::size_t page=0;page<5;++page){app.m_modulePage=page;app.refreshSuggestions();auto row=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),0);if(row!=app.m_moduleChoices.end()){app.m_selectedSuggestion=static_cast<int>(row-app.m_moduleChoices.begin());return true;}}return false;};
            if(!chooseIpsFrame())return 1113;app.m_document.insert(U'X');app.applyModuleCommand();if(!app.m_commandStatusIsError || app.m_filePath=="crash-source.cpp")return 1114;app.m_document.undo();if(!chooseIpsFrame())return 1115;app.applyModuleCommand();if(app.m_filePath!="crash-source.cpp" || app.m_document.cursorLine()!=1 || app.m_searchBar.isOpen())return 1116;
            std::filesystem::remove(temp / "nested" / "ips-map.tsv");
            app.m_crashIsIps=false; std::filesystem::remove(temp / "nested" / "report.ips");
            app.m_crashIsApple=false; std::filesystem::remove(temp / "nested" / "apple.crash");
            app.m_crashIsGdb=false; std::filesystem::remove(temp / "nested" / "trace.txt");
            app.openFile("example.txt", false); std::filesystem::remove(temp / "nested" / "crash-map.tsv"); std::filesystem::remove(temp / "nested" / "crash.sym"); std::filesystem::remove(temp / "nested" / "crash-source.cpp");
            app.m_crashShowing = false; app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected;
            std::filesystem::remove(temp / "nested" / "crash.dmp");
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            { std::ofstream out(temp / "nested" / "firmware.map"); out << "Memory Configuration\nName Origin Length Attributes\nFLASH 0x1000 0x100\nRAM 0x2000 0x100\nLinker script and memory map\n.text 0x1000 0x20\n.data 0x2000 0x10 load address 0x1020\n"; }
            app.m_moduleRoot = temp / "linker-modules"; std::string error;
            const std::string manifest = R"({"name":"Embedded","version":"1","requires":["linker-map-v1","toolchain-profiles-v1","svd-registers-v1","toolchain-target-edit-v1","toolchain-configure-v1"],"permissions":{"project":true,"network":false,"processes":true,"settings":false},"commands":[{"title":"Map","action":"review-linker-map"},{"title":"Profiles","action":"inspect-toolchain-profiles"},{"title":"Registers","action":"inspect-svd-registers"}]})";
            if (!modules::install(app.m_moduleRoot, "embedded", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "embedded", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 487;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleLinkerPath) return 488;
            app.m_searchBar.setQuery("firmware.map"); app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_linkerFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_linkerFuture.valid() || app.m_linkerMap.regions.size() != 2 || app.m_linkerMap.regions[0].used != 0x20 || app.m_linkerMap.regions[0].loadUsed != 0x10 || !app.m_linkerMap.loadIncomplete) return 489;
            app.m_linkerShowing=false; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.m_modulePage=0; app.refreshSuggestions(); app.m_selectedSuggestion=1; app.applyModuleCommand();
            if (app.m_searchBar.kind()!=ui::PromptKind::ModuleToolchainPath) return 780;
#if defined(_WIN32)
            { std::ofstream out(temp/"nested"/"toolchains.json"); out << R"({"profiles":[{"name":"Board","target":"arm-none-eabi","cCompiler":"C:\\not-installed\\compiler.exe"}]})"; }
#else
            { std::ofstream out(temp/"nested"/"toolchains.json"); out << R"({"profiles":[{"name":"Board","target":"arm-none-eabi","cCompiler":"/not-installed/compiler"}]})"; }
#endif
            app.m_searchBar.setQuery("toolchains.json"); app.applyModuleCommand();
            while (app.m_toolchainFuture.valid() && std::chrono::steady_clock::now()<deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_toolchainFuture.valid() || !app.m_toolchainShowing || app.m_commandStatusIsError || app.m_toolchainProfiles.size()!=1) return 781;
            auto selectToolchain=[&](int choice) {for(std::size_t page=0;page<8;++page){app.m_modulePage=page;app.refreshSuggestions();auto row=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),choice);if(row!=app.m_moduleChoices.end()){app.m_selectedSuggestion=static_cast<int>(row-app.m_moduleChoices.begin());app.applyModuleCommand();return true;}}return false;};
            if(!selectToolchain(0) || !app.m_toolchainSelected || std::none_of(app.m_suggestions.begin(),app.m_suggestions.end(),[](const auto& item) { return item.path=="C compiler" && item.detail.find("unverified")!=std::string_view::npos; })) return 782;
#if !defined(_WIN32)
            {std::ofstream out(temp/"nested"/"CMakeLists.txt");out << "project(Test)\n";} std::filesystem::create_directory(temp/"nested"/"target-build");
            const auto fakeCmake=temp/"fake-target-cmake"; {std::ofstream out(fakeCmake);out << "#!/bin/sh\nprintf ran > \"$0.ran\"\nexit 0\n";} std::filesystem::permissions(fakeCmake,std::filesystem::perms::owner_all);
            if(!selectToolchain(-255) || app.m_targetConfigureStage!=1) return 877;
            app.m_searchBar.setQuery("target-build");app.applyModuleCommand();if(app.m_targetConfigureStage!=2) return 878;
            app.m_searchBar.setQuery(fakeCmake.string());app.applyModuleCommand();if(app.m_targetConfigureStage!=3 || std::filesystem::exists(fakeCmake.string()+".ran")) return 879;
            if(!selectToolchain(-256) || !app.m_targetConfigureFuture.valid()) return 880;
            const auto configureDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(app.m_targetConfigureFuture.valid() && std::chrono::steady_clock::now()<configureDeadline){app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(app.m_targetConfigureFuture.valid() || app.m_targetConfigureStage!=5 || !std::filesystem::exists(fakeCmake.string()+".ran")) return 881;
            if(!selectToolchain(-193) || app.m_targetConfigureStage!=0) return 882;
            std::filesystem::remove(fakeCmake.string()+".ran");std::filesystem::remove(fakeCmake);std::filesystem::remove(temp/"nested"/"CMakeLists.txt");std::filesystem::remove_all(temp/"nested"/"target-build");
#endif
            if(!selectToolchain(-193) || !selectToolchain(-250) || app.m_toolchainEditMode!=1) return 870;
#if defined(_WIN32)
            app.m_searchBar.setQuery(R"({"profiles":[{"name":"New","target":"arm-none-eabi","cCompiler":"C:\\sdk\\cc.exe"}]})");
#else
            app.m_searchBar.setQuery(R"({"profiles":[{"name":"New","target":"arm-none-eabi","cCompiler":"/sdk/cc"}]})");
#endif
            app.applyModuleCommand(); if(app.m_toolchainEditMode!=4 || app.m_toolchainSaveFuture.valid()) return 871;
            if(!selectToolchain(-253) || !app.m_toolchainSaveFuture.valid()) return 872;
            const auto saveDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(app.m_toolchainSaveFuture.valid() && std::chrono::steady_clock::now()<saveDeadline){app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(app.m_toolchainSaveFuture.valid() || app.m_commandStatusIsError || app.m_toolchainProfiles.size()!=2) return 873;
            if(!selectToolchain(1) || !selectToolchain(-252) || app.m_toolchainEditMode!=6) return 874;
            {std::ofstream changed(temp/"nested"/"toolchains.json",std::ios::trunc);changed << "external change";}
            if(!selectToolchain(-253) || !app.m_toolchainSaveFuture.valid()) return 875;
            while(app.m_toolchainSaveFuture.valid() && std::chrono::steady_clock::now()<saveDeadline){app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(app.m_toolchainSaveFuture.valid() || !app.m_commandStatusIsError) return 876;
            app.m_toolchainShowing=false; app.m_toolchainSelected.reset();
            { std::ofstream out(temp/"nested"/"board.svd"); out << "<device><size>32</size><access>read-write</access><peripherals><peripheral><name>GPIO</name><baseAddress>0x4000</baseAddress><registers><register><name>CTRL</name><addressOffset>0x10</addressOffset><fields><field><name>EN</name><bitOffset>0</bitOffset><bitWidth>1</bitWidth></field></fields></register><register derivedFrom='CTRL'><name>CTRL_COPY</name><addressOffset>0x20</addressOffset></register><cluster><dim>2</dim><dimIncrement>0x10</dimIncrement><name>PORT%s</name><addressOffset>0x100</addressOffset><register><name>CTRL</name><description>Port &amp; control</description><addressOffset>4</addressOffset><fields><field><dim>2</dim><dimIncrement>2</dimIncrement><dimIndex>A-B</dimIndex><name>READY%s</name><description>Ready &lt;flag&gt;</description><bitOffset>3</bitOffset><bitWidth>1</bitWidth><enumeratedValues><usage>read</usage><enumeratedValue><name>YES</name><value>1</value></enumeratedValue></enumeratedValues></field><field derivedFrom='READYA'><name>READY_COPY</name><bitOffset>9</bitOffset></field></fields></register></cluster></registers></peripheral><peripheral derivedFrom='GPIO'><name>GPIO2</name><baseAddress>0x5000</baseAddress></peripheral><peripheral><dim>2</dim><dimIncrement>0x100</dimIncrement><dimIndex>A-B</dimIndex><name>UART%s</name><baseAddress>0x6000</baseAddress><registers><register><name>STATUS</name><addressOffset>4</addressOffset></register></registers></peripheral><peripheral derivedFrom='GPIO'><name>OTHER</name></peripheral></peripherals></device>"; }
            app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.m_modulePage=0; app.refreshSuggestions(); app.m_selectedSuggestion=2; app.applyModuleCommand();
            if (app.m_searchBar.kind()!=ui::PromptKind::ModuleLinkerPath || !app.m_svdInput) return 842;
            app.m_searchBar.setQuery("board.svd"); app.applyModuleCommand(); const auto svdDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
            while (app.m_svdFuture.valid() && std::chrono::steady_clock::now()<svdDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_svdFuture.valid() || app.m_svdRegisters.registers.size()!=10 || app.m_svdRegisters.registers[8].peripheral!="UARTA" || app.m_svdRegisters.registers[8].address!=0x6004 || app.m_svdRegisters.registers[9].peripheral!="UARTB" || app.m_svdRegisters.registers[9].address!=0x6104 || app.m_svdRegisters.registers[4].peripheral!="GPIO2" || app.m_svdRegisters.registers[4].address!=0x5010 || app.m_svdRegisters.registers[7].address!=0x5114 || app.m_svdRegisters.registers[7].fields.size()!=3 || app.m_svdRegisters.registers[7].fields[2].name!="READY_COPY" || app.m_svdRegisters.registers[7].fields[2].offset!=9 || app.m_svdRegisters.registers[7].fields[2].values.size()!=1 || app.m_svdRegisters.registers[2].name!="PORT0.CTRL" || app.m_svdRegisters.registers[2].address!=0x4104 || app.m_svdRegisters.registers[3].name!="PORT1.CTRL" || app.m_svdRegisters.registers[3].address!=0x4114 || app.m_svdRegisters.registers[1].name!="CTRL_COPY" || app.m_svdRegisters.registers[1].address!=0x4020 || app.m_svdRegisters.registers[1].fields.size()!=1 || app.m_svdRegisters.registers[0].address!=0x4010 || app.m_svdRegisters.registers[0].sizeBits!=32 || app.m_svdRegisters.registers[0].fields[0].access!="read-write" || !app.m_svdRegisters.incomplete) return 843;
            bool sawWarning=false, foundRegister=false; for (std::size_t page=0; page<5 && !foundRegister; ++page) { app.m_modulePage=page; app.refreshSuggestions(); sawWarning |= std::any_of(app.m_suggestions.begin(),app.m_suggestions.end(),[](const auto& item) { return item.path=="SVD limitation" && item.detail.find("derivedFrom")!=std::string_view::npos; }); auto reg=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),3); if(reg!=app.m_moduleChoices.end()) {app.m_selectedSuggestion=static_cast<int>(reg-app.m_moduleChoices.begin());foundRegister=true;} } if (!sawWarning || !foundRegister) return 844;
            app.applyModuleCommand();
            bool sawField=false,sawSecondField=false,sawDerivedField=false,sawRegDescription=false,sawFieldDescription=false,sawEnum=false;for(std::size_t page=0;page<8;++page){app.m_modulePage=page;app.refreshSuggestions();for(const auto& item:app.m_suggestions){sawField |= item.path=="READYA" && item.detail.find("Bits 3")!=std::string_view::npos;sawSecondField |= item.path=="READYB" && item.detail.find("Bits 5")!=std::string_view::npos;sawDerivedField |= item.path=="READY_COPY" && item.detail.find("Bits 9")!=std::string_view::npos;sawRegDescription |= item.path=="Register description" && item.detail=="Port & control";sawFieldDescription |= item.path=="Field description: READYA" && item.detail=="Ready <flag>";sawEnum |= item.path=="READYA: YES" && item.detail.find("0x1")!=std::string_view::npos && item.detail.find("unverified")!=std::string_view::npos;}} if (!app.m_svdSelected || !sawField || !sawSecondField || !sawDerivedField || !sawRegDescription || !sawFieldDescription || !sawEnum) return 845;
            app.m_svdShowing=false; app.m_svdSelected.reset(); std::filesystem::remove(temp/"nested"/"board.svd"); app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected;
            std::filesystem::remove(temp/"nested"/"toolchains.json");
            std::filesystem::remove(temp / "nested" / "firmware.map");
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            const std::string original("\0\x11\x22\x33", 4);
            { std::ofstream out(temp / "nested" / "sample.bin", std::ios::binary); out.write(original.data(), static_cast<std::streamsize>(original.size())); }
            app.m_moduleRoot = temp / "hex-modules"; std::string error;
            const std::string manifest = R"({"name":"Hex","version":"1","requires":["hex-editor-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Open","action":"hex-editor"}]})";
            if (!modules::install(app.m_moduleRoot, "hex", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "hex", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 468;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleHexPath) return 469;
            app.m_searchBar.setQuery("sample.bin"); app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            auto wait = [&] { while (app.m_hexFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); } };
            wait(); if (app.m_hexFuture.valid() || !app.m_hexFile || app.m_hexFile->buffer().bytes() != original) return 470;
            const auto choose = [&](int choice) {
                for (std::size_t page = 0; page < 8; ++page) { app.m_modulePage = page; app.refreshSuggestions(); const auto row = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), choice); if (row != app.m_moduleChoices.end()) { app.m_selectedSuggestion = static_cast<int>(row - app.m_moduleChoices.begin()); app.applyModuleCommand(); return true; } }
                return false;
            };
            if (!choose(-44)) return 471; app.m_searchBar.setQuery("1"); app.applyModuleCommand(); app.m_searchBar.setQuery("FF"); app.applyModuleCommand();
            std::string disk; if (!app.m_project.readFile("sample.bin", disk, error) || disk != original || app.m_hexFile->buffer().bytes()[1] != static_cast<char>(0xff)) return 472;
            if (!choose(-42) || app.m_hexFile->buffer().bytes() != original || !choose(-43) || app.m_hexFile->buffer().bytes()[1] != static_cast<char>(0xff)) return 473;
            { std::ofstream out(temp / "nested" / "sample.bin", std::ios::binary | std::ios::trunc); out << "external"; }
            if (!choose(-41)) return 474; wait(); if (app.m_hexFuture.valid() || !app.m_commandStatusIsError || app.m_hexFile->buffer().bytes()[1] != static_cast<char>(0xff)) return 475;
            { std::ofstream out(temp / "nested" / "sample.bin", std::ios::binary | std::ios::trunc); out.write(original.data(), static_cast<std::streamsize>(original.size())); }
            if (!choose(-41)) return 476; wait(); if (app.m_hexFuture.valid() || app.m_commandStatusIsError || !app.m_project.readFile("sample.bin", disk, error) || disk[1] != static_cast<char>(0xff)) return 477;
            if (!choose(-45)) return 478; app.m_searchBar.setQuery("FF 22"); app.applyModuleCommand(); if (app.m_hexEditOffset != 1 || app.m_hexFile->buffer().selectionLength() != 2) return 479;
            if (!choose(-50)) return 480; app.m_searchBar.setQuery("AA BB"); app.applyModuleCommand();
            if (static_cast<unsigned char>(app.m_hexFile->buffer().bytes()[1]) != 0xaa || !choose(-42) || static_cast<unsigned char>(app.m_hexFile->buffer().bytes()[1]) != 0xff) return 481;
            if (!choose(-49)) return 482; app.m_searchBar.setQuery("1"); app.applyModuleCommand(); app.m_searchBar.setQuery("2"); app.applyModuleCommand();
            if (app.m_hexFile->buffer().selectionLength() != 2 || !choose(-51) || app.m_hexFile->buffer().bytes().size() != 2 || !choose(-42) || app.m_hexFile->buffer().bytes().size() != 4) return 483;
            if (!choose(-52)) return 484; app.m_searchBar.setQuery("4"); app.applyModuleCommand(); app.m_searchBar.setQuery("CC"); app.applyModuleCommand();
            if (app.m_hexFile->buffer().bytes().size() != 5 || !choose(-41)) return 485; wait();
            if (app.m_hexFuture.valid() || app.m_commandStatusIsError || !app.m_project.readFile("sample.bin", disk, error) || disk.size() != 5 || static_cast<unsigned char>(disk.back()) != 0xcc) return 486;
            if (!app.m_hexFile->buffer().insert(5, std::string("\xff\x22", 2))) return 825;
            if (!choose(-45)) return 826; app.m_searchBar.setQuery("FF 22"); app.applyModuleCommand();
            if (app.m_hexEditOffset != 1 || !choose(-205) || app.m_hexEditOffset != 5 || !choose(-205) || app.m_hexEditOffset != 1) return 827;
            if (!choose(-206) || app.m_hexEditOffset != 5 || !choose(-206) || app.m_hexEditOffset != 1 || app.m_hexFile->buffer().selectionLength() != 2) return 828;
            if (!choose(-42) || !app.m_hexLastNeedle.empty()) return 829;
            app.m_hexShowing = false; app.m_hexFile.reset(); app.m_hexBaseline.clear(); app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected;
            std::filesystem::remove(temp / "nested" / "sample.bin");
        }
#if !defined(_WIN32)
        if (const char* ctest = std::getenv("LUCID_TEST_CTEST")) {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected; const auto oldSettings = app.m_settingsRoot;
            const auto historySettings = temp / "ctest-settings"; std::filesystem::create_directories(historySettings); app.m_settingsRoot = std::filesystem::canonical(historySettings).string();
            std::filesystem::create_directories(temp / "nested" / "test-build");
            { std::ofstream out(temp / "nested" / "test-build" / "CTestTestfile.cmake"); out << "add_test(\"works\" \"/usr/bin/true\")\nadd_test(\"fails\" \"/usr/bin/false\")\n"; }
            app.m_moduleRoot = temp / "ctest-modules"; std::string error;
            const std::string manifest = R"({"name":"Tests","version":"1","requires":["ctest-runner-v2"],"permissions":{"project":true,"network":false,"processes":true,"settings":true},"commands":[{"title":"Tests","action":"run-ctest"}]})";
            if (!modules::install(app.m_moduleRoot, "ctest", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "ctest", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 459;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleCTestInput) return 460;
            app.m_searchBar.setQuery("test-build"); app.applyModuleCommand(); app.m_searchBar.setQuery(ctest); app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
            while (app.m_ctestDiscoveryFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_ctestDiscoveryFuture.valid() || app.m_commandStatusIsError || app.m_ctestDiscovery.tests.size() != 2) return 461;
            const auto test = std::find_if(app.m_ctestDiscovery.tests.begin(), app.m_ctestDiscovery.tests.end(), [](const auto& item) { return item.name == "fails"; });
            const auto index = static_cast<int>(test - app.m_ctestDiscovery.tests.begin()); const auto row = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), index); if (row == app.m_moduleChoices.end()) return 462;
            app.m_selectedSuggestion = static_cast<int>(row - app.m_moduleChoices.begin()); app.applyModuleCommand();
            for (int i = 0; i < 2; ++i) {
                const auto run = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -37); if (run == app.m_moduleChoices.end()) return 463;
                app.m_selectedSuggestion = static_cast<int>(run - app.m_moduleChoices.begin()); app.applyModuleCommand();
                while (app.m_ctestRunFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
                if (app.m_ctestRunFuture.valid() || !app.m_ctestLast.contains("fails") || app.m_ctestLast["fails"].status != modules::TestResultStatus::Failed) return 464;
            }
            if (!app.m_ctestPrevious.contains("fails") || !app.m_ctestHistoryReady) return 465;
            const auto summary = std::find_if(app.m_ctestSummaries.begin(), app.m_ctestSummaries.end(), [](const auto& entry) { return entry.name == "fails"; });
            if (summary == app.m_ctestSummaries.end() || summary->observations != 2 || summary->failed != 2 || summary->consecutiveFailures != 2 ||
                std::none_of(app.m_suggestions.begin(), app.m_suggestions.end(), [](const auto& item) { return item.path == "Saved history: 2 observations"; })) return 656;
            bool sawTransitions = false, sawLastFailure = false;
            for (std::size_t page = 0; page < 8; ++page) {
                app.m_modulePage = page; app.refreshSuggestions();
                for (const auto& item : app.m_suggestions) {
                    if (item.path == "Pass/fail transitions: 0" && item.detail.find("not proof") != std::string::npos) sawTransitions = true;
                    if (item.path == "Last failure: saved run 2 of 2" && item.detail.find("streak: 2") != std::string::npos) sawLastFailure = true;
                }
            }
            if (!sawTransitions || !sawLastFailure) return 840;
            app.m_modulePage = 0; app.refreshSuggestions(); auto ctestBack = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -36); if (ctestBack == app.m_moduleChoices.end()) return 845;
            app.m_selectedSuggestion = static_cast<int>(ctestBack - app.m_moduleChoices.begin()); app.applyModuleCommand();
            auto plan = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -215); if (plan == app.m_moduleChoices.end()) return 846;
            app.m_selectedSuggestion = static_cast<int>(plan - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (!app.m_ctestRerunShowing || app.m_ctestRerunCandidates.size() != 1 || app.m_ctestRerunCandidates[0].name != "fails" || app.m_ctestRunFuture.valid()) return 847;
            auto candidate = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 0); if (candidate == app.m_moduleChoices.end()) return 848;
            app.m_selectedSuggestion = static_cast<int>(candidate - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (!app.m_ctestRerunSelected || app.m_ctestRunFuture.valid() || std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -217) == app.m_moduleChoices.end()) return 849;
            ctestBack = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -36); app.m_selectedSuggestion = static_cast<int>(ctestBack - app.m_moduleChoices.begin()); app.applyModuleCommand();
            ctestBack = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -36); app.m_selectedSuggestion = static_cast<int>(ctestBack - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_ctestRerunShowing || app.m_ctestRunFuture.valid()) return 850;
            modules::CTestHistorySnapshot history;
            if (!modules::loadCTestHistory(app.m_settingsRoot, app.m_project.root(), app.m_ctestBuild, history, error) || history.runs.size() != 2 || history.runs.back().tests[0].status != modules::TestResultStatus::Failed || !history.runs.back().tests[0].failureText.empty()) return 466;
            app.m_ctestShowing = false; app.m_ctestSelected.reset(); app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.m_modulePage = 0; app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            app.m_searchBar.setQuery("test-build"); app.applyModuleCommand(); app.m_searchBar.setQuery(ctest); app.applyModuleCommand();
            while (app.m_ctestDiscoveryFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_ctestDiscoveryFuture.valid() || !app.m_ctestPrevious.contains("fails") || app.m_ctestLast["fails"].status != modules::TestResultStatus::Failed || !app.m_ctestLast["fails"].output.empty()) return 467;
            app.m_ctestShowing = false; app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected; app.m_settingsRoot = oldSettings;
            std::filesystem::remove_all(temp / "nested" / "test-build");
        }
#endif
#if !defined(_WIN32)
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected; const auto oldSettings = app.m_settingsRoot;
            const auto interpreter = temp / "nested" / ".venv" / "bin" / "python"; std::filesystem::create_directories(interpreter.parent_path());
            { std::ofstream out(interpreter); out << "#!/bin/sh\ncase \"$3\" in *LUCID_PYTHON_VERSION*) printf 'LUCID_PYTHON_VERSION=3.12.0\\n'; exit 0;; esac\nif [ \"$1\" = \"-I\" ]; then printf 'invoked\\n' >> \"$0.provision\"; exit 0; fi\nprintf 'LUCID_PYTHON_VERSION=3.12.0\\n'\n"; }
            std::filesystem::permissions(interpreter, std::filesystem::perms::owner_all);
            { std::ofstream out(temp / "nested" / ".venv" / "pyvenv.cfg"); out << "version = 3.12.0\n"; }
            const auto settings = temp / "python-user-settings"; std::filesystem::create_directories(settings); app.m_settingsRoot = std::filesystem::canonical(settings).string();
            app.m_moduleRoot = temp / "python-manager-modules"; std::string error;
            const std::string manifest = R"({"name":"Python","version":"1","requires":["python-environments-v2","python-pylsp-install-v1"],"permissions":{"project":true,"network":true,"processes":true,"settings":true},"commands":[{"title":"Manage","action":"manage-python-environments"},{"title":"Install","action":"install-python-pylsp"}]})";
            if (!modules::install(app.m_moduleRoot, "python", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "python", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 452;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
            while (app.m_pythonDiscoveryFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_pythonDiscoveryFuture.valid() || !app.m_pythonPreferenceReady || app.m_pythonEnvironments.empty()) return 453;
            const auto candidate = std::find_if(app.m_pythonEnvironments.begin(), app.m_pythonEnvironments.end(), [&](const auto& env) { std::error_code ec; return std::filesystem::equivalent(env.interpreter, interpreter, ec) && !ec; });
            if (candidate == app.m_pythonEnvironments.end()) return 454;
            const auto index = static_cast<std::size_t>(candidate - app.m_pythonEnvironments.begin()); app.m_modulePage = (index + 1) / app.pagedContentCount(); app.refreshSuggestions();
            const auto row = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), static_cast<int>(index)); if (row == app.m_moduleChoices.end()) return 455;
            app.m_selectedSuggestion = static_cast<int>(row - app.m_moduleChoices.begin()); app.applyModuleCommand();
            const auto select = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -33); if (select == app.m_moduleChoices.end()) return 456;
            app.m_selectedSuggestion = static_cast<int>(select - app.m_moduleChoices.begin()); app.applyModuleCommand();
            while (app.m_pythonRuntimeFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_pythonRuntimeFuture.valid() || app.m_commandStatusIsError || app.m_pythonPreference.interpreter.empty() || !std::filesystem::equivalent(app.m_pythonPreference.interpreter, interpreter)) return 457;
            modules::PythonPreference loaded; if (!modules::loadPythonPreference(app.m_settingsRoot, app.m_project.root(), loaded, error) || loaded.interpreter != app.m_pythonPreference.interpreter) return 458;
            app.m_pythonSelected = static_cast<std::size_t>(candidate - app.m_pythonEnvironments.begin()); app.refreshSuggestions();
            auto installChoice = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -160);
            const auto marker = std::filesystem::path(interpreter.string() + ".provision");
            if (installChoice == app.m_moduleChoices.end() || std::filesystem::exists(marker)) return 672;
            app.m_selectedSuggestion = static_cast<int>(installChoice - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (!app.m_pythonProvisionBootstrap || *app.m_pythonProvisionBootstrap || std::filesystem::exists(marker)) return 673;
            auto confirmInstall = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -162);
            if (confirmInstall == app.m_moduleChoices.end()) return 674;
            app.m_selectedSuggestion = static_cast<int>(confirmInstall - app.m_moduleChoices.begin()); app.applyModuleCommand();
            while (app.m_pythonProvisionFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_pythonProvisionFuture.valid() || app.m_commandStatusIsError || !std::filesystem::exists(marker) || app.m_commandStatus.find("installed and verified") == std::string::npos) return 675;
            app.m_pythonShowing = false; app.m_searchBar.close();
            { std::ofstream source(temp / "nested" / "language.py"); source << "print(1)\n"; }
            app.openFile("language.py", false);
            if (app.m_languageKind != "python" || app.m_languagePythonInterpreter != loaded.interpreter) return 670;
            const auto pythonDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (!app.m_languageRoot.empty() && std::chrono::steady_clock::now() < pythonDeadline) { app.pollLanguageServer(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (!app.m_languageRoot.empty() || !app.m_statusIsError || app.m_statusText.find("python-lsp-server") == std::string::npos) return 671;
            app.openFile("example.txt", false); std::filesystem::remove(temp / "nested" / "language.py");
            app.m_pythonShowing = true; app.m_pythonSelected.reset(); app.m_searchBar.open(ui::PromptKind::ModuleCommands);
            std::promise<decltype(app)::PythonDiscoveryResult> omittedPromise;
            decltype(app)::PythonDiscoveryResult omitted; omitted.preference = loaded;
            omitted.preference.interpreter = temp / "outside-discovery" / "python";
            omittedPromise.set_value(std::move(omitted)); app.m_pythonDiscoveryFuture = omittedPromise.get_future(); app.pollModules();
            if (app.m_pythonEnvironments.size() != 1 || app.m_pythonEnvironments[0].interpreter != temp / "outside-discovery" / "python" || app.m_pythonUnlistedInterpreter != app.m_pythonEnvironments[0].interpreter) return 830;
            auto manual = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -207); if (manual == app.m_moduleChoices.end()) return 831;
            app.m_selectedSuggestion = static_cast<int>(manual - app.m_moduleChoices.begin()); app.applyModuleCommand();
            app.m_searchBar.setQuery("relative/python"); app.applyModuleCommand(); if (!app.m_commandStatusIsError || app.m_searchBar.kind() != ui::PromptKind::ModulePythonInterpreterPath) return 832;
            const auto manualPath = temp / "not-discovered" / "python"; app.m_searchBar.setQuery(manualPath.string()); app.applyModuleCommand();
            if (!app.m_pythonSelected || app.m_pythonEnvironments[*app.m_pythonSelected].interpreter != manualPath || app.m_pythonRuntimeFuture.valid() || std::filesystem::exists(manualPath)) return 833;
            app.m_pythonShowing = false; app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected; app.m_settingsRoot = oldSettings;
            std::filesystem::remove_all(temp / "nested" / ".venv");
        }
#endif
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
#if defined(_WIN32)
            const auto interpreter = temp / "nested" / ".venv" / "Scripts" / "python.exe";
#else
            const auto interpreter = temp / "nested" / ".venv" / "bin" / "python";
#endif
            std::filesystem::create_directories(interpreter.parent_path());
            { std::ofstream out(interpreter); out << "candidate"; }
#if !defined(_WIN32)
            std::filesystem::permissions(interpreter, std::filesystem::perms::owner_all);
#endif
            { std::ofstream out(temp / "nested" / ".venv" / "pyvenv.cfg"); out << "version = 3.12.0\n"; }
            app.m_moduleRoot = temp / "python-modules"; std::string error;
            const std::string manifest = R"({"name":"Python","version":"1","requires":["python-environments-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Browse","action":"browse-python-environments"}]})";
            if (!modules::install(app.m_moduleRoot, "python", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "python", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 450;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_pythonDiscoveryFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_pythonDiscoveryFuture.valid() || app.m_commandStatusIsError || std::none_of(app.m_pythonEnvironments.begin(), app.m_pythonEnvironments.end(), [&](const auto& env) { std::error_code ec; return env.projectLocal && std::filesystem::equivalent(env.interpreter, interpreter, ec) && !ec && env.version == "3.12.0"; })) return 451;
            app.m_pythonShowing = false; app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected;
            std::filesystem::remove_all(temp / "nested" / ".venv");
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            { std::ofstream out(temp / "nested" / "a.log"); out << "2026-11-04T11:00:00Z error later\nnoise\n"; }
            { std::ofstream out(temp / "nested" / "b.log"); out << "Nov  4 10:00:00 error earlier\n  detail\n"; }
            app.m_moduleRoot = temp / "log-modules"; std::string error;
            const std::string manifest = R"({"name":"Logs","version":"1","requires":["log-correlation-v1","log-json-lines-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Correlate","action":"log-correlation"},{"title":"Structured","action":"log-correlation-json"}]})";
            if (!modules::install(app.m_moduleRoot, "logs", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "logs", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 446;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleLogsInput) return 447;
            app.m_searchBar.setQuery("a.log"); app.applyModuleCommand(); app.m_searchBar.setQuery("b.log"); app.applyModuleCommand(); app.m_searchBar.setQuery(""); app.applyModuleCommand();
            app.m_searchBar.setQuery("error"); app.applyModuleCommand(); app.m_searchBar.setQuery("2026 +00:00"); app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_logFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_logFuture.valid() || app.m_logReport.records.size() != 2 || app.m_logReport.records[0].source != "b.log" || app.m_logReport.unparseableLines != 1 || app.m_logReport.records[0].text.find("detail") == std::string::npos) return 448;
            app.m_selectedSuggestion = 1; app.applyModuleCommand(); if (!app.m_logSelected || *app.m_logSelected != 0) return 449;
            { std::ofstream out(temp / "nested" / "b.log",std::ios::trunc); out << "changed\n"; }
            auto openLog=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-180); if (openLog==app.m_moduleChoices.end()) return 725;
            app.m_selectedSuggestion=static_cast<int>(openLog-app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (!app.m_logShowing || !app.m_commandStatusIsError || app.m_filePath=="b.log") return 726;
            { std::ofstream out(temp / "nested" / "b.log",std::ios::trunc); out << "Nov  4 10:00:00 error earlier\n  detail\n"; }
            openLog=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-180); if (openLog==app.m_moduleChoices.end()) return 727;
            app.m_selectedSuggestion=static_cast<int>(openLog-app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_filePath!="b.log" || app.m_document.cursorLine()!=0 || app.m_searchBar.isOpen()) return 728;
            app.openFile("example.txt",false);
            {std::ofstream out(temp/"nested"/"structured.log");out << R"({"timestamp":"2026-11-04T11:00:00Z","message":"service"})" << "\n";}
            app.m_logShowing=false; app.m_logSelected.reset(); app.m_modulePage=0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion=1; app.applyModuleCommand();
            if(app.m_searchBar.kind()!=ui::PromptKind::ModuleLogsInput || !app.m_logJson) return 1046;
            app.m_searchBar.setQuery("structured.log"); app.applyModuleCommand(); app.m_searchBar.setQuery(""); app.applyModuleCommand(); app.m_searchBar.setQuery(""); app.applyModuleCommand(); app.m_searchBar.setQuery(""); app.applyModuleCommand();
            const auto jsonDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(15); while(app.m_logFuture.valid() && std::chrono::steady_clock::now()<jsonDeadline) {app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(app.m_logFuture.valid() || app.m_logReport.jsonLines!=1 || app.m_logReport.records.size()!=1 || app.m_logReport.records[0].line!=1) return 1047;
            auto structuredRow=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),0); if(structuredRow==app.m_moduleChoices.end()) return 1048; app.m_selectedSuggestion=static_cast<int>(structuredRow-app.m_moduleChoices.begin());app.applyModuleCommand();
            auto structuredOpen=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-180); if(structuredOpen==app.m_moduleChoices.end()) return 1049; app.m_selectedSuggestion=static_cast<int>(structuredOpen-app.m_moduleChoices.begin());app.applyModuleCommand();
            if(app.m_filePath!="structured.log" || app.m_document.cursorLine()!=0 || app.m_searchBar.isOpen()) return 1050;
            app.openFile("example.txt",false); std::filesystem::remove(temp/"nested"/"structured.log");
            app.m_logShowing = false; app.m_logSelected.reset(); app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected;
            std::filesystem::remove(temp / "nested" / "a.log"); std::filesystem::remove(temp / "nested" / "b.log");
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            { std::ofstream out(temp / "nested" / "tokens.css"); out << ":root {\n  --primary: #fff;\n}\n"; }
            { std::ofstream out(temp / "nested" / "component.css"); out << ".button { color: var(--primary); }\n"; }
            app.m_moduleRoot = temp / "design-modules"; std::string error;
            const std::string manifest = R"({"name":"Tokens","version":"1","requires":["design-token-explorer-v1","design-token-aliases-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Explore","action":"design-token-explorer"}]})";
            if (!modules::install(app.m_moduleRoot, "tokens", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "tokens", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 441;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleDesignTokensPath) return 442;
            app.m_searchBar.setQuery("tokens.css"); app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_designTokenFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_designTokenFuture.valid() || app.m_commandStatusIsError || app.m_designTokenReport.tokens.size() != 1 || app.m_designTokenReport.tokens[0].name != "--primary") return 443;
            app.m_selectedSuggestion = 1; app.applyModuleCommand();
            if (!app.m_designTokenSelected) return 730;
            { std::ofstream out(temp / "nested" / "tokens.css",std::ios::trunc); out << ":root {\n  --primary: changed;\n}\n"; }
            auto openToken=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-181); if (openToken==app.m_moduleChoices.end()) return 731;
            app.m_selectedSuggestion=static_cast<int>(openToken-app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (!app.m_designTokenShowing || !app.m_commandStatusIsError || app.m_filePath=="tokens.css") return 732;
            { std::ofstream out(temp / "nested" / "tokens.css",std::ios::trunc); out << ":root {\n  --primary: #fff;\n}\n"; }
            openToken=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-181); if (openToken==app.m_moduleChoices.end()) return 733;
            app.m_selectedSuggestion=static_cast<int>(openToken-app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_filePath!="tokens.css" || app.m_document.cursorLine()!=1 || app.m_document.cursorColumn()!=2 || app.m_searchBar.isOpen()) return 734;
            app.openFile("example.txt",false); app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion=0; app.applyModuleCommand(); app.m_searchBar.setQuery("tokens.css"); app.applyModuleCommand();
            while (app.m_designTokenFuture.valid() && std::chrono::steady_clock::now()<deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            app.m_selectedSuggestion=1; app.applyModuleCommand(); auto searchToken=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-182); if (searchToken==app.m_moduleChoices.end()) return 735;
            app.m_selectedSuggestion=static_cast<int>(searchToken-app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ProjectSearch || app.m_searchBar.query() != "--primary") return 444;
            app.applyProjectSearch();
            while (app.m_projectSearchFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollProjectSearch(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_projectSearchFuture.valid() || std::none_of(app.m_projectSearchResults.files.begin(), app.m_projectSearchResults.files.end(), [](const auto& file) { return file.path == "component.css"; })) return 445;
            const std::string usageCss = ":root {\r\n  --primary: #fff;\r\n  color: var(\f    --primary\r\n  );\r\n}\r\n";
            { std::ofstream out(temp / "nested" / "uses.css", std::ios::binary); out << usageCss; }
            app.m_designTokenShowing=false; app.m_designTokenSelected.reset(); app.m_modulePage=0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion=0; app.applyModuleCommand(); app.m_searchBar.setQuery("uses.css"); app.applyModuleCommand();
            const auto usageDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(15); while(app.m_designTokenFuture.valid() && std::chrono::steady_clock::now()<usageDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if(app.m_designTokenFuture.valid() || app.m_designTokenReport.tokens.size()!=1 || app.m_designTokenUsages.usages.size()!=1) return 1101;
            auto tokenRow=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),0); if(tokenRow==app.m_moduleChoices.end()) return 1102; app.m_selectedSuggestion=static_cast<int>(tokenRow-app.m_moduleChoices.begin()); app.applyModuleCommand();
            auto viewReferences=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-313); if(viewReferences==app.m_moduleChoices.end()) return 1103; app.m_selectedSuggestion=static_cast<int>(viewReferences-app.m_moduleChoices.begin()); app.applyModuleCommand();
            auto reference=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),0); if(reference==app.m_moduleChoices.end()) return 1104;
            { std::ofstream out(temp / "nested" / "uses.css", std::ios::trunc); out << "changed"; }
            app.m_selectedSuggestion=static_cast<int>(reference-app.m_moduleChoices.begin()); app.applyModuleCommand(); if(!app.m_commandStatusIsError || !app.m_designTokenUsageShowing || app.m_filePath=="uses.css") return 1105;
            { std::ofstream out(temp / "nested" / "uses.css", std::ios::binary | std::ios::trunc); out << usageCss; }
            reference=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),0); if(reference==app.m_moduleChoices.end()) return 1106; app.m_selectedSuggestion=static_cast<int>(reference-app.m_moduleChoices.begin()); app.applyModuleCommand();
            if(app.m_filePath!="uses.css" || app.m_document.cursorLine()!=2 || app.m_document.cursorColumn()!=18 || app.m_searchBar.isOpen()) return 1107;
            app.openFile("example.txt",false); std::filesystem::remove(temp / "nested" / "uses.css");
            const std::string aliasJson=R"({"base":{"$value":"#123"},"alias":{"$value":"{base}"},"cycle":{"$value":"{cycle}"},"missing":{"$value":"{absent}"}})";
            {std::ofstream out(temp/"nested"/"tokens.json");out<<aliasJson;}
            app.m_designTokenShowing=false;app.m_modulePage=0;app.m_searchBar.open(ui::PromptKind::ModuleCommands);app.refreshSuggestions();app.m_selectedSuggestion=0;app.applyModuleCommand();app.m_searchBar.setQuery("tokens.json");app.applyModuleCommand();
            const auto aliasDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);while(app.m_designTokenFuture.valid() && std::chrono::steady_clock::now()<aliasDeadline){app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(app.m_designTokenFuture.valid() || !app.m_designTokenAliasEnabled || app.m_designTokenAliases.size()!=4 || app.m_designTokenAliases[1].status!=modules::DesignAliasStatus::Resolved || app.m_designTokenAliases[2].status!=modules::DesignAliasStatus::Cycle || app.m_designTokenAliases[3].status!=modules::DesignAliasStatus::Missing) return 1056;
            auto aliasRow=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),1);if(aliasRow==app.m_moduleChoices.end())return 1057;app.m_selectedSuggestion=static_cast<int>(aliasRow-app.m_moduleChoices.begin());app.applyModuleCommand();
            auto follow=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-312);if(follow==app.m_moduleChoices.end())return 1058;
            {std::ofstream out(temp/"nested"/"tokens.json",std::ios::trunc);out<<"changed";}
            app.m_selectedSuggestion=static_cast<int>(follow-app.m_moduleChoices.begin());app.applyModuleCommand();if(!app.m_commandStatusIsError || !app.m_designTokenSelected || *app.m_designTokenSelected!=1)return 1059;
            {std::ofstream out(temp/"nested"/"tokens.json",std::ios::trunc);out<<aliasJson;}
            follow=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-312);if(follow==app.m_moduleChoices.end())return 1060;app.m_selectedSuggestion=static_cast<int>(follow-app.m_moduleChoices.begin());app.applyModuleCommand();
            if(!app.m_designTokenSelected || *app.m_designTokenSelected!=0)return 1061;
            app.m_selectedSuggestion=0;app.applyModuleCommand();auto cycleRow=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),2);if(cycleRow==app.m_moduleChoices.end())return 1062;app.m_selectedSuggestion=static_cast<int>(cycleRow-app.m_moduleChoices.begin());app.applyModuleCommand();
            if(std::none_of(app.m_suggestions.begin(),app.m_suggestions.end(),[](const auto& item){return item.path=="Alias status" && item.detail=="Cycle";}))return 1063;
            app.m_selectedSuggestion=0;app.applyModuleCommand();auto missingRow=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),3);if(missingRow==app.m_moduleChoices.end())return 1064;app.m_selectedSuggestion=static_cast<int>(missingRow-app.m_moduleChoices.begin());app.applyModuleCommand();
            if(std::none_of(app.m_suggestions.begin(),app.m_suggestions.end(),[](const auto& item){return item.path=="Alias status" && item.detail=="Missing target";}))return 1065;
            app.m_designTokenShowing=false;app.m_designTokenSelected.reset();app.m_designTokenAliases.clear();std::filesystem::remove(temp/"nested"/"tokens.json");
            app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected;
            std::filesystem::remove(temp / "nested" / "tokens.css"); std::filesystem::remove(temp / "nested" / "component.css");
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            { std::ofstream out(temp / "nested" / "source.json"); out << R"({"greeting":"Hello {name}","missing":"Please save"})"; }
            { std::ofstream out(temp / "nested" / "translation.json"); out << R"({"greeting":"مرحبا {user}"})"; }
            app.m_moduleRoot = temp / "localization-modules"; std::string error;
            const std::string manifest = R"({"name":"Localization","version":"1","requires":["localization-workspace-v1","localization-edit-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Compare","action":"compare-localization"}]})";
            if (!modules::install(app.m_moduleRoot, "localization", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "localization", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 437;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleLocalizationPath) return 438;
            app.m_searchBar.setQuery("source.json"); app.applyModuleCommand(); app.m_searchBar.setQuery("translation.json"); app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_localizationFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_localizationFuture.valid() || app.m_localizationEntries.size() != 2 || app.m_localizationEntries[0].key != "/missing" || app.m_localizationEntries[0].inTranslation || !app.m_localizationEntries[1].translationHasRtl) return 439;
            app.m_selectedSuggestion = 2; app.applyModuleCommand();
            if (!app.m_localizationSelected || *app.m_localizationSelected != 1) return 440;
            bool sawTranslation = false, sawMissing = false, sawExtra = false;
            for (std::size_t page = 0; page < 8; ++page) {
                app.m_modulePage = page; app.refreshSuggestions();
                for (const auto& text : app.m_suggestionPaths) { sawTranslation |= text == "مرحبا {user}"; sawMissing |= text == "Missing placeholder: {name}"; sawExtra |= text == "Extra placeholder: {user}"; }
            }
            if (!sawTranslation || !sawMissing || !sawExtra) return 564;
            { std::ofstream out(temp / "nested" / "translation.json", std::ios::trunc); out << R"({"greeting":"changed"})"; }
            app.m_modulePage = 0; app.refreshSuggestions(); auto openTranslation = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -173);
            if (openTranslation == app.m_moduleChoices.end()) return 570;
            app.m_selectedSuggestion = static_cast<int>(openTranslation - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (!app.m_localizationShowing || !app.m_commandStatusIsError || app.m_filePath == "translation.json") return 571;
            { std::ofstream out(temp / "nested" / "translation.json", std::ios::trunc); out << R"({"greeting":"مرحبا {user}"})"; }
            openTranslation = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -173); if (openTranslation == app.m_moduleChoices.end()) return 573;
            app.m_selectedSuggestion = static_cast<int>(openTranslation - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_filePath != "translation.json" || app.m_document.cursorLine() != 0 || app.m_document.cursorColumn() != 12 || app.m_searchBar.isOpen()) return 572;
            app.openFile("example.txt", false);
            app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion=0; app.applyModuleCommand(); app.m_searchBar.setQuery("source.json"); app.applyModuleCommand(); app.m_searchBar.setQuery("translation.json"); app.applyModuleCommand();
            const auto editDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
            while (app.m_localizationFuture.valid() && std::chrono::steady_clock::now()<editDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            auto selectLocalization = [&](int code) { app.refreshSuggestions(); auto it=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),code); if (it==app.m_moduleChoices.end()) return false; app.m_selectedSuggestion=static_cast<int>(it-app.m_moduleChoices.begin()); app.applyModuleCommand(); return true; };
            if (!selectLocalization(0) || !selectLocalization(-270) || app.m_searchBar.kind()!=ui::PromptKind::ModuleLocalizationEdit) return 980;
            app.m_searchBar.setQuery("Bitte speichern"); app.applyModuleCommand();
            if (!app.m_localizationEditPreview || !app.m_localizationEditPlan.inserted || app.m_filePath=="translation.json") return 981;
            { std::ofstream out(temp / "nested" / "translation.json", std::ios::trunc); out << R"({"greeting":"changed"})"; }
            if (!selectLocalization(-271) || !app.m_commandStatusIsError || app.m_filePath=="translation.json") return 982;
            { std::ofstream out(temp / "nested" / "translation.json", std::ios::trunc); out << R"({"greeting":"مرحبا {user}"})"; }
            if (!selectLocalization(-271) || app.m_filePath!="translation.json" || !app.m_document.dirty() || app.m_document.toUtf8().find("Bitte speichern")==std::string::npos || app.m_searchBar.isOpen()) return 983;
            app.m_document.undo(); if (app.m_document.dirty() || app.m_document.toUtf8()!=R"({"greeting":"مرحبا {user}"})") return 984;
            app.openFile("example.txt", false);
            app.m_localizationShowing = false; app.m_localizationSelected.reset(); app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected;
            std::filesystem::remove(temp / "nested" / "source.json"); std::filesystem::remove(temp / "nested" / "translation.json");
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            std::string pe(88, '\0'); pe.replace(0, 2, "MZ"); pe[60] = 64; pe.replace(64, 4, std::string("PE\0\0", 4)); pe[68] = static_cast<char>(0x64); pe[69] = static_cast<char>(0x86);
            pe.resize(17U * 1024U * 1024U, '\0');
            { std::ofstream out(temp / "nested" / "fixture.bin", std::ios::binary); out.write(pe.data(), static_cast<std::streamsize>(pe.size())); }
            app.m_moduleRoot = temp / "binary-modules"; std::string error;
            std::string editorRead; if (app.m_project.readFile("fixture.bin", editorRead, error)) return 436;
            const std::string manifest = R"({"name":"Binary","version":"1","requires":["binary-inspector-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Inspect","action":"binary-inspector"}]})";
            if (!modules::install(app.m_moduleRoot, "binary", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "binary", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 433;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleBinaryPath) return 434;
            app.m_searchBar.setQuery("fixture.bin"); app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_binaryFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_binaryFuture.valid() || !app.m_binaryInspection.incomplete || app.m_binaryInspection.format != "PE" || app.m_binaryInspection.architecture != "x86-64") return 435;
            app.m_binaryInspection.sections = {{".text", 10, 20}};
            app.m_binaryInspection.libraries = {"libexample"}; app.m_binaryInspection.symbols = {"example_symbol"};
            app.m_binaryInspection.peImports = {{"CreateThing","sample.dll","",0,0,false},{"","sample.dll","",42,0,true}};
            app.m_binaryInspection.peExports = {{"Exported","","other.Target",7,0x1234,false}};
            app.refreshSuggestions();
            auto binaryChoice = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 2);
            if (binaryChoice == app.m_moduleChoices.end()) return 607;
            app.m_selectedSuggestion = static_cast<int>(binaryChoice - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (!app.m_binaryCategory || *app.m_binaryCategory != 2 || std::none_of(app.m_suggestions.begin(), app.m_suggestions.end(), [](const auto& entry) { return entry.path == "example_symbol"; }) || std::any_of(app.m_suggestions.begin(), app.m_suggestions.end(), [](const auto& entry) { return entry.path == "libexample"; })) return 608;
            binaryChoice = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -23);
            if (binaryChoice == app.m_moduleChoices.end()) return 609;
            app.m_selectedSuggestion = static_cast<int>(binaryChoice - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_binaryCategory || !app.m_binaryShowing) return 610;
            binaryChoice = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 0);
            if (binaryChoice == app.m_moduleChoices.end()) return 611;
            app.m_selectedSuggestion = static_cast<int>(binaryChoice - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (!app.m_binaryCategory || *app.m_binaryCategory != 0 || std::none_of(app.m_suggestions.begin(), app.m_suggestions.end(), [](const auto& entry) { return entry.path == ".text" && entry.detail.find("Offset 10") != std::string_view::npos; })) return 612;
            app.m_selectedSuggestion=0; app.applyModuleCommand();
            const auto selectBinaryCategory=[&](int category) { for (std::size_t page=0;page<5;++page) { app.m_modulePage=page; app.refreshSuggestions(); auto row=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),category); if (row!=app.m_moduleChoices.end()) { app.m_selectedSuggestion=static_cast<int>(row-app.m_moduleChoices.begin()); app.applyModuleCommand(); return true; } } return false; };
            if (!selectBinaryCategory(3) || std::none_of(app.m_suggestions.begin(),app.m_suggestions.end(),[](const auto& e) { return e.path=="CreateThing" && e.detail.find("sample.dll")!=std::string_view::npos; }) || std::none_of(app.m_suggestions.begin(),app.m_suggestions.end(),[](const auto& e) { return e.path=="Ordinal 42"; })) return 800;
            app.m_selectedSuggestion=0; app.applyModuleCommand();
            if (!selectBinaryCategory(4) || std::none_of(app.m_suggestions.begin(),app.m_suggestions.end(),[](const auto& e) { return e.path=="Exported" && e.detail.find("0x1234")!=std::string_view::npos && e.detail.find("other.Target")!=std::string_view::npos; })) return 801;
            // Selecting another universal slice reads the captured snapshot, not a changed disk file.
            std::string universal(160, '\0');
            auto be = [&](std::size_t p, unsigned long long v, unsigned n) { for (unsigned i=0; i<n; ++i) universal[p+i]=static_cast<char>(v >> (8*(n-i-1))); };
            auto le = [&](std::size_t p, unsigned long long v, unsigned n) { for (unsigned i=0; i<n; ++i) universal[p+i]=static_cast<char>(v >> (8*i)); };
            universal.replace(0,4,std::string("\xca\xfe\xba\xbe",4)); be(4,2,4); be(8,0x1000007,4); be(16,64,4); be(20,32,4); be(28,0x100000c,4); be(36,112,4); be(40,32,4);
            universal.replace(64,4,std::string("\xcf\xfa\xed\xfe",4)); le(68,0x1000007,4); universal.replace(112,4,std::string("\xcf\xfa\xed\xfe",4)); le(116,0x100000c,4);
            app.m_binaryInspection=modules::inspectBinary(universal); app.m_binarySnapshot=std::make_shared<const std::string>(universal); app.m_binarySlice=0; app.m_binaryCategory.reset(); app.m_modulePage=0; app.refreshSuggestions();
            if (!selectBinaryCategory(5) || !app.m_binaryCategory || *app.m_binaryCategory!=5) return 825;
            auto secondSlice=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),1); if (secondSlice==app.m_moduleChoices.end()) return 826;
            { std::ofstream out(temp / "nested" / "fixture.bin",std::ios::binary | std::ios::trunc); out << "changed"; }
            app.m_selectedSuggestion=static_cast<int>(secondSlice-app.m_moduleChoices.begin()); app.applyModuleCommand();
            const auto sliceDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
            while (app.m_binaryFuture.valid() && std::chrono::steady_clock::now()<sliceDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_binaryFuture.valid() || app.m_binarySlice!=1 || app.m_binaryInspection.architecture!="AArch64" || !app.m_binarySnapshot || app.m_binaryCategory) return 827;
            app.m_binaryShowing = false; app.m_binarySnapshot.reset(); app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected;
            std::filesystem::remove(temp / "nested" / "fixture.bin");
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            const auto report = [&](const char* file, int value) { std::ofstream out(temp / "nested" / file); out << "{\"context\":{\"machine\":\"host\",\"build\":\"release\",\"environment\":\"local\"},\"measurements\":[{\"name\":\"parse\",\"unit\":\"ms\",\"direction\":\"lower\",\"value\":" << value << "}]}"; };
            report("before.json", 100); report("after.json", 120);
            app.m_moduleRoot = temp / "benchmark-modules"; std::string error;
            const std::string manifest = R"({"name":"Benchmark","version":"1","requires":["benchmark-comparison-v1","benchmark-samples-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Compare","action":"benchmark-comparison"}]})";
            if (!modules::install(app.m_moduleRoot, "benchmark", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "benchmark", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 430;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleBenchmarkPath) return 431;
            app.m_searchBar.setQuery("before.json"); app.applyModuleCommand(); app.m_searchBar.setQuery("after.json"); app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_benchmarkFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_benchmarkFuture.valid() || app.m_commandStatusIsError || app.m_benchmarkComparison.entries.size() != 1 || app.m_benchmarkComparison.entries[0].assessment != modules::BenchmarkAssessment::PossibleRegression) return 432;
            auto regressions = std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-178); if (regressions==app.m_moduleChoices.end()) return 720;
            app.m_selectedSuggestion=static_cast<int>(regressions-app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_benchmarkFilter!=1) return 721;
            auto benchmark = std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),0); if (benchmark==app.m_moduleChoices.end()) return 722;
            app.m_selectedSuggestion=static_cast<int>(benchmark-app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (!app.m_benchmarkSelected || *app.m_benchmarkSelected!=0 || std::none_of(app.m_suggestions.begin(),app.m_suggestions.end(),[](const auto& item) { return item.path=="Baseline" && item.detail.find("100 ms")!=std::string_view::npos && item.detail.find("1 sample")!=std::string_view::npos; })) return 723;
            app.m_selectedSuggestion=0; app.applyModuleCommand(); if (app.m_benchmarkSelected || app.m_benchmarkFilter!=1) return 724;
            {std::ofstream out(temp/"nested"/"before.json",std::ios::trunc);out << R"({"context":{"machine":"host","build":"release","environment":"local"},"measurements":[{"name":"parse","unit":"ms","direction":"lower","samples":[90,100,110]}]})";}
            {std::ofstream out(temp/"nested"/"after.json",std::ios::trunc);out << R"({"context":{"machine":"host","build":"release","environment":"local"},"measurements":[{"name":"parse","unit":"ms","direction":"lower","samples":[110,120,130]}]})";}
            app.m_benchmarkShowing=false; app.m_modulePage=0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion=0; app.applyModuleCommand(); app.m_searchBar.setQuery("before.json"); app.applyModuleCommand(); app.m_searchBar.setQuery("after.json"); app.applyModuleCommand();
            const auto sampleDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(15); while(app.m_benchmarkFuture.valid() && std::chrono::steady_clock::now()<sampleDeadline) {app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(app.m_benchmarkFuture.valid() || app.m_benchmarkComparison.entries.size()!=1 || !app.m_benchmarkComparison.entries[0].baselineHasSpread) return 1051;
            auto sampleRow=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),0); if(sampleRow==app.m_moduleChoices.end()) return 1052; app.m_selectedSuggestion=static_cast<int>(sampleRow-app.m_moduleChoices.begin()); app.applyModuleCommand();
            bool spreadVisible=false, inferenceVisible=false; for(std::size_t page=0;page<10;++page) {app.m_modulePage=page;app.refreshSuggestions();for(const auto& item:app.m_suggestions){spreadVisible |= item.path=="Baseline variability" && item.detail.find("min 90")!=std::string_view::npos && item.detail.find("sample SD 10")!=std::string_view::npos; inferenceVisible |= item.path=="Descriptive samples only" && item.detail.find("do not establish")!=std::string_view::npos;}}
            if(!spreadVisible || !inferenceVisible) return 1053;
            app.m_benchmarkShowing = false; app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected;
            std::filesystem::remove(temp / "nested" / "before.json"); std::filesystem::remove(temp / "nested" / "after.json");
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            { std::ofstream source(temp / "nested" / "coverage-source.txt"); source << "one\ntwo\n"; }
            { std::ofstream report(temp / "nested" / "coverage.info"); report << "SF:coverage-source.txt\nDA:1,1\nDA:2,0\nend_of_record\n"; }
            app.m_moduleRoot = temp / "coverage-modules"; std::string error;
            const std::string manifest = R"({"name":"Coverage","version":"1","permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Import","action":"coverage-lcov"}]})";
            if (!modules::install(app.m_moduleRoot, "coverage", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "coverage", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 412;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleCoveragePath) return 413;
            app.applyModuleCommand(); const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_coverageFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_coverageFuture.valid() || app.m_coverageReport.lines.size() != 2) return 414;
            if (app.m_coverageReport.lines[0].covered || app.m_coverageReport.lines[0].line != 1) return 415;
            auto summary = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -150);
            if (summary == app.m_moduleChoices.end() || app.m_suggestions[static_cast<std::size_t>(summary - app.m_moduleChoices.begin())].detail.find("1 covered · 1 uncovered · 50%") == std::string_view::npos) return 601;
            app.m_coverageReport.lines.push_back({"other-coverage.txt", 0, false});
            if (!modules::summarizeCoverage(app.m_coverageReport, app.m_coverageSummary, error)) return 1009;
            app.refreshSuggestions();
            auto browseFiles = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -154);
            if (browseFiles == app.m_moduleChoices.end()) return 1001;
            app.m_selectedSuggestion = static_cast<int>(browseFiles - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (!app.m_coverageFilesMode) return 1002;
            auto fileChoice = std::find_if(app.m_suggestions.begin(), app.m_suggestions.end(), [](const auto& item) { return item.path == "coverage-source.txt" && item.detail.find("1 covered · 1 uncovered · 50%") != std::string_view::npos; });
            if (fileChoice == app.m_suggestions.end()) return 1003;
            app.m_selectedSuggestion = static_cast<int>(fileChoice - app.m_suggestions.begin()); app.applyModuleCommand();
            if (app.m_coverageFilesMode || app.m_coverageFile != "coverage-source.txt" || std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 2) != app.m_moduleChoices.end()) return 1004;
            const auto clearFile = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -155);
            if (clearFile == app.m_moduleChoices.end()) return 1005;
            app.m_selectedSuggestion = static_cast<int>(clearFile - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (!app.m_coverageFile.empty()) return 1006;
            app.m_modulePage = 1; app.refreshSuggestions();
            if (std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 2) == app.m_moduleChoices.end()) return 1011;
            app.m_modulePage = 0;
            app.m_coverageReport.lines.pop_back();
            if (!modules::summarizeCoverage(app.m_coverageReport, app.m_coverageSummary, error)) return 1010;
            app.refreshSuggestions();
            app.m_coverageReport.incomplete = true; app.refreshSuggestions();
            summary = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -150);
            if (summary == app.m_moduleChoices.end() || app.m_suggestions[static_cast<std::size_t>(summary - app.m_moduleChoices.begin())].detail.find("incomplete report") == std::string_view::npos) return 606;
            app.m_coverageReport.incomplete = false; app.refreshSuggestions();
            auto filter = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -153);
            if (filter == app.m_moduleChoices.end()) return 602;
            app.m_selectedSuggestion = static_cast<int>(filter - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -154) == app.m_moduleChoices.end()) return 1012;
            if (std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 1) == app.m_moduleChoices.end()) { app.m_modulePage = 1; app.refreshSuggestions(); }
            if (app.m_coverageFilter != 2 || std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 0) != app.m_moduleChoices.end() || std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 1) == app.m_moduleChoices.end()) return 603;
            app.m_modulePage = 0; app.refreshSuggestions();
            filter = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -152);
            if (filter == app.m_moduleChoices.end()) return 604;
            app.m_selectedSuggestion = static_cast<int>(filter - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_coverageFilter != 1 || std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 1) != app.m_moduleChoices.end()) return 605;
            if (std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 0) == app.m_moduleChoices.end()) { app.m_modulePage = 1; app.refreshSuggestions(); }
            const auto choice = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 0);
            if (choice == app.m_moduleChoices.end()) return 416;
            app.m_selectedSuggestion = static_cast<int>(choice - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_filePath != "coverage-source.txt" || app.m_document.cursorLine() != 1) return 417;
            if (app.makeFrameState(1.0).coverageCursor != 0) return 418;
            app.m_document.insert(U'X');
            if (app.makeFrameState(1.1).coverageCursor != -1) return 419;
            app.m_document.undo();
            if (app.makeFrameState(1.2).coverageCursor != 0) return 420;
            { std::ofstream source(temp / "nested" / "coverage-source.txt", std::ios::trunc); source << "changed\nsource\n"; }
            app.openFile("coverage-source.txt", false);
            if (app.makeFrameState(1.3).coverageCursor != -1) return 421;
            { std::ofstream baseline(temp / "nested" / "coverage-before.info"); baseline << "SF:coverage-source.txt\nDA:1,0\nDA:2,1\nend_of_record\n"; }
            bool openedComparison = false;
            for (std::size_t page = 0; page < 8 && !openedComparison; ++page) {
                app.m_modulePage = page; app.refreshSuggestions(); const auto compare = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -213);
                if (compare != app.m_moduleChoices.end()) { app.m_selectedSuggestion = static_cast<int>(compare - app.m_moduleChoices.begin()); app.applyModuleCommand(); openedComparison = true; }
            }
            if (!openedComparison || app.m_searchBar.kind() != ui::PromptKind::ModuleCoveragePath) return 841;
            app.m_searchBar.setQuery("coverage-before.info"); app.applyModuleCommand();
            while (app.m_coverageFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_coverageFuture.valid() || !app.m_coverageComparing || app.m_coverageComparison.changes.size() != 2 || app.m_coverageBaselines.empty()) return 842;
            if (std::none_of(app.m_suggestions.begin(), app.m_suggestions.end(), [](const auto& item) { return item.path == "Approximate comparison" && item.detail.find("stale") != std::string::npos; })) return 843;
            bool sawRegression = false;
            for (std::size_t page = 0; page < 8; ++page) { app.m_modulePage = page; app.refreshSuggestions(); for (const auto& item : app.m_suggestions) if (item.path == "coverage-source.txt:2" && item.detail.find("Less covered") != std::string::npos) sawRegression = true; }
            if (!sawRegression) return 844;
            std::filesystem::remove(temp / "nested" / "coverage-before.info");
            app.openFile("example.txt", false); app.m_coverageShowing = false; app.m_modulePage = 0;
            app.m_searchBar.open(ui::PromptKind::ModuleActions); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (!app.m_coverageReport.lines.empty() || !app.m_coverageBaselines.empty()) return 422;
            app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected;
            std::filesystem::remove(temp / "nested" / "coverage-source.txt"); std::filesystem::remove(temp / "nested" / "coverage.info");
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            { std::ofstream source(temp / "nested" / "coverage-source.txt"); source << "one\ntwo\n"; }
            { std::ofstream report(temp / "nested" / "coverage.xml"); report << R"xml(<coverage><sources><source>.</source></sources><packages><package><classes><class filename="coverage-source.txt"><lines><line number="1" hits="1"/><line number="2" hits="0" branch="true" condition-coverage="50% (1/2)"/></lines></class></classes></package></packages></coverage>)xml"; }
            app.m_moduleRoot = temp / "cobertura-modules"; std::string error;
            const std::string manifest = R"({"name":"Coverage XML","version":"1","requires":["coverage-cobertura-v1","coverage-cobertura-branches-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Import","action":"coverage-cobertura"}]})";
            if (!modules::install(app.m_moduleRoot, "coverage-xml", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "coverage-xml", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 690;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleCoveragePath || app.m_searchBar.query() != "coverage.xml") return 691;
            app.applyModuleCommand(); const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_coverageFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_coverageFuture.valid() || app.m_coverageReport.incomplete || app.m_coverageReport.lines.size() != 2 || app.m_coverageReport.lines[0].covered) return 692;
            if(app.m_coverageReport.lines[0].branchesCovered!=1 || app.m_coverageReport.lines[0].branchesUncovered!=1 || app.m_coverageSummary.total.branchesCovered!=1 || app.m_coverageSummary.total.branchesUncovered!=1) return 1066;
            bool branchSummary=false;for(std::size_t page=0;page<4;++page){app.m_modulePage=page;app.refreshSuggestions();branchSummary |= std::any_of(app.m_suggestions.begin(),app.m_suggestions.end(),[](const auto& item){return item.path=="Imported branches" && item.detail.find("1 covered · 1 uncovered")!=std::string_view::npos;});}if(!branchSummary)return 1067;
            app.m_modulePage=0;app.refreshSuggestions();
            auto choice = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 0);
            if (choice == app.m_moduleChoices.end()) { app.m_modulePage = 1; app.refreshSuggestions(); choice = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 0); }
            if (choice == app.m_moduleChoices.end()) return 693;
            app.m_selectedSuggestion = static_cast<int>(choice - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_filePath != "coverage-source.txt" || app.m_document.cursorLine() != 1) return 694;
            app.m_coverageShowing = false; app.openFile("example.txt", false);
            {std::ofstream report(temp/"nested"/"coverage.xml",std::ios::trunc);report << R"xml(<coverage><class filename="coverage-source.txt"><lines><line number="2" hits="1" branch="true" condition-coverage="50% (1/2)"/><line number="2" hits="1" branch="true" condition-coverage="100% (2/2)"/></lines></class></coverage>)xml";}
            app.m_modulePage=0;app.m_searchBar.open(ui::PromptKind::ModuleCommands);app.refreshSuggestions();app.m_selectedSuggestion=0;app.applyModuleCommand();app.applyModuleCommand();
            const auto conflictDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);while(app.m_coverageFuture.valid() && std::chrono::steady_clock::now()<conflictDeadline){app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(app.m_coverageFuture.valid() || !app.m_coverageReport.incomplete || app.m_coverageReport.lines.size()!=1 || app.m_coverageReport.lines[0].branchesUnknown!=2) return 1068;
            bool incompleteSeen=false;for(std::size_t page=0;page<4;++page){app.m_modulePage=page;app.refreshSuggestions();incompleteSeen |= std::any_of(app.m_suggestions.begin(),app.m_suggestions.end(),[](const auto& item){return item.path=="Cobertura import incomplete" && item.detail.find("unavailable")!=std::string_view::npos;});}if(!incompleteSeen)return 1069;
            app.m_coverageShowing=false;app.m_searchBar.close(); app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected;
            std::filesystem::remove(temp / "nested" / "coverage-source.txt"); std::filesystem::remove(temp / "nested" / "coverage.xml");
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            { std::ofstream source(temp / "nested" / "branch-source.txt"); source << "one\ntwo\n"; }
            { std::ofstream report(temp / "nested" / "coverage.info"); report << "SF:branch-source.txt\nDA:2,1\nBRDA:2,0,0,1\nBRDA:2,0,1,0\nBRDA:2,0,2,-\nend_of_record\n"; }
            app.m_moduleRoot = temp / "branch-modules"; std::string error;
            const std::string manifest = R"({"name":"Branch coverage","version":"1","permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Import","action":"coverage-lcov"}]})";
            if (!modules::install(app.m_moduleRoot,"branches",manifest,error) || !modules::setEnabled(app.m_moduleRoot,"branches",true,error) || !modules::list(app.m_moduleRoot,app.m_modules,error)) return 695;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand(); app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now()+std::chrono::seconds(15);
            while (app.m_coverageFuture.valid() && std::chrono::steady_clock::now()<deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_coverageFuture.valid() || app.m_coverageReport.incomplete || app.m_coverageReport.lines.size()!=1) return 696;
            bool summarySeen=false, lineSeen=false, navigated=false;
            for (std::size_t page=0; page<4 && !navigated; ++page) { app.m_modulePage=page; app.refreshSuggestions();
                for (std::size_t i=0; i<app.m_suggestions.size(); ++i) {
                    const auto& item=app.m_suggestions[i];
                    summarySeen |= item.path=="Imported branches" && item.detail.find("1 covered · 1 uncovered · 1 no count")!=std::string_view::npos;
                    if (item.path=="branch-source.txt:2" && item.detail.find("1 covered, 1 uncovered, 1 no count")!=std::string_view::npos) { lineSeen=true; app.m_selectedSuggestion=static_cast<int>(i); app.applyModuleCommand(); navigated=true; break; }
                }
            }
            if (!summarySeen || !lineSeen || !navigated || app.m_filePath!="branch-source.txt" || app.m_document.cursorLine()!=1) return 697;
            app.m_coverageShowing=false; app.openFile("example.txt",false); app.m_searchBar.close(); app.m_moduleRoot=oldRoot; app.m_modules=std::move(oldModules); app.m_moduleSelected=oldSelected;
            std::filesystem::remove(temp / "nested" / "branch-source.txt"); std::filesystem::remove(temp / "nested" / "coverage.info");
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            std::filesystem::create_directories(temp / "nested" / "docs");
            { std::ofstream file(temp / "nested" / "docs" / "exists.txt"); file << "ok"; }
            { std::ofstream file(temp / "nested" / "docs" / "links.md"); file << "é [yes](exists.txt) [no](missing.txt) [web](https://example.com)\n"; }
            app.openFile("docs/links.md", false); app.m_moduleRoot = temp / "links-modules";
            const std::string manifest = R"({"name":"Links","version":"1","permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Check","action":"markdown-local-links"}]})";
            std::string error;
            if (!modules::install(app.m_moduleRoot, "links", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "links", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 410;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_markdownLinksFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_markdownLinksFuture.valid() || !app.m_markdownLinksShowing || app.m_markdownLinksResults.links.size() != 3 ||
                app.m_markdownLinksResults.links[0].status != modules::MarkdownLinkStatus::Found || app.m_markdownLinksResults.links[1].status != modules::MarkdownLinkStatus::Missing ||
                app.m_markdownLinksResults.links[2].status != modules::MarkdownLinkStatus::NotChecked) return 411;
            auto missingFilter=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-184); if (missingFilter==app.m_moduleChoices.end()) return 740;
            app.m_selectedSuggestion=static_cast<int>(missingFilter-app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_markdownLinksFilter!=1 || std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),0)!=app.m_moduleChoices.end()) return 741;
            auto link=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),1); if (link==app.m_moduleChoices.end() || !app.m_markdownLinksResults.links[1].sourceOffset) return 742;
            const auto offset=*app.m_markdownLinksResults.links[1].sourceOffset; const auto text=app.m_document.toUtf8(); std::size_t column=0; for (std::size_t i=0;i<offset;++i) if ((static_cast<unsigned char>(text[i]) & 0xc0U)!=0x80U) ++column;
            app.m_selectedSuggestion=static_cast<int>(link-app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_searchBar.isOpen() || app.m_document.cursorLine()!=0 || app.m_document.cursorColumn()!=column) return 743;
            const std::string externalManifest = R"({"name":"External","version":"1","requires":["markdown-external-links-v1"],"permissions":{"project":true,"network":true,"processes":false,"settings":false},"commands":[{"title":"External","action":"markdown-external-links"}]})";
            if (!modules::install(app.m_moduleRoot, "external", externalManifest, error) || !modules::setEnabled(app.m_moduleRoot, "external", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 744;
            const auto external=std::find_if(app.m_modules.begin(),app.m_modules.end(),[](const auto& module){return module.id=="external";}); if (external==app.m_modules.end()) return 745;
            app.m_moduleSelected=static_cast<std::size_t>(external-app.m_modules.begin()); app.m_modulePage=0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion=0; app.applyModuleCommand();
            while (app.m_markdownLinksFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_markdownLinksFuture.valid() || !app.m_markdownExternalMode || app.m_markdownExternalFuture.valid()) return 746;
            const auto web=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),2); if (web==app.m_moduleChoices.end()) return 747;
            app.m_selectedSuggestion=static_cast<int>(web-app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (!app.m_markdownExternalSelected || app.m_markdownExternalFuture.valid() || std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-252)==app.m_moduleChoices.end()) return 748;
            if (std::any_of(app.m_suggestionPaths.begin(),app.m_suggestionPaths.end(),[](const auto& text){return text.find("example.com")!=std::string::npos;})) return 749;
            app.m_markdownLinksShowing = false; app.m_searchBar.close(); app.openFile("example.txt", false);
            app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected;
            std::filesystem::remove_all(temp / "nested" / "docs");
        }
        {
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            app.m_moduleRoot = temp / "scan-modules";
            const std::string manifest = R"({"name":"Scanner","version":"1","permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Scan","action":"scan-secrets"}]})";
            std::string error;
            if (!modules::install(app.m_moduleRoot, "scanner", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "scanner", true, error) ||
                !modules::list(app.m_moduleRoot, app.m_modules, error) || !app.m_modules[0].enabled) return 353;
            { std::ofstream file(temp / "nested" / "scan-fixture.txt"); file << "prefix " << "ghp_" << std::string(36, 'A') << "\n"; }
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_moduleScanShowingResults = false;
            app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (!app.m_moduleScanFuture.valid() || !app.m_moduleScanShowingResults) return 354;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_moduleScanFuture.valid() && std::chrono::steady_clock::now() < deadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_moduleScanFuture.valid() || app.m_moduleScanResults.incomplete || app.m_moduleScanResults.findings.size() != 1 ||
                app.m_moduleScanResults.findings[0].path != "scan-fixture.txt") return 355;
            auto githubFilter=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-191); if (githubFilter==app.m_moduleChoices.end()) return 760;
            app.m_selectedSuggestion=static_cast<int>(githubFilter-app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_moduleScanFilter!=2 || std::any_of(app.m_suggestionPaths.begin(),app.m_suggestionPaths.end(),[](const auto& text) { return text.find("ghp_")!=std::string::npos; })) return 761;
            { std::ofstream file(temp / "nested" / "scan-fixture.txt",std::ios::trunc); file << "changed\n"; }
            const auto finding = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 0);
            if (finding == app.m_moduleChoices.end()) return 356;
            app.m_selectedSuggestion = static_cast<int>(finding - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (!app.m_moduleScanShowingResults || !app.m_commandStatusIsError || app.m_filePath=="scan-fixture.txt") return 762;
            { std::ofstream file(temp / "nested" / "scan-fixture.txt",std::ios::trunc); file << "prefix " << "ghp_" << std::string(36,'A') << "\n"; }
            const auto currentFinding=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),0); if (currentFinding==app.m_moduleChoices.end()) return 763;
            app.m_selectedSuggestion=static_cast<int>(currentFinding-app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_filePath != "scan-fixture.txt" || app.m_document.cursorLine() != 0 || app.m_document.cursorColumn() != 7) return 357;
            app.openFile("example.txt", false); std::filesystem::remove(temp / "nested" / "scan-fixture.txt");
            app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected; app.m_moduleScanShowingResults = false;
        }
        {
            const auto oldSettings = app.m_settingsRoot; const auto oldRecovery = app.m_recoveryFile;
            app.m_settingsRoot = (temp / "conflict-settings" / "Settings").string(); app.m_recoveryFile.clear();
            { std::ofstream file(temp / "nested" / "conflict.txt"); file << "external version"; }
            auto unsaved = editor::Document::fromUtf8("original"); unsaved.insert(U'X');
            app.m_inactiveDocuments.push_back({"conflict.txt", std::move(unsaved), 0, "original"});
            app.openFile("conflict.txt", false);
            if (app.m_filePath != "conflict.txt" || app.m_document.toUtf8() != "Xoriginal" || !app.m_document.dirty() ||
                app.m_diskContents != "original" || !app.m_statusIsError) return 329;
            if (app.saveDocument() || app.m_document.toUtf8() != "Xoriginal") return 330;
            std::string disk, readError;
            if (!app.m_project.readFile("conflict.txt", disk, readError) || disk != "external version") return 331;
            workspace::recovery::Snapshot retained;
            if (!workspace::recovery::read(app.m_recoveryFile, retained, readError) || retained.contents != "Xoriginal" || retained.baseline != "original") return 332;
            app.m_document.undo(); app.openFile("example.txt", false);
            if (app.m_filePath != "example.txt") return 333;
            for (const bool binaryReplacement : {false, true}) {
                const std::string name = binaryReplacement ? "binary-conflict.txt" : "missing-conflict.txt";
                if (binaryReplacement) { std::ofstream file(temp / "nested" / name, std::ios::binary); file.write("\0bad", 4); }
                auto dirty = editor::Document::fromUtf8("original"); dirty.insert(U'X');
                app.m_inactiveDocuments.push_back({name, std::move(dirty), 0, "original"});
                app.openFile(name, false);
                if (app.m_filePath != name || app.m_document.toUtf8() != "Xoriginal" || !app.m_document.dirty() ||
                    app.m_diskContents != "original" || !app.m_statusIsError) return 334;
                if (app.saveDocument()) return 335;
                if (!workspace::recovery::read(app.m_recoveryFile, retained, readError) || retained.contents != "Xoriginal" || retained.file != name) return 336;
                if (!binaryReplacement && std::filesystem::exists(temp / "nested" / name)) return 337;
                if (binaryReplacement) { std::string bytes; if (!app.m_project.readFile(name, bytes, readError) || bytes != std::string("\0bad", 4)) return 338; }
                app.m_document.undo(); app.openFile("example.txt", false);
                if (app.m_filePath != "example.txt") return 339;
                std::filesystem::remove(temp / "nested" / name);
            }
            app.m_settingsRoot = oldSettings; app.m_recoveryFile = oldRecovery;
            std::filesystem::remove(temp / "nested" / "conflict.txt");
        }
        {
            workspace::recovery::Snapshot snapshot; snapshot.project = app.m_project.root().generic_string();
            snapshot.file = "recovery-collision.txt"; snapshot.baseline = "base"; snapshot.contents = "older edit";
            const auto snapshotPath = temp / "collision-snapshot.json"; std::string recoveryError;
            if (!workspace::recovery::write(snapshotPath, snapshot, recoveryError)) return 340;
            { std::ofstream file(temp / "nested" / snapshot.file); file << "base"; }
            auto dirty = editor::Document::fromUtf8("base"); dirty.insert(U'X');
            app.m_inactiveDocuments.push_back({snapshot.file, std::move(dirty), 0, "base"});
            app.m_recoveryFiles = {snapshotPath}; app.m_recoverySelected = 0; app.m_recoveryPreview = snapshot;
            app.m_recoveryPage = 0; app.m_searchBar.open(ui::PromptKind::RecoveryDetail); app.refreshSuggestions(); app.m_selectedSuggestion = 0;
            app.applyRecovery();
            if (!app.m_commandStatusIsError || app.m_commandStatus.find("unsaved edits") == std::string::npos ||
                app.m_inactiveDocuments.back().document.toUtf8() != "Xbase") return 341;
            app.m_inactiveDocuments.pop_back(); std::filesystem::remove(temp / "nested" / snapshot.file);
            std::filesystem::create_directory(temp / "nested" / snapshot.file);
            app.clearCommandFeedback(); app.m_selectedSuggestion = 0; app.applyRecovery();
            if (!app.m_commandStatusIsError || app.m_commandStatus.find("folder") == std::string::npos) return 342;
            app.refreshSuggestions(); const auto copy = std::find(app.m_recoveryChoices.begin(), app.m_recoveryChoices.end(), 3);
            if (copy == app.m_recoveryChoices.end()) return 343;
            app.m_selectedSuggestion = static_cast<int>(copy - app.m_recoveryChoices.begin()); app.applyRecovery();
            if (app.m_document.toUtf8() != "older edit" || app.m_filePath.find("recovery-collision.recovered-") != 0 ||
                !std::filesystem::is_directory(temp / "nested" / snapshot.file)) return 344;
            const auto copyPath = temp / "nested" / app.m_filePath;
            app.openFile("example.txt", false); std::filesystem::remove(copyPath);
            snapshot.file = "recovery-binary.txt"; snapshot.contents = "restored edit"; snapshot.baseline = "original";
            { std::ofstream file(temp / "nested" / snapshot.file, std::ios::binary); file.write("\0bad", 4); }
            if (!workspace::recovery::write(snapshotPath, snapshot, recoveryError)) return 345;
            const auto savedSettings = app.m_settingsRoot; const auto savedRecovery = app.m_recoveryFile;
            app.m_settingsRoot = (temp / "binary-recovery-settings" / "Settings").string(); app.m_recoveryFile.clear();
            app.m_recoveryFiles = {snapshotPath}; app.m_recoverySelected = 0; app.m_recoveryPreview = snapshot;
            app.m_recoveryPage = 0; app.m_searchBar.open(ui::PromptKind::RecoveryDetail); app.refreshSuggestions(); app.m_selectedSuggestion = 0;
            app.applyRecovery();
            if (app.m_filePath != snapshot.file || app.m_document.toUtf8() != "restored edit" || !app.m_document.dirty() || !app.m_statusIsError) return 346;
            if (app.saveDocument()) return 347;
            std::string binaryDisk;
            if (!app.m_project.readFile(snapshot.file, binaryDisk, recoveryError) || binaryDisk != std::string("\0bad", 4)) return 348;
            workspace::recovery::Snapshot protectedCopy;
            if (!workspace::recovery::read(app.m_recoveryFile, protectedCopy, recoveryError) || protectedCopy.contents != "restored edit") return 349;
            app.m_document = editor::Document::fromUtf8("original"); app.openFile("example.txt", false);
            app.m_settingsRoot = savedSettings; app.m_recoveryFile = savedRecovery;
            std::filesystem::remove(temp / "nested" / snapshot.file);
            std::filesystem::remove_all(temp / "nested" / "recovery-collision.txt"); std::filesystem::remove(snapshotPath);
            app.m_recoveryFiles.clear(); app.m_recoverySummaries.clear();
        }
#if !defined(_WIN32)
        if (std::getenv("LUCID_TEST_CMAKE")) {
            { std::ofstream cmake(temp / "nested" / "CMakeLists.txt"); cmake << "cmake_minimum_required(VERSION 3.16)\nproject(LucidTest LANGUAGES CXX)\noption(LUCID_FEATURE \"Example feature\" ON)\nadd_executable(check sample.cpp)\n"; }
            { std::ofstream source(temp / "nested" / "sample.cpp"); source << "int main() { return 0; }\n"; }
            std::filesystem::create_directory(temp / "nested" / "build");
            { std::ofstream sentinel(temp / "nested" / "build" / "sentinel"); sentinel << "keep"; }
            app.openLanguageStatus();
            auto generate = std::find(app.m_languageStatusChoices.begin(), app.m_languageStatusChoices.end(), -4);
            if (generate == app.m_languageStatusChoices.end()) return 299;
            const int generateIndex = static_cast<int>(generate - app.m_languageStatusChoices.begin());
            app.m_selectedSuggestion = generateIndex; app.applyLanguageStatus();
            if (!app.m_commandStatusIsError || !std::filesystem::exists(temp / "nested" / "build" / "sentinel")) return 300;
            std::filesystem::remove_all(temp / "nested" / "build");
            app.m_selectedSuggestion = generateIndex; app.applyLanguageStatus();
            const auto cmakeDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
            while (!app.m_compileTerminalProcess.commandsDone() && app.m_compileTerminalProcess.alive() && std::chrono::steady_clock::now() < cmakeDeadline) {
                app.m_compileTerminalProcess.update(); std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            if (!app.m_compileTerminalProcess.commandsDone() || app.m_compileTerminalProcess.exitCodes().empty() || app.m_compileTerminalProcess.exitCodes()[0] != 0 ||
                !std::filesystem::exists(temp / "nested" / "build" / "compile_commands.json")) { std::cerr << "cmake: " << app.m_compileTerminalProcess.output() << "\n"; return 301; }
            app.closeCompileTerminal();
            const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
            app.m_moduleRoot = temp / "cmake-modules";
            const std::string manifest = R"({"name":"CMake","version":"1","requires":["cmake-cache-edit-v1","cmake-cache-values-v1","cmake-codemodel-v1","cmake-build-target-v1"],"permissions":{"project":true,"network":false,"processes":true,"settings":false},"commands":[{"title":"Browse","action":"browse-cmake"},{"title":"Prepare","action":"prepare-cmake-query"},{"title":"Edit","action":"edit-cmake-options"}]})";
            std::string moduleError;
            if (!modules::install(app.m_moduleRoot, "cmake", manifest, moduleError) || !modules::setEnabled(app.m_moduleRoot, "cmake", true, moduleError) || !modules::list(app.m_moduleRoot, app.m_modules, moduleError)) return 386;
            app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_moduleCMakeShowing = false;
            app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 1; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleCMakePath) return 392;
            app.m_searchBar.setQuery("../outside"); app.applyModuleCommand();
            if (!app.m_commandStatusIsError || app.m_searchBar.kind() != ui::PromptKind::ModuleCMakePath) return 393;
            app.m_searchBar.setQuery("build"); app.applyModuleCommand();
            if (!std::filesystem::is_regular_file(temp / "nested" / "build" / ".cmake/api/v1/query/codemodel-v2")) return 394;
            if (!std::filesystem::is_regular_file(temp / "nested" / "build" / ".cmake/api/v1/query/cache-v2")) return 397;
            { std::ofstream query(temp / "nested" / "build" / ".cmake/api/v1/query/codemodel-v2"); query << "preserve"; }
            app.m_selectedSuggestion = 1; app.applyModuleCommand(); app.m_searchBar.setQuery("build"); app.applyModuleCommand();
            { std::ifstream query(temp / "nested" / "build" / ".cmake/api/v1/query/codemodel-v2"); std::string text((std::istreambuf_iterator<char>(query)), {}); if (text != "preserve") return 396; }
            const auto cmakeCommand = "cmake -S '" + (temp / "nested").string() + "' -B '" + (temp / "nested" / "build").string() + "' >/dev/null 2>&1";
            if (std::system(cmakeCommand.c_str()) != 0) return 385;
            app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
            if (app.m_searchBar.kind() != ui::PromptKind::ModuleCMakePath || app.m_searchBar.query() != "build") return 395;
            app.applyModuleCommand();
            const auto metadataDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_moduleCMakeFuture.valid() && std::chrono::steady_clock::now() < metadataDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_moduleCMakeFuture.valid() || app.m_moduleCMakeModel.targets.size() != 1 || app.m_moduleCMakeModel.targets[0].name != "check") return 387;
            const auto optionsChoice = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -11);
            if (optionsChoice == app.m_moduleChoices.end()) return 398;
            app.m_selectedSuggestion = static_cast<int>(optionsChoice - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (!app.m_moduleCMakeOptions || std::find(app.m_suggestionPaths.begin(), app.m_suggestionPaths.end(), "LUCID_FEATURE = ON") == app.m_suggestionPaths.end()) return 399;
            const auto back = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), -9);
            if (back == app.m_moduleChoices.end()) return 400;
            app.m_selectedSuggestion = static_cast<int>(back - app.m_moduleChoices.begin()); app.applyModuleCommand();
            auto target = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), 0);
            if (target == app.m_moduleChoices.end()) return 388;
            app.m_selectedSuggestion = static_cast<int>(target - app.m_moduleChoices.begin()); app.applyModuleCommand();
#if !defined(_WIN32)
            const auto fakeBuild=temp/"fake-cmake-build";{std::ofstream out(fakeBuild);out << "#!/bin/sh\nprintf ran > \"$0.ran\"\nexit 0\n";}std::filesystem::permissions(fakeBuild,std::filesystem::perms::owner_all);
            auto buildChoice=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-400);if(buildChoice==app.m_moduleChoices.end())return 896;app.m_selectedSuggestion=static_cast<int>(buildChoice-app.m_moduleChoices.begin());app.applyModuleCommand();
            if(app.m_cmakeBuildStage!=1 || app.m_cmakeBuildFuture.valid())return 897;
            app.m_searchBar.setQuery(fakeBuild.string());app.applyModuleCommand();
            const auto buildDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);while(app.m_cmakeBuildPrepareFuture.valid() && std::chrono::steady_clock::now()<buildDeadline){app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(app.m_cmakeBuildPrepareFuture.valid() || app.m_cmakeBuildStage!=3 || std::filesystem::exists(fakeBuild.string()+".ran"))return 898;
            bool confirmedBuild=false;for(std::size_t page=0;page<10 && !confirmedBuild;++page){app.m_modulePage=page;app.refreshSuggestions();auto row=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-401);if(row!=app.m_moduleChoices.end()){app.m_selectedSuggestion=static_cast<int>(row-app.m_moduleChoices.begin());app.applyModuleCommand();confirmedBuild=true;}}
            if(!confirmedBuild || !app.m_cmakeBuildFuture.valid())return 899;
            while(app.m_cmakeBuildFuture.valid() && std::chrono::steady_clock::now()<buildDeadline){app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(app.m_cmakeBuildFuture.valid() || app.m_cmakeBuildStage!=5 || !std::filesystem::exists(fakeBuild.string()+".ran"))return 900;
            app.m_modulePage=0;app.refreshSuggestions();auto backBuild=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-9);if(backBuild==app.m_moduleChoices.end())return 901;app.m_selectedSuggestion=static_cast<int>(backBuild-app.m_moduleChoices.begin());app.applyModuleCommand();if(app.m_cmakeBuildStage || !app.m_moduleCMakeTarget)return 902;
            std::filesystem::remove(fakeBuild.string()+".ran");std::filesystem::remove(fakeBuild);
#endif
            const auto source = std::find_if(app.m_moduleCMakeModel.targets[0].sources.begin(), app.m_moduleCMakeModel.targets[0].sources.end(), [](const auto& s) { return s.path == "sample.cpp"; });
            if (source == app.m_moduleCMakeModel.targets[0].sources.end()) return 389;
            const auto sourceChoice = std::find(app.m_moduleChoices.begin(), app.m_moduleChoices.end(), static_cast<int>(source - app.m_moduleCMakeModel.targets[0].sources.begin()));
            if (sourceChoice == app.m_moduleChoices.end()) return 390;
            app.m_selectedSuggestion = static_cast<int>(sourceChoice - app.m_moduleChoices.begin()); app.applyModuleCommand();
            if (app.m_filePath != "sample.cpp") return 391;
            app.openFile("example.txt", false); app.m_moduleCMakeShowing = false; app.m_modulePage=0; app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion=2; app.applyModuleCommand();
            if (app.m_searchBar.kind()!=ui::PromptKind::ModuleCMakePath || !app.m_moduleCMakeEditing) return 750;
            app.m_searchBar.setQuery("build"); app.applyModuleCommand();
            while (app.m_moduleCMakeFuture.valid() && std::chrono::steady_clock::now()<metadataDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            auto optionRow=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-11); if (optionRow==app.m_moduleChoices.end()) return 751;
            app.m_selectedSuggestion=static_cast<int>(optionRow-app.m_moduleChoices.begin()); app.applyModuleCommand();
            {std::ofstream cache(temp/"nested"/"build"/"CMakeCache.txt",std::ios::app);cache << "\nLUCID_MODE:STRING=Debug\nLUCID_MODE-STRINGS:INTERNAL=Debug;Release\n";}
            auto typed=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-320);if(typed==app.m_moduleChoices.end()) return 890;app.m_selectedSuggestion=static_cast<int>(typed-app.m_moduleChoices.begin());app.applyModuleCommand();
            const auto typedDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(app.m_cmakeValuesFuture.valid() && std::chrono::steady_clock::now()<typedDeadline){app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            const auto mode=std::find_if(app.m_cmakeValues.begin(),app.m_cmakeValues.end(),[](const auto& v){return v.name=="LUCID_MODE";});if(app.m_cmakeValuesFuture.valid() || mode==app.m_cmakeValues.end()) return 891;
            bool picked=false;const auto modeIndex=static_cast<int>(mode-app.m_cmakeValues.begin());for(std::size_t page=0;page<100 && !picked;++page){app.m_modulePage=page;app.refreshSuggestions();auto row=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),modeIndex);if(row!=app.m_moduleChoices.end()){app.m_selectedSuggestion=static_cast<int>(row-app.m_moduleChoices.begin());app.applyModuleCommand();picked=true;}} if(!picked || !app.m_cmakeValueSelected) return 892;
            auto edit=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-321);if(edit==app.m_moduleChoices.end())return 893;app.m_selectedSuggestion=static_cast<int>(edit-app.m_moduleChoices.begin());app.applyModuleCommand();app.m_searchBar.setQuery("Release");app.applyModuleCommand();
            while(app.m_cmakeCachePrepareFuture.valid() && std::chrono::steady_clock::now()<typedDeadline){app.pollModules();std::this_thread::sleep_for(std::chrono::milliseconds(10));}if(app.m_cmakeCacheUpdate.path.empty() || app.m_cmakeCacheTarget!="Release")return 894;
            app.m_selectedSuggestion=0;app.applyModuleCommand();app.m_selectedSuggestion=0;app.applyModuleCommand(); if(app.m_cmakeValueMode || app.m_cmakeValueSelected) return 895;
            const auto feature=std::find_if(app.m_moduleCMakeModel.options.begin(),app.m_moduleCMakeModel.options.end(),[](const auto& o) { return o.name=="LUCID_FEATURE"; }); if (feature==app.m_moduleCMakeModel.options.end()) return 752;
            const int featureIndex=static_cast<int>(feature-app.m_moduleCMakeModel.options.begin()); bool foundFeature=false;
            for (std::size_t page=0;page<100 && !foundFeature;++page) { app.m_modulePage=page; app.refreshSuggestions(); auto row=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),featureIndex); if (row!=app.m_moduleChoices.end()) { app.m_selectedSuggestion=static_cast<int>(row-app.m_moduleChoices.begin()); app.applyModuleCommand(); foundFeature=true; } }
            if (!foundFeature || !app.m_moduleCMakeOption) return 753;
            auto off=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-187); if (off==app.m_moduleChoices.end()) return 754;
            app.m_selectedSuggestion=static_cast<int>(off-app.m_moduleChoices.begin()); app.applyModuleCommand();
            while (app.m_cmakeCachePrepareFuture.valid() && std::chrono::steady_clock::now()<metadataDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            auto confirm=std::find(app.m_moduleChoices.begin(),app.m_moduleChoices.end(),-188); std::ifstream before(temp/"nested"/"build"/"CMakeCache.txt"); std::string beforeText((std::istreambuf_iterator<char>(before)),{});
            if (confirm==app.m_moduleChoices.end() || beforeText.find("LUCID_FEATURE:BOOL=ON")==std::string::npos) return 755;
            app.m_selectedSuggestion=static_cast<int>(confirm-app.m_moduleChoices.begin()); app.applyModuleCommand();
            while (app.m_cmakeCacheSaveFuture.valid() && std::chrono::steady_clock::now()<metadataDeadline) { app.pollModules(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            std::ifstream after(temp/"nested"/"build"/"CMakeCache.txt"); std::string afterText((std::istreambuf_iterator<char>(after)),{});
            if (app.m_cmakeCacheSaveFuture.valid() || !app.m_moduleCMakeStale || app.m_commandStatusIsError || afterText.find("LUCID_FEATURE:BOOL=OFF")==std::string::npos) return 756;
            app.m_searchBar.close(); app.m_moduleCMakeShowing = false; app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected;
            std::filesystem::remove_all(temp / "nested" / "build");
            std::filesystem::remove(temp / "nested" / "CMakeLists.txt"); std::filesystem::remove(temp / "nested" / "sample.cpp");
        }
#endif
        {
            input::KeyEvent status{}; status.key = input::Key::I; status.shift = true; status.action = input::KeyAction::Press;
#if defined(__APPLE__)
            status.super = true;
#else
            status.control = true;
#endif
            app.handleKey(status);
            if (app.m_searchBar.kind() != ui::PromptKind::LanguageStatus || app.m_languageStatusChoices.empty() || app.m_languageStatusChoices[0] != -3) return 290;
            if (std::getenv("LUCID_TEST_CLANGD")) {
                app.m_selectedSuggestion = 0; app.applyLanguageStatus();
                const auto statusDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
                while (!app.m_languageServer.ready() && app.m_languageServer.alive() && std::chrono::steady_clock::now() < statusDeadline) {
                    app.pollLanguageServer(); std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                if (!app.m_languageServer.ready() || app.m_commandStatus != "clangd is ready") return 291;
                const auto completion = std::find(app.m_suggestionPaths.begin(), app.m_suggestionPaths.end(), "Completion");
                if (completion == app.m_suggestionPaths.end() || app.m_suggestionDetails[static_cast<std::size_t>(completion - app.m_suggestionPaths.begin())] != "Supported") return 292;
                app.m_languageServer.close(); app.m_languageRoot.clear(); app.m_languageFile.clear();
            }
            app.m_searchBar.close();
        }
        if (std::getenv("LUCID_TEST_CLANGD")) {
            { std::ofstream source(temp / "nested" / "signature.cpp"); source << "int add(int first, int second);\nint main() { return add(1, ); }\nnamespace math { int nested(); }\n"; }
            app.openFile("signature.cpp", false); const auto line = app.m_document.lineUtf8(1);
            app.m_document.setCursor(1, line.find(',') + 2);
            input::KeyEvent signature{}; signature.key = input::Key::S; signature.alt = true; signature.shift = true; signature.action = input::KeyAction::Press;
            app.handleEditorKey(signature, false);
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while ((!app.m_searchBar.isOpen() || app.m_searchBar.kind() != ui::PromptKind::LanguageSignatures) && std::chrono::steady_clock::now() < deadline) {
                app.pollLanguageServer(); std::this_thread::sleep_for(std::chrono::milliseconds(10));
                if (!app.m_languageServer.alive()) break;
            }
            if (!app.m_searchBar.isOpen() || app.m_searchBar.kind() != ui::PromptKind::LanguageSignatures || app.m_languageSignatures.empty() ||
                app.m_languageSignatures[0].first.find("add") == std::string::npos || app.m_languageSignatures[0].second.find("Parameter 2") == std::string::npos ||
                app.m_languageSignatures[0].second.find("second") == std::string::npos) {
                std::cerr << "signature: " << app.m_statusText << "\n"; return 293;
            }
            app.m_searchBar.close();
            input::KeyEvent symbols{}; symbols.key = input::Key::O; symbols.alt = true; symbols.shift = true; symbols.action = input::KeyAction::Press;
            app.handleEditorKey(symbols, false);
            if (app.m_searchBar.kind() != ui::PromptKind::LanguageSymbolInput) return 294;
            app.m_searchBar.setQuery("add"); app.applyLanguageSymbols();
            const auto symbolsDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while ((!app.m_searchBar.isOpen() || app.m_searchBar.kind() != ui::PromptKind::LanguageLocations) && std::chrono::steady_clock::now() < symbolsDeadline) {
                app.pollLanguageServer(); std::this_thread::sleep_for(std::chrono::milliseconds(10));
                if (!app.m_languageServer.alive() || (!app.m_languageNavigationQueued && !app.m_languageNavigationRequest)) break;
            }
            if (!app.m_searchBar.isOpen() || app.m_searchBar.kind() != ui::PromptKind::LanguageLocations || app.m_languageLocations.empty() ||
                app.m_languageLocations[0].name.find("add") == std::string::npos) { std::cerr << "symbols: " << app.m_statusText << "\n"; return 295; }
            app.m_selectedSuggestion = 0; app.applyLanguageLocation();
            if (app.m_searchBar.isOpen() || app.m_filePath != "signature.cpp" || app.m_document.cursorLine() != 0) return 296;
            input::KeyEvent fileSymbols{}; fileSymbols.key = input::Key::F4; fileSymbols.action = input::KeyAction::Press;
            app.handleEditorKey(fileSymbols, false);
            if (app.m_searchBar.kind() != ui::PromptKind::LanguageSymbolInput || !app.m_languageDocumentSymbols) return 297;
            app.m_searchBar.setQuery("NESTED"); app.applyLanguageSymbols();
            const auto fileSymbolsDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while ((!app.m_searchBar.isOpen() || app.m_searchBar.kind() != ui::PromptKind::LanguageLocations) && std::chrono::steady_clock::now() < fileSymbolsDeadline) {
                app.pollLanguageServer(); std::this_thread::sleep_for(std::chrono::milliseconds(10));
                if (!app.m_languageServer.alive() || (!app.m_languageNavigationQueued && !app.m_languageNavigationRequest)) break;
            }
            if (!app.m_searchBar.isOpen() || app.m_searchBar.kind() != ui::PromptKind::LanguageLocations || app.m_languageLocations.size() != 1 ||
                app.m_languageLocations[0].name.find("nested") == std::string::npos || app.m_languageLocations[0].line != 2) { std::cerr << "file symbols: " << app.m_statusText << "\n"; return 298; }
            app.m_searchBar.close(); app.m_languageServer.close(); app.m_languageRoot.clear(); app.m_languageFile.clear();
            app.openFile("example.txt", false); std::filesystem::remove(temp / "nested" / "signature.cpp");
        }
        if (std::getenv("LUCID_TEST_CLANGD")) {
            { std::ofstream header(temp / "nested" / "rename.hpp"); header << "inline int oldName = 1;\n"; }
            { std::ofstream source(temp / "nested" / "rename.cpp"); source << "#include \"rename.hpp\"\nint main() { return oldName; }\n"; }
            { const auto root = app.m_project.root(); const auto source = root / "rename.cpp";
              std::ofstream commands(temp / "nested" / "compile_commands.json");
              commands << "[{\"directory\":" << debug::jsonString(root.string()) << ",\"file\":" << debug::jsonString(source.string())
                       << ",\"arguments\":[\"clang++\",\"-std=c++17\",\"-c\"," << debug::jsonString(source.string()) << "]}]"; }
            app.openFile("rename.cpp", false); app.m_document.setCursor(1, 22);
            input::KeyEvent rename{}; rename.key = input::Key::F2; rename.action = input::KeyAction::Press;
            app.handleEditorKey(rename, false);
            if (app.m_searchBar.kind() != ui::PromptKind::LanguageRenameInput) return 272;
            app.m_searchBar.setQuery("newName"); app.applyLanguageRename();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
            while ((!app.m_searchBar.isOpen() || app.m_searchBar.kind() != ui::PromptKind::LanguageRenameReview) && std::chrono::steady_clock::now() < deadline) {
                app.pollLanguageServer(); std::this_thread::sleep_for(std::chrono::milliseconds(10));
                if (!app.m_languageServer.alive()) break;
            }
            if (!app.m_searchBar.isOpen() || app.m_searchBar.kind() != ui::PromptKind::LanguageRenameReview || app.m_languageRenameEdits.size() != 2) { std::cerr << "rename: " << app.m_statusText << " files=" << app.m_languageRenameEdits.size() << "\n"; return 273; }
            for (const auto& file : app.m_languageRenameEdits) {
                if (file.changes.size() != 1 || file.changes[0].before != "oldName" || file.changes[0].after != "newName") return 284;
            }
            std::string original, renameError;
            if (!app.m_project.readFile("rename.cpp", original, renameError) || original.find("oldName") == std::string::npos) return 274;
            app.refreshSuggestions(); const auto fileChoice = std::find(app.m_languageRenameChoices.begin(), app.m_languageRenameChoices.end(), 0);
            if (fileChoice == app.m_languageRenameChoices.end()) return 280;
            app.m_selectedSuggestion = static_cast<int>(fileChoice - app.m_languageRenameChoices.begin()); app.applyLanguageRename();
            if (!app.m_languageRenameReviewFile || app.m_languageRenameEdits[*app.m_languageRenameReviewFile].changes.empty()) return 281;
            bool foundChange = false;
            for (const auto& change : app.m_languageRenameEdits[*app.m_languageRenameReviewFile].changes) {
                if (change.before == "oldName" && change.after == "newName") foundChange = true;
            }
            if (!foundChange) return 282;
            input::KeyEvent back{}; back.key = input::Key::Escape; back.action = input::KeyAction::Press; app.handleCommandKey(back, false);
            if (app.m_languageRenameReviewFile || !app.m_searchBar.isOpen()) return 283;
            app.refreshSuggestions(); const auto apply = std::find(app.m_languageRenameChoices.begin(), app.m_languageRenameChoices.end(), -3);
            if (apply == app.m_languageRenameChoices.end()) return 275;
            const int applyIndex = static_cast<int>(apply - app.m_languageRenameChoices.begin());
            app.m_selectedSuggestion = -1; app.applyLanguageRename();
            if (!app.m_searchBar.isOpen() || !app.m_project.readFile("rename.cpp", original, renameError) || original.find("oldName") == std::string::npos) return 278;
            auto dirtyHeader = editor::Document::fromUtf8("inline int oldName = 1;\n"); dirtyHeader.insert(U'X');
            app.m_inactiveDocuments.push_back({"rename.hpp", std::move(dirtyHeader), 0, "inline int oldName = 1;\n"});
            app.m_selectedSuggestion = applyIndex; app.applyLanguageRename();
            if (!app.m_searchBar.isOpen() || !app.m_commandStatusIsError || app.m_commandStatus.find("unsaved") == std::string::npos ||
                !app.m_project.readFile("rename.hpp", original, renameError) || original.find("oldName") == std::string::npos) return 285;
            app.m_inactiveDocuments.pop_back();
            const auto reviewedRename = app.m_languageRenameEdits;
            { std::ofstream external(temp / "nested" / "rename.hpp", std::ios::trunc); external << "external edit\n"; }
            app.m_selectedSuggestion = applyIndex; app.applyLanguageRename();
            if (!app.m_statusIsError || app.m_statusText.find("changed since review") == std::string::npos ||
                !app.m_project.readFile("rename.cpp", original, renameError) || original.find("oldName") == std::string::npos) return 286;
            { std::ofstream restore(temp / "nested" / "rename.hpp", std::ios::trunc); restore << "inline int oldName = 1;\n"; }
            app.m_languageRenameEdits = reviewedRename; app.m_languageRenameReviewFile.reset(); app.m_languageRenamePage = 0;
            app.m_searchBar.open(ui::PromptKind::LanguageRenameReview); app.refreshSuggestions();
            app.m_document.insert(U'X'); app.m_selectedSuggestion = applyIndex; app.applyLanguageRename();
            if (!app.m_commandStatusIsError || !app.m_searchBar.isOpen()) return 279;
            app.m_document.undo();
            // Revision changed even after undo, so a fresh request is required.
            app.m_languageRenameRevision = app.m_document.revision(); app.m_selectedSuggestion = applyIndex; app.applyLanguageRename();
            if (app.m_statusIsError || !app.m_project.readFile("rename.cpp", original, renameError) || original.find("oldName") != std::string::npos || original.find("newName") == std::string::npos) return 276;
            if (!app.m_project.readFile("rename.hpp", original, renameError) || original.find("oldName") != std::string::npos || original.find("newName") == std::string::npos) return 277;
            app.m_languageServer.close(); app.m_languageRoot.clear(); app.m_languageFile.clear();
            app.openFile("example.txt", false); std::filesystem::remove(temp / "nested" / "rename.cpp"); std::filesystem::remove(temp / "nested" / "rename.hpp"); std::filesystem::remove(temp / "nested" / "compile_commands.json");
        }
        {
            app.m_languageLocations = {{"missing-location.cpp", 0, 0}}; app.m_languageLocationPage = 0;
            app.m_searchBar.open(ui::PromptKind::LanguageLocations); app.refreshSuggestions(); app.m_selectedSuggestion = 0;
            app.applyLanguageLocation();
            if (std::filesystem::exists(temp / "nested" / "missing-location.cpp") || !app.m_statusIsError || app.m_filePath != "example.txt") return 269;
            app.m_searchBar.close();
            { std::ofstream source(temp / "nested" / "location.cpp"); source << "a😀b"; }
            app.m_languageLocations = {{"location.cpp", 0, 2}}; app.m_searchBar.open(ui::PromptKind::LanguageLocations);
            app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyLanguageLocation();
            if (!app.m_statusIsError || app.m_statusText.find("invalid character") == std::string::npos) return 270;
            app.m_languageLocations = {{"location.cpp", 0, 3}}; app.m_searchBar.open(ui::PromptKind::LanguageLocations);
            app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyLanguageLocation();
            if (app.m_document.cursorColumn() != 2 || app.m_statusIsError) return 271;
            app.openFile("example.txt", false); std::filesystem::remove(temp / "nested" / "location.cpp");
            app.m_languageLocations.clear();
        }
        {
            app.m_project.refresh(); app.m_projectSearchResults = workspace::searchProject(app.m_project, "opened");
            app.m_projectSearchRoot = app.m_project.root().generic_string();
            app.m_searchBar.open(ui::PromptKind::ProjectSearchResults); app.refreshSuggestions(); app.m_selectedSuggestion = 0;
            app.applyProjectSearch();
            if (!app.m_lastProjectSearchMatch || app.m_searchBar.isOpen()) return 220;
            input::KeyEvent returnSearch{}; returnSearch.key = input::Key::O; returnSearch.shift = true; returnSearch.action = input::KeyAction::Press;
#if defined(__APPLE__)
            returnSearch.super = true;
#else
            returnSearch.control = true;
#endif
            app.handleKey(returnSearch);
            if (app.m_searchBar.kind() != ui::PromptKind::ProjectSearchResults || app.m_selectedSuggestion < 0 ||
                app.m_projectSearchChoices[static_cast<std::size_t>(app.m_selectedSuggestion)] != 0) return 221;
            app.m_document.setCursor(0, 0); app.m_document.insert(U'X');
            app.applyProjectSearch();
            if (!app.m_searchBar.isOpen() || !app.m_commandStatusIsError || !app.m_document.dirty() ||
                app.m_document.toUtf8() != "Xopened externally") return 252;
            std::string unchanged, readError;
            if (!app.m_project.readFile("example.txt", unchanged, readError) || unchanged != "opened externally") return 253;
            app.m_document.undo();
            { std::ofstream changed(temp / "nested" / "example.txt", std::ios::trunc); changed << "changed externally"; }
            app.applyProjectSearch();
            if (!app.m_searchBar.isOpen() || !app.m_commandStatusIsError || app.m_document.toUtf8() != "opened externally") return 254;
            { std::ofstream restored(temp / "nested" / "example.txt", std::ios::trunc); restored << "opened externally"; }
            { std::ofstream file(temp / "nested" / "cached-search.txt"); file << "needle"; }
            auto cachedSearch = editor::Document::fromUtf8("needle"); cachedSearch.insert(U'X');
            app.m_inactiveDocuments.push_back({"cached-search.txt", std::move(cachedSearch), 0, "needle"});
            app.m_projectSearchResults = {}; app.m_projectSearchResults.files.push_back({"cached-search.txt", "needle"});
            app.m_projectSearchResults.matches.push_back({0, 0, 0, 0, "needle"});
            app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyProjectSearch();
            if (!app.m_searchBar.isOpen() || !app.m_commandStatusIsError || app.m_filePath != "example.txt" ||
                !app.m_inactiveDocuments.back().document.dirty()) return 255;
            app.m_inactiveDocuments.pop_back(); std::filesystem::remove(temp / "nested" / "cached-search.txt");
            app.m_searchBar.close(); app.m_projectSearchResults = {}; app.m_lastProjectSearchMatch.reset();
        }
        {
            app.m_projectSearchResults = {};
            app.m_projectSearchResults.files.push_back({"example.txt", "opened externally"});
            app.m_projectSearchResults.matches.push_back({0, 0, 0, 0, "opened externally"});
            app.m_projectSearchQuery = "opened"; app.m_projectReplacement = "changed";
            app.m_document.setCursor(0, 0); app.m_document.insert(U'X');
            app.m_searchBar.open(ui::PromptKind::ProjectReplaceReview); app.m_selectedSuggestion = 0;
            app.applyProjectSearch();
            std::ifstream disk(temp / "nested" / "example.txt");
            const std::string contents((std::istreambuf_iterator<char>(disk)), {});
            if (contents != "opened externally" || app.m_document.toUtf8() != "Xopened externally" ||
                !app.m_commandStatusIsError || app.m_commandStatus.find("unsaved") == std::string::npos) return 205;
            app.m_document.undo();
            auto background = editor::Document::fromUtf8("opened elsewhere"); background.insert(U'X');
            app.m_inactiveDocuments.push_back({"background.txt", std::move(background), 0, "opened elsewhere"});
            app.m_projectSearchResults.files[0] = {"background.txt", "opened elsewhere"};
            app.m_selectedSuggestion = 0; app.applyProjectSearch();
            if (!app.m_commandStatusIsError || app.m_commandStatus.find("background.txt") == std::string::npos ||
                app.m_inactiveDocuments.back().document.toUtf8() != "Xopened elsewhere") return 206;
            app.m_inactiveDocuments.pop_back(); app.m_searchBar.close(); app.m_projectSearchResults = {};
        }
        {
            { std::ofstream out(temp / "nested" / "selective.txt"); out << "needle needle"; }
            app.m_project.refresh();
            app.m_projectSearchResults = workspace::searchProject(app.m_project, "needle");
            if (app.m_projectSearchResults.matches.size() != 2) return 212;
            app.m_projectSearchSelected = {false, true}; app.m_projectSearchQuery = "needle"; app.m_projectReplacement = "pin";
            const auto oldSettingsRoot = app.m_settingsRoot; app.m_settingsRoot = (temp / "settings").string();
            app.m_searchBar.open(ui::PromptKind::ProjectReplaceReview); app.m_selectedSuggestion = 0;
            app.applyProjectSearch();
            app.m_settingsRoot = oldSettingsRoot;
            std::ifstream disk(temp / "nested" / "selective.txt");
            const std::string contents((std::istreambuf_iterator<char>(disk)), {});
            if (contents != "needle pin" || app.m_statusIsError) return 213;
            app.m_projectSearchResults = {}; app.m_projectSearchSelected.clear();
        }
        if (std::getenv("LUCID_TEST_CLANGD")) {
            const std::string source = "// 😀 marker\nint first(){return 1;}\nint second(){return 2;}\n";
            { std::ofstream out(temp / "nested" / "format.cpp"); out << source; }
            app.openFile("format.cpp");
            if (app.m_filePath != "format.cpp") return 191;
            app.m_document.setCursor(0, 4); app.m_document.extendSelectionTo(1, 22);
            app.requestLanguageFormatting();
            if (!app.m_languageFormatQueued || app.m_languageFormatRange.find("\"character\":5") == std::string::npos ||
                app.m_languageFormatRange.find("\"line\":1,\"character\":22") == std::string::npos) return 192;
            const auto formatDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while ((app.m_languageFormatQueued || app.m_languageFormatRequest) && std::chrono::steady_clock::now() < formatDeadline) {
                app.pollLanguageServer(); std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (app.m_languageFormatQueued || app.m_languageFormatRequest || app.m_document.toUtf8() == source ||
                app.m_document.toUtf8().find("int second(){return 2;}") == std::string::npos) return 193;
            app.m_document.undo();
            if (app.m_document.toUtf8() != source || app.m_document.selectionUtf8().empty()) return 194;
            app.requestLanguageFormatting();
            app.m_document.insert(U'x');
            const auto changed = app.m_document.toUtf8();
            app.pollLanguageServer();
            if (app.m_languageFormatQueued || app.m_languageFormatRequest || app.m_document.toUtf8() != changed ||
                app.m_statusText.find("document changed") == std::string::npos) return 196;
            app.m_document.undo();
            app.openFile("example.txt");
            if (app.m_filePath != "example.txt") return 195;
        }
        {
            workspace::ProjectFunction stale;
            stale.name = "old-build"; stale.file = "example.txt"; stale.line = 0;
            app.runProjectFunction(stale);
            if (!app.m_statusIsError || app.m_statusText.find("changed or moved") == std::string::npos ||
                app.m_compileTerminalProcess.alive()) return 178;
            stale.file = "missing-function.cpp";
            app.runProjectFunction(stale);
            if (std::filesystem::exists(temp / "nested/missing-function.cpp")) return 179;
            { std::ofstream external(temp / "nested/example.txt", std::ios::trunc); external << "changed externally"; }
            editor::CompileFunction command; command.name = "build"; command.commands.push_back({"true", 0});
            app.runCompileFunction(command);
            if (!app.m_statusIsError || app.m_statusText.find("Compile source changed") == std::string::npos ||
                app.m_compileTerminalProcess.alive()) return 180;
            { std::ofstream external(temp / "nested/example.txt", std::ios::trunc); external << "opened externally"; }
        }
#if !defined(_WIN32)
        {
            std::string debugError;
            if (!app.m_debugAdapter.start("/bin/cat", {}, debugError)) return 175;
            app.m_debugSession.start("program", temp.string(), "example.txt", {});
            app.m_debugClosing = false; app.m_debugStartDeadline = 1.0;
            app.openDebugger(); app.pollDebugger();
            if (app.m_debugAdapter.alive() || app.m_debugSession.state() != debug::State::Failed ||
                !app.m_commandStatusIsError || app.m_commandStatus.find("did not start") == std::string::npos ||
                std::find(app.m_debugChoices.begin(), app.m_debugChoices.end(), -10) == app.m_debugChoices.end()) return 176;
            app.m_searchBar.close();
        }
#endif
        {
            const auto previousRoot = app.m_project.root();
            const auto otherProject = temp / "other-project";
            std::filesystem::create_directories(otherProject);
            app.m_document.insert(U'X');
            const auto unsaved = app.m_document.toUtf8();
            { std::ofstream external(temp / "nested/example.txt", std::ios::trunc); external << "external change"; }
            workspace::session::State previous; previous.project = otherProject.string();
            previous.files.push_back({"example.txt", 0, 0, 0});
            app.restoreSession(previous);
            if (app.m_project.root() != previousRoot || app.m_document.toUtf8() != unsaved ||
                !app.m_document.dirty() || !app.m_pendingSession ||
                app.m_searchBar.kind() != ui::PromptKind::SessionRestore) return 173;
            { std::ofstream target(temp / "nested/another.txt"); target << "another"; }
            app.openExternalPath((temp / "nested/another.txt").string());
            if (!app.m_pendingSession || app.m_project.root() != previousRoot ||
                app.m_document.toUtf8() != unsaved || !app.m_document.dirty()) return 174;
            { std::ofstream external(temp / "nested/example.txt", std::ios::trunc); external << "opened externally"; }
            app.m_document.undo(); app.m_pendingSession.reset(); app.m_searchBar.close();
        }
        {
            const auto priorPath = app.m_recoveryFile; auto priorDocument = std::move(app.m_document);
            app.m_document = editor::Document::fromUtf8("active edits"); app.m_document.markRecovered();
            app.m_recoveryFile = temp / "partial-checkpoint" / "active.json";
            auto bad = editor::Document::fromUtf8("unsaved"); bad.markRecovered();
            auto good = editor::Document::fromUtf8("good edits"); good.markRecovered();
            app.m_inactiveDocuments.push_back({"bad.txt", std::move(bad), 0, "mixed\r\nendings\n"});
            app.m_inactiveDocuments.push_back({"good.txt", std::move(good), 0, "good original"});
            if (app.checkpointRecovery() || app.m_statusText.find("bad.txt") == std::string::npos) return 267;
            workspace::recovery::Snapshot partialActive, partialGood; std::string partialError;
            const auto goodKey = app.m_project.root().generic_string() + '\0' + "good.txt";
            const auto goodPath = app.m_inactiveRecoveryFiles.find(goodKey);
            if (!workspace::recovery::read(app.m_recoveryFile, partialActive, partialError) || partialActive.contents != "active edits" ||
                goodPath == app.m_inactiveRecoveryFiles.end() || !workspace::recovery::read(goodPath->second, partialGood, partialError) || partialGood.contents != "good edits") return 268;
            std::filesystem::remove(goodPath->second); app.m_inactiveRecoveryFiles.erase(goodPath);
            app.m_inactiveDocuments.pop_back(); app.m_inactiveDocuments.pop_back();
            app.m_document = std::move(priorDocument); app.m_recoveryFile = priorPath;
        }
        {
            const auto priorRecoveryFile = app.m_recoveryFile;
            app.m_recoveryFile = temp / "checkpoint" / "snapshot.json";
            if (app.m_document.dirty()) return 264;
            auto unsavedInactive = editor::Document::fromUtf8("inactive original"); unsavedInactive.insert(U'X');
            app.m_inactiveDocuments.push_back({"inactive-recovery.txt", std::move(unsavedInactive), 0, "inactive original"});
            const auto recoverySettings = app.m_settingsRoot;
            app.m_settingsRoot = "/dev/null/Settings";
            if (app.checkpointInactiveEdits() || !app.m_inactiveDocuments.back().document.dirty()) return 265;
            const auto originalProject = app.m_project.root();
            if (app.openProject((temp / "other-project").string()) || app.m_project.root() != originalProject ||
                !app.m_inactiveDocuments.back().document.dirty()) return 266;
            app.m_settingsRoot = recoverySettings;
            app.openRecovery();
            if (app.m_searchBar.kind() != ui::PromptKind::RecoveryList || app.m_commandStatusIsError) return 170;
            const auto recoveryKey = app.m_project.root().generic_string() + '\0' + "inactive-recovery.txt";
            const auto inactivePath = app.m_inactiveRecoveryFiles.find(recoveryKey);
            workspace::recovery::Snapshot inactiveSnapshot; std::string inactiveError;
            if (inactivePath == app.m_inactiveRecoveryFiles.end() || !workspace::recovery::read(inactivePath->second, inactiveSnapshot, inactiveError) ||
                inactiveSnapshot.contents != "Xinactive original" || inactiveSnapshot.baseline != "inactive original") return 257;
            const auto listedInactive = std::find(app.m_recoveryFiles.begin(), app.m_recoveryFiles.end(), inactivePath->second);
            if (listedInactive == app.m_recoveryFiles.end() || app.m_recoverySummaries[static_cast<std::size_t>(listedInactive - app.m_recoveryFiles.begin())].diskState != "Disk file unavailable") return 263;
            app.m_recoveryFiles = {inactivePath->second}; app.m_recoverySelected = 0; app.m_recoveryPreview = inactiveSnapshot;
            app.m_recoveryPage = 0; app.m_searchBar.open(ui::PromptKind::RecoveryDetail); app.refreshSuggestions();
            const auto inactiveDelete = std::find(app.m_recoveryChoices.begin(), app.m_recoveryChoices.end(), 1);
            if (!app.recoverySnapshotProtected() || inactiveDelete == app.m_recoveryChoices.end()) return 259;
            const auto inactiveRow = static_cast<std::size_t>(inactiveDelete - app.m_recoveryChoices.begin());
            if (app.m_suggestionDetails[inactiveRow].find("protects unsaved") == std::string::npos) return 260;
            app.m_selectedSuggestion = static_cast<int>(inactiveRow); app.applyRecovery();
            if (!app.m_commandStatusIsError || !std::filesystem::exists(inactivePath->second)) return 261;
            const auto savedPath = app.m_filePath; auto savedDocument = std::move(app.m_document);
            app.m_filePath = "inactive-recovery.txt"; app.m_document = editor::Document::fromUtf8("edited"); app.m_document.markRecovered();
            app.m_inactiveDocuments.pop_back();
            if (!app.recoverySnapshotProtected()) return 262;
            app.m_inactiveDocuments.push_back({"inactive-recovery.txt", editor::Document::fromUtf8("inactive original"), 0, "inactive original"});
            app.m_document = std::move(savedDocument); app.m_filePath = savedPath;
            app.m_searchBar.close(); app.m_recoveryFiles.clear(); app.clearCommandFeedback();
            app.m_inactiveDocuments.pop_back(); std::filesystem::remove(inactivePath->second); app.m_inactiveRecoveryFiles.erase(inactivePath);
            if (!app.checkpointRecovery()) return 258;
            const auto oldTime = std::filesystem::file_time_type::clock::now() - std::chrono::hours(1);
            std::filesystem::last_write_time(app.m_recoveryFile, oldTime);
            const auto storedTime = std::filesystem::last_write_time(app.m_recoveryFile);
            if (!app.checkpointRecovery() || std::filesystem::last_write_time(app.m_recoveryFile) != storedTime) return 171;
            std::filesystem::remove(app.m_recoveryFile);
            if (!app.checkpointRecovery() || !std::filesystem::exists(app.m_recoveryFile)) return 172;
            app.m_recoveryFile = priorRecoveryFile;
        }
        {
            auto previousDocument = std::move(app.m_document);
            app.m_document = editor::Document::fromUtf8(std::string(workspace::Project::maxFileSize + 1, 'x'));
            app.m_document.markRecovered();
            if (app.saveDocument() || !app.m_document.dirty() || app.m_statusText.find("16 MiB limit") == std::string::npos) return 164;
            std::ifstream unchanged(temp / "nested" / "example.txt");
            if (std::string((std::istreambuf_iterator<char>(unchanged)), {}) != "opened externally") return 165;
            app.m_document = std::move(previousDocument);
        }
        app.openProjectSearch(false); app.m_searchBar.setQuery("opened");
        app.m_projectSearchRoot = app.m_project.root().generic_string();
        std::promise<workspace::ProjectSearchResults> failedSearch;
        app.m_projectSearchFuture = failedSearch.get_future();
        failedSearch.set_exception(std::make_exception_ptr(std::runtime_error("test failure")));
        app.pollProjectSearch();
        if (app.m_searchBar.kind() != ui::PromptKind::ProjectSearch || app.m_searchBar.query() != "opened" ||
            !app.m_commandStatusIsError || app.m_commandStatus.find("test failure") == std::string::npos) return 163;
        app.m_searchBar.close();
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
        app.m_document.setCursor(0, 0); app.m_document.insert(U'X');
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
            switchFrame.suggestions[0].detail.find("Unsaved") == std::string_view::npos ||
            switchFrame.suggestions[1].path != "example.txt") return 34;
        app.m_document.undo();
        app.m_selectedSuggestion = 1;
        input::KeyEvent closeSelected{}; closeSelected.key = input::Key::W; closeSelected.action = input::KeyAction::Press;
#if defined(__APPLE__)
        closeSelected.super = true;
#else
        closeSelected.control = true;
#endif
#if defined(__APPLE__)
        if (!triggerCloseFileShortcut()) return 219;
#else
        app.handleKey(closeSelected);
#endif
        if (app.m_filePath != "other.txt" || app.m_closedDocuments.empty() || app.m_closedDocuments.front().path != "example.txt") return 218;
        input::KeyEvent reopenSelected = closeSelected; reopenSelected.key = input::Key::E; reopenSelected.shift = true;
        app.handleKey(reopenSelected);
        if (app.m_filePath != "example.txt" || app.m_document.cursorColumn() != 18) return 35;
        app.m_document.undo();
        if (app.m_document.toUtf8() != "opened externally") return 36;
        app.openFile("other.txt");
        { std::ofstream changed(temp / "nested" / "example.txt", std::ios::trunc); changed << "external edit"; }
        app.openFile("example.txt");
        if (app.m_document.toUtf8() != "external edit") return 37;
        app.closeDocument();
        if (!app.m_hasDocument || app.m_filePath != "other.txt") return 44;
        input::KeyEvent reopen{}; reopen.key = input::Key::E; reopen.shift = true; reopen.action = input::KeyAction::Press;
#if defined(__APPLE__)
        reopen.super = true;
#else
        reopen.control = true;
#endif
        app.handleKey(reopen);
        if (app.m_filePath != "example.txt" || app.m_document.toUtf8() != "external edit") return 216;
        app.closeDocument();
        if (app.m_filePath != "other.txt") return 217;
        app.openFile("example.txt");
        app.openFile("other.txt");
        std::filesystem::remove(temp / "nested" / "example.txt");
        app.openFile("example.txt");
        if (std::filesystem::exists(temp / "nested" / "example.txt") || app.m_filePath != "other.txt") return 38;
        app.closeDocument();
        if (std::filesystem::exists(temp / "nested" / "example.txt")) return 39;
        app.openFile("other.txt");
        { std::ofstream scrollFile(temp / "nested" / "scroll.txt");
          for (int i = 0; i < 100; ++i) scrollFile << "line " << i << '\n'; }
        app.openFile("scroll.txt");
        app.m_document.setCursor(0, 0);
        app.m_firstLine = 30; app.m_followCursor = false;
        app.openFile("other.txt");
        app.openFile("scroll.txt");
        (void)app.makeFrameState(6.0);
        if (app.m_firstLine != 30 || app.m_document.cursorLine() != 0) return 132;
        app.openFile("other.txt");
        { std::ofstream(temp / "nested" / "still-open.txt") << "available";
          std::ofstream(temp / "nested" / "missing-open.txt") << "removed";
          std::ofstream(temp / "nested" / "closing.txt") << "closing"; }
        app.openFile("still-open.txt");
        if (app.m_filePath != "still-open.txt") { std::cerr << "Could not open fallback file: " << app.m_statusText << '\n'; return 149; }
        app.openFile("missing-open.txt");
        if (app.m_filePath != "missing-open.txt") { std::cerr << "Could not open file to remove: " << app.m_statusText << '\n'; return 150; }
        app.openFile("closing.txt");
        if (app.m_filePath != "closing.txt") { std::cerr << "Could not open file to close: " << app.m_statusText << '\n'; return 151; }
        if (!std::filesystem::remove(temp / "nested" / "missing-open.txt")) return 152;
        app.closeDocument();
        if (!app.m_hasDocument || app.m_filePath != "still-open.txt" ||
            std::filesystem::exists(temp / "nested" / "missing-open.txt")) {
            std::cerr << "Close fallback: " << app.m_filePath << " status: " << app.m_statusText << '\n'; return 133;
        }
        app.openFile("other.txt");
        { std::ofstream(temp / "nested" / "reload.txt") << "original"; }
        app.openFile("reload.txt");
        app.m_document.setCursor(0, 3);
        { std::ofstream(temp / "nested" / "reload.txt", std::ios::trunc) << "changed"; }
        app.openFile("reload.txt");
        if (app.m_document.toUtf8() != "changed" || app.m_document.cursorColumn() != 3 || app.m_document.dirty()) return 134;
        { std::ofstream out(temp / "nested" / "reload.txt", std::ios::binary | std::ios::trunc); out.put('\0'); }
        app.openFile("reload.txt");
        if (app.m_document.toUtf8() != "changed" || app.m_diskContents != "changed") return 135;
        { std::ofstream(temp / "nested" / "reload.txt", std::ios::trunc) << "changed"; }
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
        const auto changedIndex = static_cast<std::size_t>(changed - app.m_gitChanges.begin());
        app.m_gitPage = (changedIndex + 6) / app.pagedContentCount();
        app.refreshSuggestions();
        const auto gitChoice = std::find(app.m_gitChoices.begin(), app.m_gitChoices.end(), static_cast<int>(changedIndex));
        if (gitChoice == app.m_gitChoices.end()) return 58;
        app.m_selectedSuggestion = static_cast<int>(gitChoice - app.m_gitChoices.begin());
        app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::GitFile) return 58;
        app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::GitDiff) return 85;
        app.m_renderer->drawFrame(app.makeFrameState(8.0));
        auto line = std::find_if(app.m_gitReview.lines.begin(), app.m_gitReview.lines.end(),
            [](const auto& item) { return !item.text.empty() && item.text[0] == '+' && item.line == 1; });
        if (line == app.m_gitReview.lines.end()) return 59;
        const auto lineIndex = static_cast<std::size_t>(line - app.m_gitReview.lines.begin());
        app.m_gitPage = lineIndex / app.pagedContentCount();
        app.refreshSuggestions();
        const auto lineChoice = std::find(app.m_gitChoices.begin(), app.m_gitChoices.end(), static_cast<int>(lineIndex));
        if (lineChoice == app.m_gitChoices.end()) return 59;
        app.m_selectedSuggestion = static_cast<int>(lineChoice - app.m_gitChoices.begin());
        app.applyCommand(false);
        if (app.m_filePath != "other.txt" || app.m_document.cursorLine() != 0) return 60;
        app.openGitReview();
        changed = std::find_if(app.m_gitChanges.begin(), app.m_gitChanges.end(), [](const auto& item) { return item.path == "other.txt"; });
        if (changed == app.m_gitChanges.end()) return 86;
        const auto stageIndex = static_cast<std::size_t>(changed - app.m_gitChanges.begin());
        app.m_gitPage = (stageIndex + 6) / app.pagedContentCount();
        app.refreshSuggestions();
        const auto stageChoice = std::find(app.m_gitChoices.begin(), app.m_gitChoices.end(), static_cast<int>(stageIndex));
        if (stageChoice == app.m_gitChoices.end()) return 86;
        app.m_selectedSuggestion = static_cast<int>(stageChoice - app.m_gitChoices.begin());
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
        // A pull that reports failure may nevertheless have changed the working tree.
        const auto simulateFailedPull = [&] {
            std::promise<Application::GitRemoteResult> result;
            app.m_gitRemoteFuture = result.get_future();
            app.m_gitRemoteRoot = app.m_project.root().generic_string();
            app.m_gitRemoteAction = workspace::GitRemoteAction::Pull;
            result.set_value({false, {}, "merge conflict"});
            app.pollGitRemote();
        };
        { std::ofstream out(temp / "nested/other.txt", std::ios::trunc); out << "conflict markers"; }
        simulateFailedPull();
        if (app.m_document.toUtf8() != "conflict markers" || app.m_diskContents != "conflict markers" ||
            !app.m_statusIsError) return 157;
        app.m_document.insert(U'Z');
        const auto unsavedAfterPull = app.m_document.toUtf8();
        { std::ofstream out(temp / "nested/other.txt", std::ios::trunc); out << "new conflict markers"; }
        simulateFailedPull();
        if (app.m_document.toUtf8() != unsavedAfterPull || !app.m_document.dirty() ||
            app.m_diskContents != "conflict markers" || app.m_statusText.find("changed on disk") == std::string::npos) return 158;
        { std::ofstream out(temp / "nested/other.txt", std::ios::trunc); out << "conflict markers"; }
        if (!app.saveDocument()) return 159;
#if defined(__APPLE__)
        if (std::getenv("LUCID_TEST_LLDB")) {
            std::filesystem::create_directories(temp / "nested" / "run");
            { std::ofstream marker(temp / "nested" / "run" / "cwd-marker"); marker << "present"; }
            { std::ofstream source(temp / "nested/debug.cpp"); source << "#include <atomic>\nextern \"C\" int access(const char*, int); extern \"C\" char* getenv(const char*); extern \"C\" unsigned int sleep(unsigned int); extern \"C\" long write(int,const void*,unsigned long); std::atomic<bool> ready{false}; void* worker(void*) { ready.store(true); for (;;) sleep(1); } int main(int argc, char** argv) { const char* env = getenv(\"LUCID_DEBUG_TEST\"); if (argc != 2 || argv[1][0] != 'h' || argv[1][5] != ' ' || access(\"cwd-marker\",0) || !env || env[0] != 'y') return 3; pthread_t t; if (pthread_create(&t,nullptr,worker,nullptr)) return 4; while (!ready.load()) {} write(1,\"WORKER_READY\\n\",13);\n  volatile int n = 7;\n  n += 1;\n  sleep(30); return n == 8 ? 0 : 1;\n}\n"; }
            const std::string compile = "c++ -g -O0 '" + (app.m_project.root() / "debug.cpp").string() + "' -o '" + (app.m_project.root() / "debug-program").string() + "'";
            if (std::system(compile.c_str()) != 0) return 91;
            app.openFile("debug.cpp"); app.m_activeTaskId = app.m_taskBoard.tasks[0].id;
            app.m_document.setCursor(3, 0); app.toggleBreakpoint();
            if (app.m_taskBoard.tasks[0].debugSource != "debug.cpp" || app.m_taskBoard.tasks[0].debugBreakpoints != std::vector<int>{4}) return 99;
            app.openDebugger(); app.applyCommand(false);
            if (app.m_searchBar.kind() != ui::PromptKind::DebugInput) return 92;
            app.m_searchBar.setQuery("debug-program"); app.applyCommand(false);
            if (!app.m_debugAwaitArguments || app.m_searchBar.query() != "[]" ||
                app.makeFrameState(9.0).commandPlaceholder != "Arguments as JSON array...") return 227;
            app.m_searchBar.setQuery(R"(["hello world",7])"); app.applyCommand(false);
            if (!app.m_commandStatusIsError || app.m_debugAdapter.alive()) return 228;
            app.m_searchBar.setQuery(R"(["hello world"])"); app.applyCommand(false);
            if (!app.m_debugAwaitWorkingDirectory || app.m_searchBar.query() != ".") return 229;
            app.m_searchBar.setQuery("../outside"); app.applyCommand(false);
            if (!app.m_commandStatusIsError || app.m_debugAdapter.alive()) return 230;
            app.m_searchBar.setQuery("run"); app.applyCommand(false);
            if (!app.m_debugAwaitEnvironment || app.m_searchBar.query() != "{}") return 231;
            app.m_searchBar.setQuery(R"({"BAD=NAME":"x"})"); app.applyCommand(false);
            if (!app.m_commandStatusIsError || app.m_debugAdapter.alive()) return 232;
            app.m_searchBar.setQuery(R"({"LUCID_DEBUG_TEST":"yes"})"); app.applyCommand(false);
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
            {
                const auto oldRoot = app.m_moduleRoot; auto oldModules = app.m_modules; const auto oldSelected = app.m_moduleSelected;
                const auto oldProvider = app.m_settings.aiDebugProvider;
                app.m_moduleRoot = temp / "live-explainer-modules";
                const std::string manifest = R"({"name":"Explain","version":"1","permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Explain debug","action":"explain-debug-local"}]})";
                std::string error;
                if (!modules::install(app.m_moduleRoot, "live-explain", manifest, error) || !modules::setEnabled(app.m_moduleRoot, "live-explain", true, error) || !modules::list(app.m_moduleRoot, app.m_modules, error)) return 372;
                const auto expectedFrame = app.m_debugSession.stack()[0].name;
                const auto expectedVariable = app.m_debugSession.variables()[0].name;
                app.m_settings.aiDebugProvider = "openrouter"; app.m_moduleSelected = 0; app.m_modulePage = 0; app.m_moduleScanShowingResults = false;
                app.m_searchBar.open(ui::PromptKind::ModuleCommands); app.refreshSuggestions(); app.m_selectedSuggestion = 0; app.applyModuleCommand();
                if (app.m_searchBar.kind() != ui::PromptKind::Ai || !app.m_aiDebugExplanation || app.m_aiDebugOpenRouter ||
                    app.m_aiDebugContext.find(expectedFrame) == std::string::npos || app.m_aiDebugContext.find(expectedVariable) == std::string::npos) return 373;
                if (std::getenv("LUCID_TEST_LOCAL_AI")) {
                    const auto model = settingsDirectory().parent_path() / "Models" / "qwen2.5-coder-0.5b-instruct-q4_k_m.gguf";
                    if (!std::filesystem::is_regular_file(model)) { std::cerr << "Local test model unavailable\n"; return 374; }
                    app.applyCommand(false);
                    const auto aiDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(90);
                    while (app.m_aiFuture.valid() && std::chrono::steady_clock::now() < aiDeadline) { app.pollAi(); std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
                    if (app.m_aiFuture.valid() || !app.m_aiReady || app.m_aiResponse.empty()) { std::cerr << "Local explanation: " << app.m_commandStatus << "\n"; return 375; }
                }
                app.m_searchBar.close(); app.m_settings.aiDebugProvider = oldProvider;
                app.m_moduleRoot = oldRoot; app.m_modules = std::move(oldModules); app.m_moduleSelected = oldSelected;
            }
            if (std::getenv("LUCID_TEST_MULTITHREAD_LLDB") && app.m_debugSession.output().find("WORKER_READY") == std::string::npos) {
                std::cerr << "Worker startup marker missing; stop occurred before worker initialization\n"; return 240;
            }
            app.openDebugger();
            auto threadChoice = std::find(app.m_debugChoices.begin(), app.m_debugChoices.end(), -24);
            while (threadChoice == app.m_debugChoices.end()) {
                const auto next = std::find(app.m_debugChoices.begin(), app.m_debugChoices.end(), -2);
                if (next == app.m_debugChoices.end()) return 233;
                app.m_selectedSuggestion = static_cast<int>(next - app.m_debugChoices.begin()); app.applyDebugCommand();
                threadChoice = std::find(app.m_debugChoices.begin(), app.m_debugChoices.end(), -24);
            }
            app.m_selectedSuggestion = static_cast<int>(threadChoice - app.m_debugChoices.begin()); app.applyDebugCommand();
            while ((app.m_debugSession.threads().empty() || (std::getenv("LUCID_TEST_MULTITHREAD_LLDB") && app.m_debugSession.threads().size() < 2)) && std::chrono::steady_clock::now() < deadline) {
                app.pollDebugger(); std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (app.m_searchBar.kind() != ui::PromptKind::DebugThreads || app.m_debugSession.threads().empty()) return 234;
            if (std::getenv("LUCID_TEST_MULTITHREAD_LLDB") && app.m_debugSession.threads().size() < 2) {
                std::cerr << "thread count=" << app.m_debugSession.threads().size() << " selected=" << app.m_debugSession.selectedThread() << " status=" << app.m_debugSession.message() << "\n"; return 234;
            }
            const int mainThread = app.m_debugSession.selectedThread();
            if (std::getenv("LUCID_TEST_MULTITHREAD_LLDB")) {
            const auto worker = std::find_if(app.m_debugSession.threads().begin(), app.m_debugSession.threads().end(), [mainThread](const auto& thread) { return thread.id != mainThread; });
            if (worker == app.m_debugSession.threads().end()) return 236;
            const auto workerIndex = static_cast<int>(worker - app.m_debugSession.threads().begin());
            app.m_selectedSuggestion = workerIndex; app.applyDebugCommand();
            while (app.m_debugSession.stack().empty() && std::chrono::steady_clock::now() < deadline) {
                app.pollDebugger(); std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (app.m_debugSession.stack().empty() || app.m_debugSession.selectedThread() == mainThread) return 237;
            std::string threadError;
            if (!app.m_debugSession.requestThreads(threadError)) return 238;
            while (app.m_debugSession.threadsPending() && std::chrono::steady_clock::now() < deadline) {
                app.pollDebugger(); std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            const auto main = std::find_if(app.m_debugSession.threads().begin(), app.m_debugSession.threads().end(), [mainThread](const auto& thread) { return thread.id == mainThread; });
            if (main == app.m_debugSession.threads().end() || !app.m_debugSession.selectThread(static_cast<std::size_t>(main - app.m_debugSession.threads().begin()), threadError)) return 239;
            } else {
                app.m_selectedSuggestion = 0; app.applyDebugCommand();
            }
            while ((app.m_debugSession.stack().empty() || app.m_debugSession.variables().empty()) && std::chrono::steady_clock::now() < deadline) {
                app.pollDebugger(); std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (app.m_debugSession.stack().empty() || app.m_debugSession.variables().empty()) return 235;
            app.m_searchBar.close();
            const auto debugFrame = app.makeFrameState(9.0);
            if (debugFrame.debugStoppedLine != 3 || debugFrame.debugBreakpointLines.empty() || debugFrame.debugInline.empty()) return 95;
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
                if (app.m_debugSession.state() == debug::State::Stopped && !app.m_debugSession.stack().empty() && app.m_debugSession.stack()[0].line == 5 && !app.m_debugSession.variables().empty()) { reached = true; break; }
                if (!app.m_debugAdapter.alive()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (!reached) return 97;
            if (app.makeFrameState(9.5).debugInline.find("* n =") == std::string_view::npos) return 105;
            app.openDebugger();
            const auto resumeChoice = std::find(app.m_debugChoices.begin(), app.m_debugChoices.end(), -12);
            if (resumeChoice == app.m_debugChoices.end()) return 241;
            app.m_selectedSuggestion = static_cast<int>(resumeChoice - app.m_debugChoices.begin()); app.applyDebugCommand();
            app.openDebugger();
            const auto pauseChoice = std::find(app.m_debugChoices.begin(), app.m_debugChoices.end(), -26);
            if (pauseChoice == app.m_debugChoices.end()) return 242;
            app.m_selectedSuggestion = static_cast<int>(pauseChoice - app.m_debugChoices.begin()); app.applyDebugCommand();
            const auto pauseDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (app.m_debugSession.state() == debug::State::Running && app.m_debugAdapter.alive() && std::chrono::steady_clock::now() < pauseDeadline) {
                app.pollDebugger(); std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (app.m_debugSession.state() != debug::State::Stopped) {
                std::cerr << "pause failed: " << app.m_debugSession.message() << " output=" << app.m_debugSession.output() << "\n"; return 243;
            }
            app.stopDebugger();
            while (app.m_debugAdapter.alive() && std::chrono::steady_clock::now() < deadline) { app.pollDebugger(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
            if (app.m_debugAdapter.alive()) return 98;
            const auto debugTaskId = app.m_activeTaskId;
            app.m_activeTaskId.clear(); app.m_debugBreakpoints.clear();
            if (!app.activateTask(debugTaskId) || app.m_debugBreakpoints["debug.cpp"] != std::vector<int>{4}) return 104;
            app.m_activeTaskId.clear();
            app.openFile("other.txt");
        }
#endif
        const auto baseline = app.m_diskContents;
        app.m_document.insert(U'Y');
        { std::ofstream external(temp / "nested/other.txt", std::ios::trunc); external << "external change"; }
        if (app.saveDocument() || !app.m_document.dirty()) return 71;
        workspace::recovery::Snapshot savedBeforeConflict; std::string conflictError;
        if (!workspace::recovery::read(app.m_recoveryFile, savedBeforeConflict, conflictError) ||
            savedBeforeConflict.contents != app.m_document.toUtf8() || savedBeforeConflict.baseline != baseline) return 256;
        const auto beforeConflictedReopen = app.m_document.toUtf8();
        app.openFile("other.txt");
        if (app.m_document.toUtf8() != beforeConflictedReopen || !app.m_document.dirty() || app.m_diskContents != baseline) return 136;
        { std::ifstream external(temp / "nested/other.txt"); std::string text((std::istreambuf_iterator<char>(external)), {});
          if (text != "external change") return 72; }
        const auto unsavedConflict = app.m_document.toUtf8();
        app.m_settingsRoot = (temp / "settings" / "Settings").string();
        app.m_recoveryFile.clear();
        app.checkpointRecovery();
        workspace::recovery::Snapshot conflictSnapshot;
        std::string recoveryError;
        if (!workspace::recovery::read(app.m_recoveryFile, conflictSnapshot, recoveryError)) return 105;
        app.openRecovery();
        const auto activeRecovery = std::find(app.m_recoveryFiles.begin(), app.m_recoveryFiles.end(), app.m_recoveryFile);
        if (activeRecovery == app.m_recoveryFiles.end()) return 105;
        app.m_recoverySelected = static_cast<std::size_t>(activeRecovery - app.m_recoveryFiles.begin());
        if (!workspace::recovery::read(app.m_recoveryFile, app.m_recoveryPreview, recoveryError)) return 105;
        app.m_recoveryPage = 0;
        app.m_searchBar.open(ui::PromptKind::RecoveryDetail);
        app.clearCommandFeedback();
        app.refreshSuggestions();
        const auto deleteChoice = std::find(app.m_recoveryChoices.begin(), app.m_recoveryChoices.end(), 1);
        if (deleteChoice == app.m_recoveryChoices.end()) return 110;
        app.m_selectedSuggestion = static_cast<int>(deleteChoice - app.m_recoveryChoices.begin());
        app.applyRecovery();
        if (!std::filesystem::exists(app.m_recoveryFile) || !app.m_commandStatusIsError ||
            app.m_commandStatus.find("protects your current unsaved edits") == std::string::npos) return 111;
        app.clearCommandFeedback();
        app.refreshSuggestions();
        const auto copyChoice = std::find(app.m_recoveryChoices.begin(), app.m_recoveryChoices.end(), 3);
        if (copyChoice == app.m_recoveryChoices.end()) return 106;
        app.m_selectedSuggestion = static_cast<int>(copyChoice - app.m_recoveryChoices.begin());
        app.applyRecovery();
        bool recoveredCopy = false;
        for (const auto& entry : std::filesystem::directory_iterator(temp / "nested")) {
            if (entry.path().filename().string().starts_with("other.recovered-")) {
                std::ifstream copy(entry.path());
                std::string text((std::istreambuf_iterator<char>(copy)), {});
                recoveredCopy = text == unsavedConflict;
            }
        }
        if (!recoveredCopy || !app.m_document.dirty() || app.m_document.toUtf8() != unsavedConflict || app.m_filePath != "other.txt") return 107;
        { std::ifstream external(temp / "nested/other.txt"); std::string text((std::istreambuf_iterator<char>(external)), {});
          if (text != "external change") return 108; }
        const auto validRecoveryPath = app.m_recoveryFile;
        app.m_recoveryFile = "/dev/null/unavailable.json";
        app.openRecovery();
        if (!app.m_commandStatusIsError || app.m_commandStatus.find("could not be backed up") == std::string::npos ||
            app.m_document.toUtf8() != unsavedConflict || !app.m_document.dirty()) return 109;
        app.m_searchBar.close();
        app.m_recoveryFile = validRecoveryPath;
        { std::ofstream restore(temp / "nested/other.txt", std::ios::trunc); restore << baseline; }
        if (!app.saveDocument()) return 73;
        app.m_settingsRoot = (temp / "settings" / "Settings").string();
        app.m_document.insert(U'Z');
        const auto recoveredText = app.m_document.toUtf8();
        app.checkpointRecovery();
        if (!std::filesystem::exists(app.m_recoveryFile)) return 74;
        const auto expectedRecoveryPath = app.m_recoveryFile;
        app.m_recoveryFile.clear();
        if (!app.saveDocument()) return 75;
        { std::ofstream external(temp / "nested/other.txt", std::ios::trunc); external << "newer version"; }
        app.openRecovery();
        if (app.m_searchBar.kind() != ui::PromptKind::RecoveryList || app.m_recoveryFiles.empty()) return 76;
        const auto expectedRecovery = std::find(app.m_recoveryFiles.begin(), app.m_recoveryFiles.end(), expectedRecoveryPath);
        if (expectedRecovery == app.m_recoveryFiles.end()) return 77;
        const int recoveryIndex = static_cast<int>(expectedRecovery - app.m_recoveryFiles.begin());
        app.m_recoveryPage = static_cast<std::size_t>(recoveryIndex) / app.pagedContentCount();
        app.refreshSuggestions();
        const auto recoveryChoice = std::find(app.m_recoveryChoices.begin(), app.m_recoveryChoices.end(), recoveryIndex);
        if (recoveryChoice == app.m_recoveryChoices.end()) return 77;
        app.m_selectedSuggestion = static_cast<int>(recoveryChoice - app.m_recoveryChoices.begin());
        app.applyCommand(false);
        if (app.m_searchBar.kind() != ui::PromptKind::RecoveryDetail || app.m_recoveryPreview.contents != recoveredText) return 77;
        {
            const auto validFiles = app.m_recoveryFiles; const auto validSelected = app.m_recoverySelected;
            const auto validPreview = app.m_recoveryPreview; auto unsafe = validPreview;
            unsafe.contents = std::string("unsafe\0text", 11);
            const auto unsafePath = temp / "unsafe-recovery.json";
            if (!workspace::recovery::write(unsafePath, unsafe, recoveryError)) return 166;
            app.m_recoveryFiles = {unsafePath}; app.m_recoverySelected = 0; app.m_recoveryPreview = unsafe;
            app.m_selectedSuggestion = 0;
            const auto beforeUnsafeRestore = app.m_document.toUtf8();
            app.applyRecovery();
            if (app.m_document.toUtf8() != beforeUnsafeRestore || !app.m_commandStatusIsError ||
                app.m_commandStatus.find("cannot be opened safely") == std::string::npos) return 167;
            unsafe.contents = validPreview.contents; unsafe.baseline = "mixed\r\nendings\n";
            if (!workspace::recovery::write(unsafePath, unsafe, recoveryError)) return 168;
            app.m_recoveryPreview = unsafe; app.m_selectedSuggestion = 0;
            app.applyRecovery();
            if (app.m_document.toUtf8() != beforeUnsafeRestore || !app.m_commandStatusIsError ||
                app.m_commandStatus.find("cannot be opened safely") == std::string::npos) return 169;
            app.m_recoveryFiles = validFiles; app.m_recoverySelected = validSelected; app.m_recoveryPreview = validPreview;
            app.clearCommandFeedback(); app.m_selectedSuggestion = 0;
        }
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
int main() {
    // This executable exercises session restoration explicitly. Do not load or
    // overwrite the desktop user's real session before those tests run.
#if defined(_WIN32)
    if (_putenv_s("LUCID_SESSION_DISABLED", "1") != 0) return 1;
#else
    if (setenv("LUCID_SESSION_DISABLED", "1", 1) != 0) return 1;
#endif
    return lucid::ApplicationInteractionTest::run();
}
