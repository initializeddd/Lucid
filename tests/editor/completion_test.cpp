#include "editor/completion/Completion.hpp"
#include <set>
#include <string>
int main() {
    std::set<std::string> names;
    lucid::editor::collectFunctionNames(U"void greet (int x) { helper(x); }", names);
    lucid::editor::collectFunctionNames(U"if (x) greet(); foo_bar(42);", names);
    if (names.size() != 3 || !names.contains("greet") || !names.contains("helper") || !names.contains("foo_bar")) return 1;
    lucid::editor::collectFunctionNames(U"void gr\u00fc\u00dfen() { \u03b2eta(); }", names);
    if (!names.contains("gr\xc3\xbc\xc3\x9f" "en") || !names.contains("\xce\xb2" "eta")) return 2;
    if (!lucid::editor::isCompletionIdentifierUtf8("gr\xc3\xbc\xc3\x9f" "en") ||
        lucid::editor::isCompletionIdentifierUtf8("two words") ||
        lucid::editor::isCompletionIdentifierUtf8(std::string("a\xff", 2))) return 3;
    if (lucid::editor::isCompletionIdentifierUtf8("a\xc2\xa0" "b") ||
        lucid::editor::isCompletionIdentifier(U'\u2003') ||
        lucid::editor::isCompletionIdentifier(U'\u3000') ||
        lucid::editor::isCompletionIdentifier(U'\u0085')) return 4;
    return 0;
}
