#include "editor/search/TextSearch.hpp"
using namespace lucid::editor;
int main() {
    auto doc = Document::fromUtf8("alpha beta alpha\nβeta alpha");
    auto match = findText(doc.lines(), U"alpha", {0, 1});
    if (!match || match->line != 0 || match->column != 11) return 1;
    match = findText(doc.lines(), U"alpha", {1, 10});
    if (!match || match->line != 0 || match->column != 0) return 2;
    match = findText(doc.lines(), U"alpha", {0, 0}, true);
    if (!match || match->line != 1 || match->column != 5) return 3;
    match = findText(doc.lines(), U"βeta", {0, 0});
    if (!match || match->line != 1 || match->column != 0) return 4;
    if (findText(doc.lines(), U"", {0, 0}) || findText(doc.lines(), U"absent", {0, 0})) return 5;
    const auto repeated = Document::fromUtf8("aaaa");
    match = findText(repeated.lines(), U"aa", {0, 1});
    if (!match || match->column != 1) return 6;
    match = findText(repeated.lines(), U"aa", {0, 1}, true);
    if (!match || match->column != 0) return 7;
    return 0;
}
