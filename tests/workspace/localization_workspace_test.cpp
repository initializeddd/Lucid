#include "modules/LocalizationWorkspace.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    namespace fs = std::filesystem;
    using namespace lucid::modules;
    auto root = fs::temp_directory_path() / ("lucid-localization-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    auto write = [&](const char* name, const std::string& content) { std::ofstream(root / name, std::ios::binary) << content; };
    write("en.json", R"({"title":"Hello", "nested":{"a/b":"\u00e9", "only":"x"}})");
    write("de.json", R"({"title":"Hallo!", "nested":{"a/b":"ab", "extra":"x"}})");
    std::vector<LocalizationEntry> entries; std::string error;
    bool ok = compareLocalizationFiles(root, "en.json", "de.json", entries, error) && entries.size() == 4;
    ok = ok && entries[0].key == "/nested/a~1b" && entries[0].sourceLength == 1 && entries[0].translationLength == 2 && entries[0].sourceText == "\xc3\xa9" && entries[0].translationText == "ab";
    ok = ok && entries[0].sourceOffset == std::string(R"({"title":"Hello", "nested":{"a/b":"\u00e9", "only":"x"}})").find("\"\\u00e9\"") && entries[0].translationOffset == std::string(R"({"title":"Hallo!", "nested":{"a/b":"ab", "extra":"x"}})").find("\"ab\"");
    ok = ok && entries[1].key == "/nested/extra" && !entries[1].inSource && entries[1].inTranslation;
    ok = ok && entries[2].key == "/nested/only" && entries[2].inSource && !entries[2].inTranslation;
    ok = ok && entries[3].sourceLength == 5 && entries[3].translationLength == 6;
    write("bad.json", "{\"x\":");
    ok = ok && !compareLocalizationFiles(root, "en.json", "bad.json", entries, error) && entries.empty();
    write("bad.json", R"({"x":"a","x":"b"})");
    ok = ok && !compareLocalizationFiles(root, "en.json", "bad.json", entries, error);
    write("bad.json", R"({"x":42})");
    ok = ok && !compareLocalizationFiles(root, "en.json", "bad.json", entries, error);
    write("en.json", R"({"plural":{"one":"one", "other":"many"}, "list":["first", {"label":"second"}]})");
    write("de.json", std::string(R"({"plural":{"one":"\u05d0", "other":")") + std::string(121, 'x') + R"("}, "list":["first"]})");
    ok = ok && compareLocalizationFiles(root, "en.json", "de.json", entries, error) && entries.size() == 4;
    ok = ok && entries[1].key == "/list/1/label" && !entries[1].inTranslation;
    ok = ok && entries[2].key == "/plural/one" && entries[2].translationHasRtl && !entries[2].sourceHasRtl && entries[2].translationText == "\xd7\x90";
    ok = ok && entries[3].translationLong && !entries[3].sourceLong && entries[3].translationLength == 121;
    write("bad.json", std::string("{\"x\":\"") + char(0xff) + "\"}");
    ok = ok && !compareLocalizationFiles(root, "en.json", "bad.json", entries, error);
    std::string deep; for (int i = 0; i < 100; ++i) deep += "{\"a\":"; deep += "\"x\""; for (int i = 0; i < 100; ++i) deep += '}';
    write("bad.json", deep);
    ok = ok && !compareLocalizationFiles(root, "en.json", "bad.json", entries, error);
    ok = ok && !compareLocalizationFiles(root, "en.json", root / ".." / "outside.json", entries, error);
    ok = ok && compareLocalizationText(R"({"hello":"Hi"})", R"({"hello":"Hallo","extra":"x"})", entries, error)
        && entries.size() == 2 && entries[1].sourceText == "Hi" && entries[1].translationText == "Hallo";
    ok = ok && !compareLocalizationText("{}", std::string(1024 * 1024 + 1, ' '), entries, error) && entries.empty();
    ok = ok && !compareLocalizationText("{}", "{", entries, error) && entries.empty();
    ok = ok && compareLocalizationText(R"({"greet":"Hello {name}, {count}; {{literal}}"})", R"({"greet":"Hallo {name}, {other}; {{literal}}"})", entries, error)
        && entries.size() == 1 && entries[0].missingPlaceholders == std::vector<std::string>{"count"}
        && entries[0].extraPlaceholders == std::vector<std::string>{"other"};
    ok = ok && compareLocalizationText(R"({"greet":"{name} {name} {other}"})", R"({"greet":"{name} {other} {other}"})", entries, error)
        && entries.size() == 1 && entries[0].missingPlaceholders.empty() && entries[0].extraPlaceholders.empty()
        && entries[0].differingPlaceholderCounts.size() == 2
        && entries[0].differingPlaceholderCounts[0].name == "name" && entries[0].differingPlaceholderCounts[0].source == 2 && entries[0].differingPlaceholderCounts[0].translation == 1
        && entries[0].differingPlaceholderCounts[1].name == "other" && entries[0].differingPlaceholderCounts[1].source == 1 && entries[0].differingPlaceholderCounts[1].translation == 2;
    fs::remove_all(root); return ok ? 0 : 1;
}
