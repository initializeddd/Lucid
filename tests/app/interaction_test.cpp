#include "app/Application.hpp"
#include "ai/ModelDownload.hpp"
#include "platform/macos/VulkanEnvironment.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <cstdlib>
#include <thread>

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
        app.m_languageServer.close(); app.m_languageRoot = "test-language-server";
        app.m_languageNavigationQueued = true; app.m_languageFormatRequest = 7;
        app.pollLanguageServer();
        if (!app.m_languageRoot.empty() || app.m_languageNavigationQueued || app.m_languageFormatRequest != 0 ||
            !app.m_statusIsError || app.m_statusText.find("Language server stopped") == std::string::npos) return 188;
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
          out << R"({"name":"Example","version":"1","permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Insert hello","insert":"hello"}]})"; }
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
        { std::ofstream out(temp / "nested" / "example.txt"); out << "opened externally"; }
        app.openExternalPath((temp / "nested" / "example.txt").string());
        if (!app.m_hasDocument || app.m_document.toUtf8() != "opened externally") return 24;
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
            const auto priorRecoveryFile = app.m_recoveryFile;
            app.m_recoveryFile = temp / "checkpoint" / "snapshot.json";
            if (!app.checkpointRecovery()) return 170;
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
        app.m_gitPage = (changedIndex + 5) / app.pagedContentCount();
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
        app.m_gitPage = (stageIndex + 5) / app.pagedContentCount();
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
            { std::ofstream source(temp / "nested/debug.cpp"); source << "extern \"C\" int access(const char*, int); extern \"C\" char* getenv(const char*); extern \"C\" int pthread_create(unsigned long*, const void*, void*(*)(void*), void*); extern \"C\" unsigned int sleep(unsigned int); void* worker(void*) { for (;;) sleep(1); } int main(int argc, char** argv) { const char* env = getenv(\"LUCID_DEBUG_TEST\"); if (argc != 2 || argv[1][0] != 'h' || argv[1][5] != ' ' || access(\"cwd-marker\",0) || !env || env[0] != 'y') return 3; unsigned long t; if (pthread_create(&t,nullptr,worker,nullptr)) return 4;\n  volatile int n = 7;\n  n += 1;\n  return n == 8 ? 0 : 1;\n}\n"; }
            const std::string compile = "c++ -g -O0 '" + (app.m_project.root() / "debug.cpp").string() + "' -o '" + (app.m_project.root() / "debug-program").string() + "'";
            if (std::system(compile.c_str()) != 0) return 91;
            app.openFile("debug.cpp"); app.m_activeTaskId = app.m_taskBoard.tasks[0].id;
            app.m_document.setCursor(2, 0); app.toggleBreakpoint();
            if (app.m_taskBoard.tasks[0].debugSource != "debug.cpp" || app.m_taskBoard.tasks[0].debugBreakpoints != std::vector<int>{3}) return 99;
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
            app.openDebugger();
            auto threadChoice = std::find(app.m_debugChoices.begin(), app.m_debugChoices.end(), -24);
            while (threadChoice == app.m_debugChoices.end()) {
                const auto next = std::find(app.m_debugChoices.begin(), app.m_debugChoices.end(), -2);
                if (next == app.m_debugChoices.end()) return 233;
                app.m_selectedSuggestion = static_cast<int>(next - app.m_debugChoices.begin()); app.applyDebugCommand();
                threadChoice = std::find(app.m_debugChoices.begin(), app.m_debugChoices.end(), -24);
            }
            app.m_selectedSuggestion = static_cast<int>(threadChoice - app.m_debugChoices.begin()); app.applyDebugCommand();
            while (app.m_debugSession.threads().empty() && std::chrono::steady_clock::now() < deadline) {
                app.pollDebugger(); std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (app.m_searchBar.kind() != ui::PromptKind::DebugThreads || app.m_debugSession.threads().size() < 2) {
                std::cerr << "thread count=" << app.m_debugSession.threads().size() << " selected=" << app.m_debugSession.selectedThread() << " status=" << app.m_debugSession.message() << "\n"; return 234;
            }
            const int mainThread = app.m_debugSession.selectedThread();
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
            while ((app.m_debugSession.stack().empty() || app.m_debugSession.variables().empty()) && std::chrono::steady_clock::now() < deadline) {
                app.pollDebugger(); std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (app.m_debugSession.stack().empty() || app.m_debugSession.variables().empty()) return 235;
            app.m_searchBar.close();
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
        app.m_recoveryFile.clear();
        if (!app.saveDocument()) return 75;
        { std::ofstream external(temp / "nested/other.txt", std::ios::trunc); external << "newer version"; }
        app.openRecovery();
        if (app.m_searchBar.kind() != ui::PromptKind::RecoveryList || app.m_recoveryFiles.empty()) return 76;
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
