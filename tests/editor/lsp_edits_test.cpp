#include "editor/lsp/TextEdits.hpp"
#include "editor/json/Json.hpp"
#include <string>
int main() {
    using namespace lucid::editor;
    JsonError parseError; std::string output, error;
    auto edits = parseJson(R"([{"range":{"start":{"line":0,"character":1},"end":{"line":0,"character":3}},"newText":"Z"}])", parseError);
    if (!edits || !lsp::applyTextEdits("a😀b\n", *edits, output, error) || output != "aZb\n") return 1;
    edits = parseJson(R"([{"range":{"start":{"line":0,"character":2},"end":{"line":0,"character":3}},"newText":"Z"}])", parseError);
    if (!edits || lsp::applyTextEdits("a😀b\n", *edits, output, error) || error.empty()) return 2;
    edits = parseJson(R"([{"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":2}},"newText":"x"},{"range":{"start":{"line":0,"character":1},"end":{"line":0,"character":3}},"newText":"y"}])", parseError);
    if (!edits || lsp::applyTextEdits("abcd", *edits, output, error) || error.empty()) return 3;
    return 0;
}
