#include "workspace/search/ProjectSearch.hpp"
#include "workspace/recovery/History.hpp"
#include "editor/document/Document.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("lucid-project-search-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::string error; bool created = false;
    auto project = lucid::workspace::Project::openOrCreate(root, created, error);
    if (!error.empty() || !created) return 1;
    if (!project.writeFile("a.cpp", "hello target\r\ntarget end\r\n", error) ||
        !project.writeFile("src/b.cpp", "other target\n", error) ||
        !project.writeFile("binary.bin", std::string("\0target", 7), error)) return 2;
    project.refresh();
    auto results = lucid::workspace::searchProject(project, "target");
    const auto filtered = lucid::workspace::searchProject(project, "target", 1000, nullptr, false, true, "src/");
    if (filtered.files.size() != 1 || filtered.files[0].path != "src/b.cpp" || filtered.matches.size() != 1 || filtered.skipped != 0) return 53;
    const auto wildcard = lucid::workspace::searchProject(project, "target", 1000, nullptr, false, true, "*.cpp, !src/");
    if (wildcard.files.size() != 1 || wildcard.files[0].path != "a.cpp" || wildcard.matches.size() != 2) return 54;
    const auto excludeOnly = lucid::workspace::searchProject(project, "target", 1000, nullptr, false, true, "!src/");
    if (excludeOnly.files.size() != 1 || excludeOnly.files[0].path != "a.cpp") return 55;
    const auto alternatives = lucid::workspace::searchProject(project, "target", 1000, nullptr, false, true, "a.cpp, src/*.cpp");
    if (alternatives.files.size() != 2 || alternatives.matches.size() != 3) return 56;
    const auto invalidFilter = lucid::workspace::searchProject(project, "target", 1000, nullptr, false, true, "!");
    if (!invalidFilter.indexIncomplete || !invalidFilter.matches.empty()) return 57;
    std::atomic_bool cancelSearch{true};
    std::string cancelledContent = "old";
    if (project.readFile("a.cpp", cancelledContent, error, &cancelSearch) || !cancelledContent.empty() || error.find("cancelled") == std::string::npos) return 50;
    if (lucid::editor::Document::canEditLosslessly("valid", error, &cancelSearch) || error.find("cancelled") == std::string::npos) return 51;
    if (!lucid::editor::Document::canEditLosslessly("\xef\xbf\xbd", error) ||
        lucid::editor::Document::canEditLosslessly("\xc0\x80", error) ||
        lucid::editor::Document::canEditLosslessly("\xed\xa0\x80", error) ||
        lucid::editor::Document::canEditLosslessly("a\r\nb\nc", error)) return 52;
    const auto cancelled = lucid::workspace::searchProject(project, "target", 1000, &cancelSearch);
    if (!cancelled.indexIncomplete || !cancelled.matches.empty()) return 37;
    if (results.matches.size() != 3 || results.files.size() != 2 || results.skipped != 1) return 3;
    if (results.firstSkippedPath != "binary.bin" || results.firstSkippedReason != "binary content") return 63;
    if (results.skippedFiles.size() != 1 || results.skippedFiles[0].path != "binary.bin" || results.skippedFiles[0].reason != "binary content") return 69;
    if (results.matches[1].line != 1 || results.matches[1].column != 0) return 4;
    const auto limited = lucid::workspace::searchProject(project, "target", 1);
    std::size_t changed = 0;
    if (!limited.truncated || lucid::workspace::replaceProject(project, limited, "target", "x", changed, error)) return 5;
    auto incomplete = results; incomplete.indexIncomplete = true;
    if (lucid::workspace::replaceProject(project, incomplete, "target", "x", changed, error) || changed != 0) return 25;
    const auto missingRoot = root / "vanishing-project";
    auto missing = lucid::workspace::Project::openOrCreate(missingRoot, created, error);
    if (!error.empty() || !created) return 26;
    fs::remove_all(missingRoot);
    bool scanComplete = true; missing.refresh(&scanComplete);
    if (scanComplete) return 27;
    project.refresh(&scanComplete);
    if (!scanComplete) return 28;
    if (!project.writeFile("src/b.cpp", "changed elsewhere\n", error)) return 6;
    if (lucid::workspace::replaceProject(project, results, "target", "x", changed, error) || changed != 0) return 7;
    std::string text;
    if (!project.readFile("a.cpp", text, error) || text != "hello target\r\ntarget end\r\n") return 8;
    if (!project.writeFile("src/b.cpp", "other target\n", error)) return 9;
    if (lucid::workspace::replaceProject(project, results, "target", std::string("\0", 1), changed, error) || changed != 0 || error.empty()) return 29;
    if (lucid::workspace::replaceProject(project, results, "target", std::string("\xff", 1), changed, error) || changed != 0 || error.empty()) return 30;
    const std::string largeReplacement(lucid::workspace::Project::maxFileSize / 2 + 1, 'x');
    if (lucid::workspace::replaceProject(project, results, "target", largeReplacement, changed, error) ||
        changed != 0 || error.find("file limit") == std::string::npos) return 32;
    if (!project.readFile("a.cpp", text, error) || text != "hello target\r\ntarget end\r\n") return 31;
    if (!lucid::workspace::replaceProject(project, results, "target", "line\nline", changed, error) || changed != 2) return 33;
    if (!project.readFile("a.cpp", text, error) || text != "hello line\r\nline\r\nline\r\nline end\r\n") return 34;
    if (!project.readFile("src/b.cpp", text, error) || text != "other line\nline\n") return 35;
    if (!project.writeFile("a.cpp", "hello target\r\ntarget end\r\n", error) ||
        !project.writeFile("src/b.cpp", "other target\n", error)) return 36;
    const auto unavailableHistory = root / "not-a-directory";
    { std::ofstream blocked(unavailableHistory); blocked << "x"; }
    if (lucid::workspace::replaceProject(project, results, "target", "name", changed, error, nullptr, unavailableHistory) || changed != 0 || error.empty()) return 14;
    if (!project.readFile("a.cpp", text, error) || text != "hello target\r\ntarget end\r\n") return 15;
    const auto historyRoot = root / ".lucid-safety";
    if (!lucid::workspace::replaceProject(project, results, "target", "name", changed, error, nullptr, historyRoot) || changed != 2) return 10;
    if (!project.readFile("a.cpp", text, error) || text != "hello name\r\nname end\r\n") return 11;
    if (!project.readFile("src/b.cpp", text, error) || text != "other name\n") return 12;
    std::vector<fs::path> backups;
    if (!lucid::workspace::recovery::listHistory(historyRoot, project.root().generic_string(), backups, error) || backups.size() != 2) return 13;
    const std::string longLine = std::string(200, 'x') + "\xf0\x9f\x8c\xb2" + " target " + std::string(200, 'y');
    if (!project.writeFile("long.cpp", longLine + "\r\n", error)) return 16;
    project.refresh();
    const auto longResults = lucid::workspace::searchProject(project, "target");
    bool checkedLong = false;
    for (const auto& match : longResults.matches) {
        if (longResults.files[match.file].path != "long.cpp") continue;
        checkedLong = true;
        if (match.column != 202 || match.preview.find("target") == std::string::npos ||
            match.preview.substr(0, 3) != "..." ||
            match.preview.substr(match.preview.size() - 3) != "...") return 17;
        if (!lucid::editor::Document::canEditLosslessly(match.preview, error)) return 18;
    }
    if (!checkedLong) return 17;
    std::string unicodeLine;
    for (int i = 0; i < 200; ++i) unicodeLine += "\xf0\x9f\x8c\xb2";
    unicodeLine += " target ";
    for (int i = 0; i < 200; ++i) unicodeLine += "\xf0\x9f\x8c\xb2";
    if (!project.writeFile("unicode.cpp", unicodeLine, error)) return 19;
    project.refresh();
    const auto unicodeResults = lucid::workspace::searchProject(project, "target");
    bool checkedUnicode = false;
    for (const auto& match : unicodeResults.matches) {
        if (unicodeResults.files[match.file].path != "unicode.cpp") continue;
        checkedUnicode = true;
        if (match.column != 201 || match.preview.find("target") == std::string::npos ||
            !lucid::editor::Document::canEditLosslessly(match.preview, error)) return 20;
    }
    if (!checkedUnicode) return 21;
    if (!project.writeFile("positions.cpp", "target \xce\xb2 target\r\n\xce\xb2 target target\r\ntarget", error)) return 22;
    project.refresh();
    const auto positionResults = lucid::workspace::searchProject(project, "target");
    const std::size_t expectedLines[] = {0, 0, 1, 1, 2};
    const std::size_t expectedColumns[] = {0, 9, 2, 9, 0};
    std::size_t positionIndex = 0;
    for (const auto& match : positionResults.matches) {
        if (positionResults.files[match.file].path != "positions.cpp") continue;
        if (positionIndex >= 5 || match.line != expectedLines[positionIndex] ||
            match.column != expectedColumns[positionIndex]) return 23;
        ++positionIndex;
    }
    if (positionIndex != 5) return 24;
    if (!project.writeFile("whole.cpp", "needle needles _needle needle_ (needle) éneedle needleé\n", error)) return 38;
    project.refresh();
    const auto whole = lucid::workspace::searchProject(project, "needle", 1000, nullptr, true);
    if (whole.matches.size() != 2 || whole.files.size() != 1) return 39;
    if (lucid::workspace::replacementPreview(whole.files[0], whole.matches[0], "needle", "a\nb").find("a ↵ b") == std::string::npos ||
        lucid::workspace::replacementPreview(whole.files[0], whole.matches[0], "wrong", "x") != "Preview unavailable") return 41;
    auto invalid = whole; invalid.matches[0].offset++;
    if (lucid::workspace::replaceProject(project, invalid, "needle", "pin", changed, error) || changed != 0 ||
        !project.readFile("whole.cpp", text, error) || text != "needle needles _needle needle_ (needle) éneedle needleé\n") return 40;
    if (
        !lucid::workspace::replaceProject(project, whole, "needle", "pin", changed, error) || changed != 1 ||
        !project.readFile("whole.cpp", text, error) || text != "pin needles _needle needle_ (pin) éneedle needleé\n") return 39;
    if (!project.writeFile("case.cpp", "Needle NEEDLE needleX (nEeDlE)\n", error)) return 42;
    project.refresh();
    const auto insensitive = lucid::workspace::searchProject(project, "needle", 1000, nullptr, true, false);
    if (insensitive.caseSensitive || insensitive.matches.size() != 3 || insensitive.files.size() != 1) return 43;
    if (lucid::workspace::replacementPreview(insensitive.files[0], insensitive.matches[0], "needle", "pin", false).find("pin") == std::string::npos) return 44;
    if (!lucid::workspace::replaceProject(project, insensitive, "needle", "pin", changed, error) || changed != 1 ||
        !project.readFile("case.cpp", text, error) || text != "pin pin needleX (pin)\n") return 45;
    if (!project.writeFile("overlap.cpp", "a-a- ", error)) return 46;
    project.refresh();
    const auto overlap = lucid::workspace::searchProject(project, "a-", 1000, nullptr, true);
    if (overlap.matches.size() != 1 || overlap.matches[0].offset != 2) return 47;
    const std::string adversarial(1024 * 1024, 'a');
    if (!project.writeFile("large.cpp", adversarial + "B", error)) return 48;
    project.refresh();
    const auto longMatch = lucid::workspace::searchProject(project, std::string(4096, 'a') + "b", 1000, nullptr, false, false);
    if (longMatch.matches.size() != 1 || longMatch.matches[0].offset != adversarial.size() - 4096) return 49;
    if (!project.writeFile("filter/keep.cpp", "token", error) || !project.writeFile("filter/vendor/skip.cpp", "token", error)) return 58;
    project.refresh();
    const auto scoped = lucid::workspace::searchProject(project, "token", 1000, nullptr, false, true, "filter/*.cpp,!vendor/");
    if (scoped.matches.size() != 1 || !lucid::workspace::replaceProject(project, scoped, "token", "done", changed, error) || changed != 1 ||
        !project.readFile("filter/keep.cpp", text, error) || text != "done" ||
        !project.readFile("filter/vendor/skip.cpp", text, error) || text != "token") return 59;
    if (!project.writeFile("filter/disappears.cpp", "token", error)) return 60;
    project.refresh();
    fs::remove(root / "filter/disappears.cpp");
    const auto missingFile = lucid::workspace::searchProject(project, "token", 1000, nullptr, false, true, "filter/");
    if (missingFile.readErrors != 1 || missingFile.firstReadErrorPath != "filter/disappears.cpp" || missingFile.matches.empty() ||
        lucid::workspace::replaceProject(project, missingFile, "token", "unsafe", changed, error) || changed != 0) return 61;
    if (missingFile.firstSkippedPath != "filter/disappears.cpp" || missingFile.firstSkippedReason != "could not read") return 64;
    if (missingFile.skippedFiles.size() != 1 || missingFile.skippedFiles[0].path != "filter/disappears.cpp") return 70;
    const auto omittedMissing = lucid::workspace::searchProject(project, "token", 1000, nullptr, false, true, "filter/,!disappears.cpp");
    if (omittedMissing.readErrors != 0 || omittedMissing.matches.size() != 1) return 62;
    { std::ofstream mixed(root / "filter/mixed.txt", std::ios::binary); mixed << "token\r\nline\n"; }
    project.refresh();
    const auto mixedFile = lucid::workspace::searchProject(project, "token", 1000, nullptr, false, true, "mixed.txt");
    if (mixedFile.firstSkippedPath != "filter/mixed.txt" || mixedFile.firstSkippedReason != "unsupported text encoding or line endings" || mixedFile.skipped != 1) return 65;
    fs::create_directories(root / "build"); fs::create_directories(root / ".git"); fs::create_directories(root / ".lucid");
    { std::ofstream file(root / "build/generated.txt"); file << "unique-generated-token"; }
    { std::ofstream file(root / ".git/private.txt"); file << "unique-generated-token"; }
    { std::ofstream file(root / ".lucid/private.txt"); file << "unique-generated-token"; }
    bool complete = false; project.refresh(&complete);
    if (!complete || !lucid::workspace::searchProject(project, "unique-generated-token").matches.empty()) return 66;
    project.refresh(&complete, nullptr, true);
    const auto generated = lucid::workspace::searchProject(project, "unique-generated-token");
    if (!complete || generated.matches.size() != 1 || generated.files[0].path != "build/generated.txt") return 67;
    std::atomic_bool cancelGenerated{true}; project.refresh(&complete, &cancelGenerated, true);
    if (complete) return 68;
    fs::remove_all(root); return 0;
}
