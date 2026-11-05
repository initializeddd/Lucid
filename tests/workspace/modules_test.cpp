#include "modules/Modules.hpp"
#include "modules/Registry.hpp"
#include "modules/SecretScan.hpp"
#include "modules/MarkdownOutline.hpp"
#include "modules/MarkdownPreview.hpp"
#include "modules/MarkdownLinks.hpp"
#include "modules/Coverage.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace lucid::modules;
int main() {
    {
        MarkdownPreview rendered; std::string error;
        if (!parseMarkdownPreview("Title\n=====\n\nHello **world**.\n\n```cpp\n# literal\n```\n\n<script>bad()</script>\n", rendered, error) || rendered.truncated || rendered.blocks.size() != 3 ||
            rendered.blocks[0].kind != MarkdownBlockKind::Heading || rendered.blocks[0].level != 1 || rendered.blocks[0].text != "Title" ||
            rendered.blocks[1].text != "Hello world." || rendered.blocks[2].kind != MarkdownBlockKind::Code || rendered.blocks[2].text.find("# literal") == std::string::npos) return 43;
        if (parseMarkdownPreview(std::string(16U * 1024U * 1024U + 1U, 'x'), rendered, error) || error.empty()) return 44;
        if (!parseMarkdownPreview("A &amp; B &#x3b2;\n\n- first\n- second\n\n> quote\n", rendered, error) || rendered.blocks.size() != 4 ||
            rendered.blocks[0].text != "A & B β" || rendered.blocks[1].kind != MarkdownBlockKind::ListItem ||
            rendered.blocks[2].kind != MarkdownBlockKind::ListItem || rendered.blocks[3].kind != MarkdownBlockKind::Quote) { for (const auto& block : rendered.blocks) std::cerr << static_cast<int>(block.kind) << ":" << block.text << "\n"; return 45; }
        if (!parseMarkdownPreview("- outer\n  - inner\n\n  after\n", rendered, error) || rendered.blocks.size() != 3 ||
            rendered.blocks[0].text != "outer" || rendered.blocks[1].text != "inner" || rendered.blocks[2].text != "after" ||
            rendered.blocks[2].kind != MarkdownBlockKind::ListItem) return 196;
        if (!parseMarkdownPreview({}, rendered, error) || !rendered.blocks.empty()) return 46;
        if (!parseMarkdownPreview("| a | b |\n|---|---|\n| x | y |\n", rendered, error) || rendered.blocks.empty() || rendered.blocks[0].text.find("| x | y |") == std::string::npos) return 47;
        if (!parseMarkdownPreview("- [ ] draft\n- [x] done\n- [X] reviewed\n\n3. third\n4. fourth\n", rendered, error) || rendered.truncated || rendered.blocks.size()!=5 ||
            rendered.blocks[0].text!="[ ] draft" || rendered.blocks[1].text!="[x] done" || rendered.blocks[2].text!="[x] reviewed" ||
            rendered.blocks[3].text!="3. third" || rendered.blocks[4].text!="4. fourth") return 197;
        if (!parseMarkdownPreview("5) outer\n   - [x] inner\n6) next\n", rendered, error) || rendered.blocks.size()!=3 ||
            rendered.blocks[0].text!="5) outer" || rendered.blocks[1].text!="[x] inner" || rendered.blocks[2].text!="6) next") return 198;
    }
    {
        const auto document = lucid::editor::Document::fromUtf8("# Hello ##\n```cpp\n# not a heading\n```\n  ## Über\n####### no\n#bad\n#\n");
        const auto outline = markdownOutline(document);
        if (outline.truncated || outline.headings.size() != 3 || outline.headings[0].text != "Hello" || outline.headings[0].line != 0 ||
            outline.headings[1].text != "Über" || outline.headings[1].line != 4 || outline.headings[1].level != 2 || !outline.headings[2].text.empty()) return 41;
        if (!markdownOutline(document, 1).truncated || markdownOutline(document, 1).headings.size() != 1) return 42;
    }
    ExpandedSnippet snippet; std::string expansionError;
    if (!expandSnippet("before {{selection}}\n  {{cursor}}after", "β", snippet, expansionError) ||
        snippet.text != "before β\n  after" || !snippet.hasCursor || snippet.cursorLine != 1 || snippet.cursorColumn != 2) return 20;
    if (expandSnippet("{{cursor}}x{{cursor}}", "", snippet, expansionError) || expansionError.empty()) return 21;
    if (expandSnippet("{{selection}}", std::string(1024U * 1024U + 1U, 'x'), snippet, expansionError) || expansionError.empty()) return 22;
    if (!expandSnippet("plain text", "ignored", snippet, expansionError) || snippet.hasCursor || snippet.text != "plain text") return 23;
    if (!expandSnippet("α\n{{placeholder:β\nx}}!", "", snippet, expansionError, true) || snippet.text != "α\nβ\nx!" ||
        !snippet.hasPlaceholder || snippet.cursorLine != 1 || snippet.cursorColumn != 0 || snippet.endLine != 2 || snippet.endColumn != 1) return 38;
    if (expandSnippet("{{placeholder:x}}{{cursor}}", "", snippet, expansionError, true) ||
        expandSnippet("{{placeholder:broken", "", snippet, expansionError, true)) return 39;
    if (!expandSnippet("{{placeholder:x}}", "", snippet, expansionError) || snippet.text != "{{placeholder:x}}") return 40;
    if (!expandSnippet("{{placeholder:α}} + {{placeholder:two\nlines}}{{cursor}}!", "", snippet, expansionError, true, true) ||
        snippet.text != "α + two\nlines!" || snippet.fields.size() != 2 || snippet.fields[0].start != 0 || snippet.fields[0].end != 2 ||
        snippet.fields[1].start != 5 || snippet.fields[1].end != 14 || snippet.finalOffset != 14 || snippet.finalFieldIndex != 2) return 58;
    if (!expandSnippet("{{placeholder:α}}\n{{placeholder:two\nlines}}{{cursor}}!", "", snippet, expansionError, true, true) ||
        !indentSnippet(snippet," \t",expansionError) || snippet.text!="α\n \ttwo\n \tlines!" ||
        snippet.fields.size()!=2 || snippet.text.substr(snippet.fields[1].start,snippet.fields[1].end-snippet.fields[1].start)!="two\n \tlines" ||
        snippet.finalOffset!=snippet.text.size()-1) return 130;
    if (indentSnippet(snippet,"x",expansionError)) return 131;
    if (expandSnippet("{{placeholder:x}}{{placeholder:y}}", "", snippet, expansionError, true) ||
        expandSnippet("{{placeholder:x}}{{placeholder:unclosed", "", snippet, expansionError, true, true)) return 59;
    if (!expandSnippet("{{linked:item:α}} + {{linked:item}} + {{placeholder:other}}{{cursor}}", "", snippet, expansionError, true, true, true) || snippet.text!="α + α + other" || snippet.fields.size()!=3 || !snippet.fields[0].link || snippet.fields[0].link!=snippet.fields[1].link || snippet.fields[2].link) return 1120;
    if (expandSnippet("{{linked:item}}", "", snippet, expansionError, true, true, true) || expandSnippet("{{linked:item:a}}{{linked:item:b}}", "", snippet, expansionError, true, true, true)) return 1121;
    if (!expandSnippet("line\r\n{{placeholder:a\rb}}{{cursor}}", "", snippet, expansionError, true, true) ||
        snippet.text != "line\na\nb" || snippet.fields[0].start != 5 || snippet.fields[0].end != 8 || snippet.finalOffset != 8) return 60;
    const auto invalidSnippet = R"({"name":"Bad","version":"1","permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"x","insert":"{{cursor}}{{cursor}}","snippet":true}]})";
    if (preview("bad-snippet", invalidSnippet).valid) return 24;
    const auto legacyMarkers = R"({"name":"Legacy","version":"1","permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"x","insert":"{{cursor}}{{cursor}}"}]})";
    if (!preview("legacy", legacyMarkers).valid || preview("legacy", legacyMarkers).commands[0].snippet) return 25;
    const auto web = R"({"name":"Web","version":"1","requires":["web-component-preview-macos-v1"],"permissions":{"project":true,"network":true,"processes":false,"settings":false},"commands":[{"title":"Preview","action":"web-preview"}]})";
#if defined(__APPLE__)
    if (!supportsCapability("web-component-preview-macos-v1") || !preview("web", web).valid || !preview("web", web).error.empty()) return 151;
#else
    if (supportsCapability("web-component-preview-macos-v1") || !preview("web", web).valid || preview("web", web).error != "Requires unsupported capability: web-component-preview-macos-v1") return 151;
#endif
    const auto noWebNetwork = R"({"name":"Web","version":"1","requires":["web-component-preview-macos-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Preview","action":"web-preview"}]})";
    if (preview("bad-web", noWebNetwork).valid) return 152;
    const auto cmakeEdit = R"({"name":"CMake","version":"1","requires":["cmake-cache-edit-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Edit","action":"edit-cmake-options"}]})";
    const auto noCmakePermission = R"({"name":"CMake","version":"1","requires":["cmake-cache-edit-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Edit","action":"edit-cmake-options"}]})";
    if (!supportsCapability("cmake-cache-edit-v1") || !preview("cmake-edit",cmakeEdit).valid || preview("cmake-edit",noCmakePermission).valid) return 195;
    const auto toolchain = R"({"name":"Tools","version":"1","requires":["toolchain-profiles-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Inspect","action":"inspect-toolchain-profiles"}]})";
    const auto toolchainNoProject = R"({"name":"Tools","version":"1","requires":["toolchain-profiles-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Inspect","action":"inspect-toolchain-profiles"}]})";
    if (!supportsCapability("toolchain-profiles-v1") || !preview("tools",toolchain).valid || !preview("tools",toolchain).error.empty() || preview("tools",toolchainNoProject).valid) return 197;
    const auto scanner = R"({"name":"Scanner","version":"1","permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Scan","action":"scan-secrets"}]})";
    if (!preview("scanner", scanner).valid || !preview("scanner", scanner).error.empty()) return 29;
    const auto unauthorized = R"({"name":"Scanner","version":"1","permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Scan","action":"scan-secrets"}]})";
    if (preview("bad-scan", unauthorized).valid) return 30;
    const auto arbitrary = R"({"name":"Bad","version":"1","permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Run","action":"run-anything"}]})";
    if (preview("bad-action", arbitrary).valid) return 31;
    const auto history = R"({"name":"History","version":"1","requires":["git-file-history-v1"],"permissions":{"project":true,"network":false,"processes":true,"settings":false},"commands":[{"title":"History","action":"git-file-history"}]})";
    if (!preview("history", history).valid || !preview("history", history).error.empty()) return 55;
    const auto rangeHistory = R"({"name":"History","version":"1","requires":["git-range-history-v1"],"permissions":{"project":true,"network":false,"processes":true,"settings":false},"commands":[{"title":"Range","action":"git-range-history"}]})";
    const auto unsafeRange = R"({"name":"History","version":"1","permissions":{"project":true,"network":false,"processes":true,"settings":false},"commands":[{"title":"Range","action":"git-range-history"}]})";
    if (!supportsCapability("git-range-history-v1") || !preview("range-history", rangeHistory).valid || preview("range-history", unsafeRange).valid) return 143;
    const auto unsafeProcess = R"({"name":"Unsafe","version":"1","permissions":{"project":false,"network":false,"processes":true,"settings":false},"commands":[{"title":"x","insert":"x"}]})";
    if (preview("unsafe-process", unsafeProcess).error.empty()) return 56;
    const auto noCapability = R"({"name":"History","version":"1","permissions":{"project":true,"network":false,"processes":true,"settings":false},"commands":[{"title":"History","action":"git-file-history"}]})";
    if (preview("history-no-capability", noCapability).valid) return 57;
    const auto pythonManager = R"({"name":"Python","version":"1","requires":["python-environments-v2"],"permissions":{"project":true,"network":false,"processes":true,"settings":true},"commands":[{"title":"Manage","action":"manage-python-environments"}]})";
    if (!preview("python-manager", pythonManager).valid || !preview("python-manager", pythonManager).error.empty()) return 61;
    const auto pythonUnauthorized = R"({"name":"Python","version":"1","requires":["python-environments-v2"],"permissions":{"project":true,"network":false,"processes":false,"settings":true},"commands":[{"title":"Manage","action":"manage-python-environments"}]})";
    if (preview("python-unauthorized", pythonUnauthorized).valid) return 62;
    const auto future = R"({"name":"Future","version":"1","requires":["future-v9"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"x","insert":"x"}]})";
    const auto futureModule = preview("future", future);
    if (!futureModule.valid || futureModule.error.find("future-v9") == std::string::npos || futureModule.requirements.size() != 1) return 32;
    const auto duplicateCapabilities = R"({"name":"Bad","version":"1","requires":["snippet-v1","snippet-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"x","insert":"x"}]})";
    if (preview("duplicate", duplicateCapabilities).valid) return 33;
    const auto duplicatePermission = R"({"name":"Bad","version":"1","permissions":{"project":false,"network":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"x","insert":"x"}]})";
    if (preview("duplicate-permission", duplicatePermission).valid) return 153;
    const auto duplicateCommand = R"({"name":"Bad","version":"1","permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"x","insert":"safe","insert":"other"}]})";
    if (preview("duplicate-command", duplicateCommand).valid) return 154;
    const auto localAi = R"({"name":"Explain","version":"1","requires":["local-error-explainer-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Explain","action":"explain-diagnostic-local"}]})";
    if (!preview("explain", localAi).valid || !preview("explain", localAi).error.empty()) return 36;
    const auto localDebug = R"({"name":"Debug","version":"1","requires":["local-debug-explainer-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Explain","action":"explain-debug-local"}]})";
    if (!preview("debug", localDebug).valid || !preview("debug", localDebug).error.empty()) return 37;
    const auto root = std::filesystem::temp_directory_path() / ("lucid-modules-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    {
        const auto scanRoot = root / "scan"; std::filesystem::create_directories(scanRoot);
        const auto fakeToken = std::string("ghp_") + std::string(36, 'A');
        const auto fakeKey = std::string("AKIA") + std::string(16, 'B');
        { std::ofstream file(scanRoot / "source.txt"); file << "α " << fakeToken << "\n" << fakeKey << "\n" << "-----BEGIN " << "PRIVATE KEY-----\n"; }
        { std::ofstream file(scanRoot / "no-match.txt"); file << "x" << fakeToken << "y\n" << "AKIAshort"; }
        { std::ofstream file(scanRoot / "binary.dat", std::ios::binary); file << '\0' << fakeToken; }
        bool created = false; std::string error;
        auto project = lucid::workspace::Project::openOrCreate(scanRoot, created, error);
        const auto scan = scanSecrets(project);
        if (!error.empty() || !scan.incomplete || scan.skipped != 1 || scan.findings.size() != 3 ||
            scan.findings[0].line != 0 || scan.findings[0].column != 2 || scan.findings[1].line != 1 || scan.findings[2].line != 2 ||
            scan.findings[0].kind.find(fakeToken) != std::string::npos) return 26;
        const auto limited = scanSecrets(project, nullptr, 1);
        if (!limited.incomplete || limited.findings.size() != 1) return 27;
        std::atomic_bool cancel{true}; if (!scanSecrets(project, &cancel).incomplete) return 28;
        std::filesystem::create_directories(scanRoot / "docs");
        { std::ofstream file(scanRoot / "docs" / "space name.txt"); file << "x"; }
        const auto links = checkMarkdownLinks(project, "docs/readme.md", "[ok](space%20name.txt) [missing](missing.txt) [web](https://example.com) [frag](space%20name.txt#part) [outside](../../outside)\n\n`[not](missing)`");
        if (links.incomplete || links.links.size() != 5 || links.links[0].status != MarkdownLinkStatus::Found || links.links[1].status != MarkdownLinkStatus::Missing ||
            links.links[2].status != MarkdownLinkStatus::NotChecked || links.links[3].status != MarkdownLinkStatus::NotChecked || links.links[4].status != MarkdownLinkStatus::NotChecked) return 48;
        const std::string source = "β [first](missing.txt) [second](missing.txt)";
        const auto mapped = checkMarkdownLinks(project, "docs/readme.md", source);
        if (mapped.links.size()!=2 || !mapped.links[0].sourceOffset || !mapped.links[1].sourceOffset ||
            source.substr(*mapped.links[0].sourceOffset,5)!="first" || source.substr(*mapped.links[1].sourceOffset,6)!="second") return 197;
        const auto unicodeDir = scanRoot / std::filesystem::path(u8"döcs");
        std::filesystem::create_directories(unicodeDir);
        { std::ofstream file(unicodeDir / std::filesystem::path(u8"ü.txt")); file << "x"; }
        const auto unicodeLinks = checkMarkdownLinks(project, "döcs/readme.md", "[ü](ü.txt)");
        if (unicodeLinks.links.size()!=1 || unicodeLinks.links[0].status!=MarkdownLinkStatus::Found) return 198;
        { std::ofstream file(scanRoot / "docs" / "guide.md"); file << "# Introduction\n\n## Same heading\n\n## Same heading\n"; }
        const auto anchors = checkMarkdownLinks(project, "docs/readme.md", "# Current Section\n[here](#current-section) [missing](#missing) [guide](guide.md#same-heading-1) [bad](guide.md#absent) [html](space%20name.txt#part)\n");
        if(anchors.incomplete || anchors.links.size()!=5 || anchors.links[0].status!=MarkdownLinkStatus::Found || anchors.links[1].status!=MarkdownLinkStatus::Missing || anchors.links[2].status!=MarkdownLinkStatus::Found || anchors.links[3].status!=MarkdownLinkStatus::Missing || anchors.links[4].status!=MarkdownLinkStatus::NotChecked) return 250;
        const auto uncertain = checkMarkdownLinks(project,"docs/readme.md", "# Hello, world!\n[x](#hello-world)\n");
        if(uncertain.links.size()!=1 || uncertain.links[0].status!=MarkdownLinkStatus::NotChecked) return 251;
        const auto custom = checkMarkdownLinks(project,"docs/readme.md", "<a id=\"custom\"></a>\n\n# Title\n[x](#custom)\n");
        if(custom.links.size()!=1 || custom.links[0].status!=MarkdownLinkStatus::NotChecked) return 252;
        if (!checkMarkdownLinks(project, "docs/readme.md", "[a](x) [b](y)", nullptr, 1).incomplete ||
            !checkMarkdownLinks(project, "docs/readme.md", "[a](x)", &cancel).incomplete) return 49;
        const auto coverage = parseLcov(project, "SF:source.txt\nDA:1,0\nDA:2,3\nend_of_record\nSF:source.txt\nDA:1,1\nend_of_record\n");
        if (coverage.incomplete || coverage.lines.size() != 2 || !coverage.lines[0].covered || !coverage.lines[1].covered || coverage.lines[0].line != 0) return 50;
        const auto badCoverage = parseLcov(project, "SF:../outside\nDA:1,1\nend_of_record\nSF:source.txt\nDA:0,1\nend_of_record\n");
        if (!badCoverage.incomplete || badCoverage.skippedFiles != 1 || !badCoverage.lines.empty()) return 51;
        if (!parseLcov(project, "SF:source.txt\nDA:1,0\nend_of_record\n", &cancel).incomplete) return 52;
        const auto branches = parseLcov(project, "SF:source.txt\nDA:1,1\nBRDA:1,0,0,1\nBRDA:1,0,1,0\nBRDA:1,0,2,-\nend_of_record\nSF:source.txt\nDA:1,0\nBRDA:1,0,0,0\nBRDA:1,0,1,-\nend_of_record\n");
        if (branches.incomplete || branches.lines.size()!=1 || branches.lines[0].branchesCovered!=1 || branches.lines[0].branchesUncovered!=1 || branches.lines[0].branchesUnknown!=1) return 193;
        if (!parseLcov(project, "SF:source.txt\nDA:1,1\nBRDA:1,0,0,bad\nend_of_record\n").incomplete || !parseLcov(project, "SF:source.txt\nBRDA:1,0,0,1\nend_of_of_record\n").incomplete || !parseLcov(project, "SF:source.txt\nDA:1,1\nBRDA:1,0,0,1\nend_of_record\n", nullptr, 0).incomplete) return 194;
        const std::string xml = R"(<?xml version="1.0"?><!DOCTYPE coverage SYSTEM "http://invalid.example/coverage.dtd"><coverage><sources><source>.</source></sources><packages><package><classes><class filename="source.txt"><methods><method><lines><line number="1" hits="999"/></lines></method></methods><lines><line number="1" hits="0"/><line number="2" hits="3"/><line number="1" hits="1"/></lines></class></classes></package></packages></coverage>)";
        const auto cobertura = parseCobertura(project, xml);
        if (cobertura.incomplete || cobertura.lines.size()!=2 || !cobertura.lines[0].covered || cobertura.lines[0].line!=0 || !cobertura.lines[1].covered) return 190;
        const auto invalidXml = parseCobertura(project, R"(<coverage><class filename="../outside"><lines><line number="1" hits="0"/></lines></class></coverage>)");
        if (!invalidXml.incomplete || !invalidXml.lines.empty() || invalidXml.skippedFiles!=1) return 191;
        if (!parseCobertura(project, "<!DOCTYPE coverage [<!ENTITY x SYSTEM 'file:///etc/passwd'>]><coverage/>").incomplete || !parseCobertura(project, "<coverage><class>").incomplete || !parseCobertura(project, xml, &cancel).incomplete || !parseCobertura(project, xml, nullptr, 1).incomplete) return 192;
        std::filesystem::remove_all(scanRoot);
    }
    std::filesystem::create_directories(root / "example");
    auto manifest = [&](bool network) {
        std::ofstream f(root / "example/module.json", std::ios::trunc);
        f << "{\"name\":\"Example\",\"version\":\"1\",\"permissions\":{\"project\":false,\"network\":" << (network ? "true" : "false") << ",\"processes\":false,\"settings\":false},\"commands\":[{\"title\":\"Hello\",\"insert\":\"hello\\n\"}]}";
    };
    std::string error; std::vector<Module> modules;
    manifest(false);
    if (!list(root, modules, error) || modules.size() != 1 || !modules[0].valid || modules[0].enabled || modules[0].commands[0].insert != "hello\n") return 1;
    if (!setEnabled(root, "example", true, error) || !list(root, modules, error) || !modules[0].enabled) return 2;
    manifest(true);
    if (!list(root, modules, error) || modules[0].enabled || !modules[0].network || setEnabled(root, "example", true, error)) return 3;
    if (!setEnabled(root, "example", false, error)) return 4;
    if (setEnabled(root, "../escape", true, error) || remove(root, "../escape", error)) return 5;
    { std::ofstream f(root / "example/module.json", std::ios::trunc); f << "{}"; }
    if (!list(root, modules, error) || modules[0].valid || setEnabled(root, "example", true, error)) return 6;
    if (!remove(root, "example", error) || std::filesystem::exists(root / "example")) return 7;
    const std::string candidate = R"({"name":"Starter","version":"1.0.0","permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Hello","insert":"hello"}]})";
    const auto candidatePreview = preview("starter", candidate);
    if (!candidatePreview.valid || candidatePreview.enabled || candidatePreview.commands.size() != 1) return 8;
    if (!install(root, "starter", candidate, error) || !list(root, modules, error) || modules.size() != 1 || modules[0].enabled) return 9;
    if (install(root, "starter", candidate, error) || error.empty()) return 10;
    if (install(root, "../escape", candidate, error) || install(root, "bad", "{}", error)) return 11;
    const auto unsupported = R"({"name":"Network","version":"1","permissions":{"project":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"x","insert":"x"}]})";
    if (install(root, "network", unsupported, error) || std::filesystem::exists(root / "network")) return 12;
    const auto binaryCommand = R"({"name":"Unsafe","version":"1","permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Insert","insert":"a\u0000b"}]})";
    if (preview("unsafe", binaryCommand).valid || install(root, "unsafe", binaryCommand, error) ||
        error.empty() || std::filesystem::exists(root / "unsafe")) return 19;
    std::vector<RegistryEntry> entries;
    if (!parseCatalog(R"({"modules":[{"id":"starter","package":"author/lucid-starter","version":"1.0.0","name":"Starter","description":"Handy snippets"}]})", entries, error) || entries.size() != 1 || entries[0].name != "Starter" || entries[0].description != "Handy snippets") return 13;
    if (!parseCatalog(R"({"modules":[{"id":"new","package":"author/new","version":"1","requires":["future-v9"]}]})", entries, error) || entries[0].requirements.size() != 1 || supportsCapability(entries[0].requirements[0])) return 34;
    if (parseCatalog(R"({"modules":[{"id":"new","package":"author/new","version":"1","requires":["x","x"]}]})", entries, error)) return 35;
    if (parseCatalog(R"({"modules":[{"id":"starter","package":"author/../escape","version":"1"}]})", entries, error) || !entries.empty()) return 14;
    if (parseCatalog(R"({"modules":[{"id":"a","package":"author/a","version":"1"},{"id":"a","package":"author/b","version":"1"}]})", entries, error) || !entries.empty()) return 15;
    std::string unused;
    if (fetchPackage("author/../bad", "1", "application/vnd.lucid.module.v1+json", unused, error)) return 16;
    const std::string digest = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    int requests = 0;
    const RegistryRequest transport = [&](std::string_view url, const std::vector<std::string>& headers, std::string& output, std::string&) {
        ++requests;
        if (url == "https://ghcr.io/token?scope=repository:author/starter:pull" && headers.empty()) output = R"({"token":"temporary-token"})";
        else if (url == "https://ghcr.io/v2/author/starter/manifests/1.0.0" && headers.size() == 2) output = "{\"layers\":[{\"mediaType\":\"application/vnd.lucid.module.v1+json\",\"digest\":\"sha256:" + digest + "\"}]}";
        else if (url == "https://ghcr.io/v2/author/starter/blobs/sha256:" + digest && headers.size() == 2) output = "abc";
        else return false;
        return true;
    };
    if (!fetchPackage("author/starter", "1.0.0", "application/vnd.lucid.module.v1+json", unused, error, transport) || unused != "abc" || requests != 3) return 17;
    const RegistryRequest corrupt = [&](std::string_view url, const std::vector<std::string>& headers, std::string& output, std::string& reason) {
        if (!transport(url, headers, output, reason)) return false;
        if (url.find("/blobs/") != std::string_view::npos) output = "tampered";
        return true;
    };
    if (fetchPackage("author/starter", "1.0.0", "application/vnd.lucid.module.v1+json", unused, error, corrupt) || !unused.empty()) return 18;
    const std::string apiEdit = R"({"name":"API editor","version":"1","requires":["api-collection-edit-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Edit","action":"edit-api-collection"}]})";
    const std::string apiEditNoCapability = R"({"name":"API editor","version":"1","permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Edit","action":"edit-api-collection"}]})";
    const std::string apiEditNoAction = R"({"name":"API editor","version":"1","requires":["api-collection-edit-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Text","insert":"x"}]})";
    if (!supportsCapability("api-collection-edit-v1") || !preview("api-edit", apiEdit).error.empty() || preview("api-edit", apiEditNoCapability).valid || preview("api-edit", apiEditNoAction).error.empty()) return 241;
    const std::string sqlColumns = R"({"name":"SQL","version":"1","requires":["sql-snapshot-v1","sql-columns-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Run","action":"sql-query"}]})";
    const std::string sqlColumnsNoAction = R"({"name":"SQL","version":"1","requires":["sql-columns-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Text","insert":"x"}]})";
    if (!supportsCapability("sql-columns-v1") || !preview("sql", sqlColumns).error.empty() || preview("sql", sqlColumnsNoAction).error.empty()) return 242;
    const std::string localizationEdit = R"({"name":"Localization","version":"1","requires":["localization-workspace-v1","localization-edit-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Compare","action":"compare-localization"}]})";
    const std::string localizationEditNoAction = R"({"name":"Localization","version":"1","requires":["localization-edit-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Text","insert":"x"}]})";
    if (!supportsCapability("localization-edit-v1") || !preview("localization", localizationEdit).error.empty() || preview("localization", localizationEditNoAction).error.empty()) return 244;
    const std::string sql = R"({"name":"SQL","version":"1","requires":["sql-snapshot-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Run","action":"sql-query"},{"title":"Plan","action":"sql-explain"}]})";
    if (!supportsCapability("sql-snapshot-v1") || !preview("sql", sql).error.empty() || !preview("sql", sql).valid) return 110;
    const std::string boundSql = R"({"name":"SQL","version":"1","requires":["sql-parameters-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Bound","action":"sql-query-bound"}]})";
    const std::string undeclaredBoundSql = R"({"name":"SQL","version":"1","permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Bound","action":"sql-query-bound"}]})";
    if (!supportsCapability("sql-parameters-v1") || !preview("sql-bound", boundSql).valid || preview("sql-bound", undeclaredBoundSql).valid) return 113;
    const std::string undeclaredSql = R"({"name":"SQL","version":"1","permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Run","action":"sql-query"}]})";
    if (preview("sql", undeclaredSql).valid) return 111;
    const std::string api = R"({"name":"API","version":"1","requires":["api-request-studio-v1"],"permissions":{"project":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"Requests","action":"open-api-requests"}]})";
    if (!supportsCapability("api-request-studio-v1") || !preview("api", api).valid || !preview("api", api).error.empty()) return 112;
    const std::string apiEnvironment = R"({"name":"API Profiles","version":"1","requires":["api-environments-v1"],"permissions":{"project":true,"network":true,"processes":false,"settings":false},"commands":[{"title":"Profiles","action":"open-api-environment"}]})";
    const std::string apiEnvironmentNoProject = R"({"name":"API Profiles","version":"1","requires":["api-environments-v1"],"permissions":{"project":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"Profiles","action":"open-api-environment"}]})";
    if (!supportsCapability("api-environments-v1") || !preview("api-env", apiEnvironment).valid || preview("api-env", apiEnvironmentNoProject).valid) return 113;
    const std::string apiSave = R"({"name":"API Save","version":"1","requires":["api-request-studio-v1","api-response-save-v1"],"permissions":{"project":true,"network":true,"processes":false,"settings":false},"commands":[{"title":"Requests","action":"open-api-requests"}]})";
    const std::string apiSaveNoProject = R"({"name":"API Save","version":"1","requires":["api-request-studio-v1","api-response-save-v1"],"permissions":{"project":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"Requests","action":"open-api-requests"}]})";
    const std::string apiAssertions = R"({"name":"API Assertions","version":"1","requires":["api-request-studio-v1","api-response-assertions-v1"],"permissions":{"project":true,"network":true,"processes":false,"settings":false},"commands":[{"title":"Requests","action":"open-api-requests"}]})";
    const std::string apiAssertionsNoProject = R"({"name":"API Assertions","version":"1","requires":["api-request-studio-v1","api-response-assertions-v1"],"permissions":{"project":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"Requests","action":"open-api-requests"}]})";
    if (!supportsCapability("api-response-assertions-v1") || !preview("api-assertions",apiAssertions).valid || preview("api-assertions",apiAssertionsNoProject).error.empty()) return 199;
    if (!supportsCapability("api-response-save-v1") || !preview("api-save",apiSave).valid || !preview("api-save",apiSave).error.empty() || preview("api-save",apiSaveNoProject).error.empty()) return 198;
    const std::string apiNoNetwork = R"({"name":"API","version":"1","requires":["api-request-studio-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Requests","action":"open-api-requests"}]})";
    if (preview("api", apiNoNetwork).valid) return 113;
    const std::string apiNoCapability = R"({"name":"API","version":"1","permissions":{"project":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"Requests","action":"open-api-requests"}]})";
    if (preview("api", apiNoCapability).valid) return 114;
    const std::string configuredContainer = R"({"name":"Container","version":"1","requires":["container-inspection-v1","container-project-actions-v1"],"permissions":{"project":true,"network":true,"processes":true,"settings":false},"commands":[{"title":"Inspect","action":"inspect-containers"},{"title":"Actions","action":"configured-container-actions"}]})";
    const std::string configuredNoProject = R"({"name":"Container","version":"1","requires":["container-project-actions-v1"],"permissions":{"project":false,"network":true,"processes":true,"settings":false},"commands":[{"title":"Actions","action":"configured-container-actions"}]})";
    const std::string configuredNoAction = R"({"name":"Container","version":"1","requires":["container-project-actions-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Text","insert":"x"}]})";
    if (!supportsCapability("container-project-actions-v1") || !preview("container", configuredContainer).error.empty() || preview("container", configuredNoProject).valid || preview("container", configuredNoAction).error.empty()) return 243;
    const std::string container = R"({"name":"Container","version":"1","requires":["container-inspection-v1"],"permissions":{"project":false,"network":true,"processes":true,"settings":false},"commands":[{"title":"Inspect","action":"inspect-containers"}]})";
    if (!supportsCapability("container-inspection-v1") || !preview("container", container).valid || !preview("container", container).error.empty()) return 120;
    const std::string containerWithoutProcess = R"({"name":"Container","version":"1","requires":["container-inspection-v1"],"permissions":{"project":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"Inspect","action":"inspect-containers"}]})";
    if (preview("container", containerWithoutProcess).valid) return 121;
    const std::string containerWithoutNetwork = R"({"name":"Container","version":"1","requires":["container-inspection-v1"],"permissions":{"project":false,"network":false,"processes":true,"settings":false},"commands":[{"title":"Inspect","action":"inspect-containers"}]})";
    if (preview("container", containerWithoutNetwork).valid) return 122;
    const std::string serial = R"({"name":"Serial","version":"1","requires":["microcontroller-serial-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false,"devices":true},"commands":[{"title":"Monitor","action":"serial-monitor"}]})";
    if (!preview("serial", serial).valid || !preview("serial", serial).devices || !preview("serial", serial).error.empty()) return 135;
    const std::string noDevice = R"({"name":"Serial","version":"1","requires":["microcontroller-serial-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Monitor","action":"serial-monitor"}]})";
    if (preview("serial", noDevice).valid) return 136;
    const std::string flash = R"({"name":"Flash","version":"1","requires":["openocd-flash-v1"],"permissions":{"project":true,"network":false,"processes":true,"settings":false,"devices":true},"commands":[{"title":"Flash","action":"flash-firmware"}]})";
    const std::string flashNoDevice = R"({"name":"Flash","version":"1","requires":["openocd-flash-v1"],"permissions":{"project":true,"network":false,"processes":true,"settings":false},"commands":[{"title":"Flash","action":"flash-firmware"}]})";
    const std::string flashNoCapability = R"({"name":"Flash","version":"1","permissions":{"project":true,"network":false,"processes":true,"settings":false,"devices":true},"commands":[{"title":"Flash","action":"flash-firmware"}]})";
    if (!supportsCapability("openocd-flash-v1") || !preview("flash", flash).valid || preview("flash", flashNoDevice).valid || preview("flash", flashNoCapability).valid) return 144;
    const std::string shader = R"({"name":"Shader","version":"1","requires":["shader-compile-v1"],"permissions":{"project":true,"network":false,"processes":true,"settings":false},"commands":[{"title":"Compile","action":"compile-shader"}]})";
    if (!supportsCapability("shader-compile-v1") || !preview("shader", shader).valid || !preview("shader", shader).error.empty()) return 130;
    const std::string shaderWithoutProcess = R"({"name":"Shader","version":"1","requires":["shader-compile-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Compile","action":"compile-shader"}]})";
    if (preview("shader", shaderWithoutProcess).valid) return 131;
    const std::string shaderIncludes = R"({"name":"Shader","version":"1","requires":["shader-compile-v1","shader-includes-v1"],"permissions":{"project":true,"network":false,"processes":true,"settings":false},"commands":[{"title":"Compile","action":"compile-shader"}]})";
    const std::string shaderIncludesNoAction = R"({"name":"Shader","version":"1","requires":["shader-includes-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Insert","insert":"hello"}]})";
    if (!supportsCapability("shader-includes-v1") || !preview("shader", shaderIncludes).error.empty() || preview("shader", shaderIncludesNoAction).error.empty()) return 235;
    const std::string spirvInspection = R"({"name":"Shader","version":"1","requires":["shader-compile-v1","shader-spirv-inspection-v1"],"permissions":{"project":true,"network":false,"processes":true,"settings":false},"commands":[{"title":"Compile","action":"compile-shader"}]})";
    const std::string spirvNoAction = R"({"name":"Shader","version":"1","requires":["shader-spirv-inspection-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Insert","insert":"hello"}]})";
    if (!supportsCapability("shader-spirv-inspection-v1") || !preview("shader", spirvInspection).error.empty() || preview("shader", spirvNoAction).error.empty()) return 236;
    const std::string shaderPreview = R"({"name":"Shader","version":"1","requires":["shader-compile-v1","shader-render-preview-v1"],"permissions":{"project":true,"network":false,"processes":true,"settings":false},"commands":[{"title":"Compile","action":"compile-shader"}]})";
    const std::string shaderPreviewNoAction = R"({"name":"Shader","version":"1","requires":["shader-render-preview-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Insert","insert":"hello"}]})";
    if (!supportsCapability("shader-render-preview-v1") || !preview("shader", shaderPreview).error.empty() || preview("shader", shaderPreviewNoAction).error.empty()) return 237;
    const std::string pullRequest = R"({"name":"Pull Request","version":"1","requires":["pull-request-companion-v1"],"permissions":{"project":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"Review","action":"fetch-pull-request"}]})";
    const std::string pullRequestNoNetwork = R"({"name":"Pull Request","version":"1","requires":["pull-request-companion-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Review","action":"fetch-pull-request"}]})";
    if (!supportsCapability("pull-request-companion-v1") || !preview("pull-request", pullRequest).valid || preview("pull-request", pullRequestNoNetwork).valid) return 123;
    const std::string review = R"({"name":"Review","version":"1","requires":["pull-request-companion-v1","pull-request-review-v1"],"permissions":{"project":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"Review","action":"fetch-pull-request"}]})";
    const std::string reviewNoAction = R"({"name":"Review","version":"1","requires":["pull-request-review-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Insert","insert":"hello"}]})";
    if (!supportsCapability("pull-request-review-v1") || !preview("review", review).error.empty() || preview("review", reviewNoAction).error.empty()) return 234;
    const std::string dependency = R"({"name":"Dependency","version":"1","requires":["dependency-risk-review-v1"],"permissions":{"project":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"Review","action":"review-dependency-risk"}]})";
    if (!supportsCapability("dependency-risk-review-v1") || !preview("dependency", dependency).valid || !preview("dependency", dependency).error.empty()) return 115;
    const std::string dependencyNoNetwork = R"({"name":"Dependency","version":"1","requires":["dependency-risk-review-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Review","action":"review-dependency-risk"}]})";
    if (preview("dependency", dependencyNoNetwork).valid) return 116;
    const std::string dependencyNoCapability = R"({"name":"Dependency","version":"1","permissions":{"project":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"Review","action":"review-dependency-risk"}]})";
    if (preview("dependency", dependencyNoCapability).valid) return 117;
    const std::string dependencyArtifact = R"({"name":"Dependency","version":"1","requires":["dependency-risk-review-v1","dependency-provenance-v1","dependency-artifact-verify-v1"],"permissions":{"project":true,"network":true,"processes":false,"settings":false},"commands":[{"title":"Review","action":"review-dependency-risk"}]})";
    const std::string dependencyArtifactNoProject = R"({"name":"Dependency","version":"1","requires":["dependency-risk-review-v1","dependency-provenance-v1","dependency-artifact-verify-v1"],"permissions":{"project":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"Review","action":"review-dependency-risk"}]})";
    if (!supportsCapability("dependency-artifact-verify-v1") || !preview("dependency", dependencyArtifact).error.empty() || preview("dependency", dependencyArtifactNoProject).error.empty()) return 245;
    const std::string artifactSha2 = R"({"name":"Dependency","version":"1","requires":["dependency-risk-review-v1","dependency-provenance-v1","dependency-artifact-verify-v1","dependency-artifact-sha2-v2"],"permissions":{"project":true,"network":true,"processes":false,"settings":false},"commands":[{"title":"Review","action":"review-dependency-risk"}]})";
    const std::string artifactSha2NoBase = R"({"name":"Dependency","version":"1","requires":["dependency-artifact-sha2-v2"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Text","insert":"x"}]})";
    if (!supportsCapability("dependency-artifact-sha2-v2") || !preview("dependency",artifactSha2).error.empty() || preview("dependency",artifactSha2NoBase).error.empty()) return 246;
    const std::string artifactGo = R"({"name":"Dependency","version":"1","requires":["dependency-risk-review-v1","dependency-provenance-v1","dependency-artifact-verify-v1","dependency-artifact-go-v1"],"permissions":{"project":true,"network":true,"processes":false,"settings":false},"commands":[{"title":"Review","action":"review-dependency-risk"}]})";
    const std::string artifactGoNoBase = R"({"name":"Dependency","version":"1","requires":["dependency-artifact-go-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Text","insert":"x"}]})";
    if (!supportsCapability("dependency-artifact-go-v1") || !preview("dependency",artifactGo).error.empty() || preview("dependency",artifactGoNoBase).error.empty()) return 247;
    const std::string dependencyGo = R"({"name":"Dependency","version":"1","requires":["dependency-risk-review-v1","dependency-risk-go-v1"],"permissions":{"project":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"Review","action":"review-dependency-risk"}]})";
    if (!supportsCapability("dependency-risk-go-v1") || !preview("dependency", dependencyGo).error.empty()) return 235;
    const std::string dependencyGoNoCommand = R"({"name":"Dependency","version":"1","requires":["dependency-risk-go-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Text","insert":"x"}]})";
    if (preview("dependency", dependencyGoNoCommand).error.empty()) return 236;
    const std::string dependencyCargo = R"({"name":"Dependency","version":"1","requires":["dependency-risk-review-v1","dependency-risk-cargo-v1"],"permissions":{"project":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"Review","action":"review-dependency-risk"}]})";
    if (!supportsCapability("dependency-risk-cargo-v1") || !preview("dependency", dependencyCargo).error.empty()) return 237;
    const std::string dependencyCargoNoCommand = R"({"name":"Dependency","version":"1","requires":["dependency-risk-cargo-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Text","insert":"x"}]})";
    if (preview("dependency", dependencyCargoNoCommand).error.empty()) return 238;
    const std::string dependencyProvenance = R"({"name":"Dependency","version":"1","requires":["dependency-risk-review-v1","dependency-provenance-v1"],"permissions":{"project":false,"network":true,"processes":false,"settings":false},"commands":[{"title":"Review","action":"review-dependency-risk"}]})";
    if (!supportsCapability("dependency-provenance-v1") || !preview("dependency", dependencyProvenance).error.empty()) return 239;
    const std::string provenanceNoReview = R"({"name":"Dependency","version":"1","requires":["dependency-provenance-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Text","insert":"x"}]})";
    if (preview("dependency", provenanceNoReview).error.empty()) return 240;
    const std::string visualConfig = R"({"name":"Visual","version":"1","requires":["visual-regression-config-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Compare","action":"compare-visual-configured"}]})";
    const std::string visualNoCapability = R"({"name":"Visual","version":"1","permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Compare","action":"compare-visual-configured"}]})";
    if (!supportsCapability("visual-regression-config-v1") || !preview("visual-config", visualConfig).valid || preview("visual-config", visualNoCapability).valid) return 141;
    const std::string pythonInstall = R"({"name":"Python","version":"1","requires":["python-environments-v2","python-pylsp-install-v1"],"permissions":{"project":true,"network":true,"processes":true,"settings":true},"commands":[{"title":"Manage","action":"manage-python-environments"},{"title":"Install","action":"install-python-pylsp"}]})";
    const std::string onlyPythonInstall = R"({"name":"Python","version":"1","requires":["python-pylsp-install-v1"],"permissions":{"project":true,"network":true,"processes":true,"settings":true},"commands":[{"title":"Install","action":"install-python-pylsp"}]})";
    const std::string onlyBoardProfile = R"({"name":"Board","version":"1","requires":["microcontroller-serial-v1"],"permissions":{"project":false,"network":false,"processes":false,"settings":false,"devices":true},"commands":[{"title":"Profile","action":"board-profile"}]})";
    if (!preview("only-python",onlyPythonInstall).valid || !preview("only-python",onlyPythonInstall).error.empty() || !preview("only-board",onlyBoardProfile).valid || !preview("only-board",onlyBoardProfile).error.empty()) return 196;
    const std::string pythonInstallNoNetwork = R"({"name":"Python","version":"1","requires":["python-environments-v2","python-pylsp-install-v1"],"permissions":{"project":true,"network":false,"processes":true,"settings":true},"commands":[{"title":"Manage","action":"manage-python-environments"},{"title":"Install","action":"install-python-pylsp"}]})";
    const std::string pythonInstallNoCapability = R"({"name":"Python","version":"1","requires":["python-environments-v2"],"permissions":{"project":true,"network":true,"processes":true,"settings":true},"commands":[{"title":"Manage","action":"manage-python-environments"},{"title":"Install","action":"install-python-pylsp"}]})";
    if (!supportsCapability("python-pylsp-install-v1") || !preview("python-install", pythonInstall).valid || preview("python-install", pythonInstallNoNetwork).valid || preview("python-install", pythonInstallNoCapability).valid) return 142;
    const std::string externalLinks = R"({"name":"Links","version":"1","requires":["markdown-external-links-v1"],"permissions":{"project":true,"network":true,"processes":false,"settings":false},"commands":[{"title":"Check","action":"markdown-external-links"}]})";
    const std::string externalNoNetwork = R"({"name":"Links","version":"1","requires":["markdown-external-links-v1"],"permissions":{"project":true,"network":false,"processes":false,"settings":false},"commands":[{"title":"Check","action":"markdown-external-links"}]})";
    const std::string externalNoCapability = R"({"name":"Links","version":"1","permissions":{"project":true,"network":true,"processes":false,"settings":false},"commands":[{"title":"Check","action":"markdown-external-links"}]})";
    if (!supportsCapability("markdown-external-links-v1") || !preview("external", externalLinks).valid || !preview("external", externalLinks).error.empty() || preview("external", externalNoNetwork).valid || preview("external", externalNoCapability).valid) return 242;
    std::filesystem::remove_all(root); return 0;
}
