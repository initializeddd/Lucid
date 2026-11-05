#include "editor/lsp/TextEdits.hpp"
#include "editor/json/Json.hpp"
#include "editor/lsp/Client.hpp"
#include "debug/Protocol.hpp"
#include "workspace/Project.hpp"
#include "workspace/recovery/History.hpp"
#include <chrono>
#include <filesystem>
#include <string>
int main() {
    using namespace lucid::editor;
    JsonError parseError; std::string output, error;
    auto signature = parseJson(R"json({"label":"f(😀, int value)","parameters":[{"label":[6,15]},{"label":"plain"},{"label":[3,4]},{"label":[0,999]}]})json", parseError);
    if (!signature || lsp::signatureParameterLabel(*signature, 0) != std::optional<std::string>("int value") ||
        lsp::signatureParameterLabel(*signature, 1) != std::optional<std::string>("plain") ||
        lsp::signatureParameterLabel(*signature, 2) || lsp::signatureParameterLabel(*signature, 3) || lsp::signatureParameterLabel(*signature, 4)) return 18;
    auto edits = parseJson(R"([{"range":{"start":{"line":0,"character":1},"end":{"line":0,"character":3}},"newText":"Z"}])", parseError);
    if (!edits || !lsp::applyTextEdits("a😀b\n", *edits, output, error) || output != "aZb\n") return 1;
    edits = parseJson(R"([{"range":{"start":{"line":0,"character":2},"end":{"line":0,"character":3}},"newText":"Z"}])", parseError);
    if (!edits || lsp::applyTextEdits("a😀b\n", *edits, output, error) || error.empty()) return 2;
    edits = parseJson(R"([{"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":2}},"newText":"x"},{"range":{"start":{"line":0,"character":1},"end":{"line":0,"character":3}},"newText":"y"}])", parseError);
    if (!edits || lsp::applyTextEdits("abcd", *edits, output, error) || error.empty()) return 3;
    std::string manySource, manyExpected, manyJson = "[";
    for (int i = 0; i < 5000; ++i) {
        manySource += "😀 old\n"; manyExpected += "😀 new\n";
        if (i) manyJson += ',';
        manyJson += "{\"range\":{\"start\":{\"line\":" + std::to_string(i) + ",\"character\":3},\"end\":{\"line\":" + std::to_string(i) + ",\"character\":6}},\"newText\":\"new\"}";
    }
    manyJson += ']'; edits = parseJson(manyJson, parseError);
    if (!edits || !lsp::applyTextEdits(manySource, *edits, output, error) || output != manyExpected) return 16;
    edits = parseJson(R"([{"range":{"start":{"line":0,"character":1},"end":{"line":0,"character":1}},"newText":"x"},{"range":{"start":{"line":0,"character":1},"end":{"line":0,"character":1}},"newText":"y"}])", parseError);
    if (!edits || lsp::applyTextEdits("ab", *edits, output, error) || !output.empty()) return 17;
    const auto root = std::filesystem::temp_directory_path() / ("lucid-workspace-edit-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    bool created = false; auto project = lucid::workspace::Project::openOrCreate(root, created, error);
    if (!created || !error.empty() || !project.writeFile("a.cpp", "foo\n", error) || !project.writeFile("b.cpp", "foo\r\n", error)) return 4;
    const auto a = lucid::debug::jsonString(lsp::fileUri(root / "a.cpp"));
    const auto b = lucid::debug::jsonString(lsp::fileUri(root / "b.cpp"));
    const std::string change = R"([{"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":3}},"newText":"bar"}])";
    auto workspace = parseJson("{\"changes\":{" + a + ":" + change + "," + b + ":" + change + "}}", parseError);
    std::vector<lsp::WorkspaceFileEdit> files;
    if (!workspace || !lsp::prepareWorkspaceEdits(project, *workspace, "a.cpp", "foo\n", 7, files, error) || files.size() != 2 ||
        files[0].updated != "bar\n" || files[1].updated != "bar\r\n") return 5;
    const auto reviewed = files; std::vector<std::string> changed;
    if (!project.writeFile("b.cpp", "external\n", error) || lsp::applyWorkspaceEdits(project, reviewed, root / ".history", changed, error) || !changed.empty()) return 10;
    std::string disk;
    if (!project.readFile("a.cpp", disk, error) || disk != "foo\n" || !project.writeFile("b.cpp", "foo\r\n", error)) return 11;
    auto unsafe = reviewed; unsafe[1].updated = std::string("bad\0data", 8);
    if (lsp::applyWorkspaceEdits(project, unsafe, root / ".history", changed, error) || !changed.empty()) return 12;
    if (!lsp::applyWorkspaceEdits(project, reviewed, root / ".history", changed, error) || changed.size() != 2 ||
        !project.readFile("a.cpp", disk, error) || disk != "bar\n" || !project.readFile("b.cpp", disk, error) || disk != "bar\r\n") return 13;
    std::vector<std::filesystem::path> backups;
    if (!lucid::workspace::recovery::listHistory(root / ".history", project.root().generic_string(), backups, error) || backups.size() != 2) return 15;
    if (!project.writeFile("a.cpp", "foo\n", error) || !project.writeFile("b.cpp", "foo\r\n", error)) return 14;
    workspace = parseJson("{\"documentChanges\":[{\"textDocument\":{\"uri\":" + a + ",\"version\":6},\"edits\":" + change + "}]}", parseError);
    if (!workspace || lsp::prepareWorkspaceEdits(project, *workspace, "a.cpp", "foo\n", 7, files, error) || !files.empty()) return 6;
    workspace = parseJson("{\"documentChanges\":[{\"textDocument\":{\"uri\":" + a + ",\"version\":7},\"edits\":" + change + "}]}", parseError);
    if (!workspace || !lsp::prepareWorkspaceEdits(project, *workspace, "a.cpp", "foo\n", 7, files, error) || files.size() != 1 || files[0].updated != "bar\n") return 9;
    const auto outside = lucid::debug::jsonString(lsp::fileUri(root.parent_path() / "outside.cpp"));
    workspace = parseJson("{\"changes\":{" + outside + ":" + change + "}}", parseError);
    if (!workspace || lsp::prepareWorkspaceEdits(project, *workspace, "a.cpp", "foo\n", 7, files, error) || !files.empty()) return 7;
    workspace = parseJson(R"({"documentChanges":[{"kind":"create","uri":"file:///tmp/x"}]})", parseError);
    if (!workspace || lsp::prepareWorkspaceEdits(project, *workspace, "a.cpp", "foo\n", 7, files, error)) return 8;
    std::filesystem::remove_all(root);
    return 0;
}
