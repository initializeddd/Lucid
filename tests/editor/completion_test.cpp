#include "editor/completion/Completion.hpp"
#include <set>
#include <string>
int main() {
    std::set<std::string> names;
    lucid::editor::collectFunctionNames(U"void greet (int x) { helper(x); }", names);
    lucid::editor::collectFunctionNames(U"if (x) greet(); foo_bar(42);", names);
    if (names.size() != 3 || !names.contains("greet") || !names.contains("helper") || !names.contains("foo_bar")) return 1;
    return 0;
}
