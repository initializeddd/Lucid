#include "editor/diagnostics/Diagnostics.hpp"

#include <string>
#include <vector>

namespace {

using lucid::editor::Diagnostic;
using lucid::editor::diagnoseCpp;

[[nodiscard]] bool hasMessage(const std::vector<Diagnostic>& diagnostics,
                              std::string_view message) {
    for (const Diagnostic& diagnostic : diagnostics) {
        if (diagnostic.message == message) {
            return true;
        }
    }
    return false;
}

} // namespace

int main() {
    {
        const std::vector<std::u32string> lines = {
            U"#include <string>",
            U"int main() {",
            U"    const char* text = \"nice \\\"quote\\\"\";",
            U"    int value = 100'000;",
            U"    // ignore ( unbalanced",
            U"    return value;",
            U"}",
        };
        if (!diagnoseCpp(lines).empty()) {
            return 1;
        }
    }

    {
        const std::vector<std::u32string> lines = {U"const char* text = \"oops;"};
        const std::vector<Diagnostic> diagnostics = diagnoseCpp(lines);
        if (diagnostics.size() != 1 ||
            diagnostics[0].message != "Unterminated string literal" ||
            diagnostics[0].column != 20) {
            return 2;
        }
    }

    {
        const std::vector<std::u32string> lines = {U"int value = 1; /* oops"};
        const std::vector<Diagnostic> diagnostics = diagnoseCpp(lines);
        if (diagnostics.size() != 1 ||
            diagnostics[0].message != "Unterminated block comment") {
            return 3;
        }
    }

    {
        const std::vector<std::u32string> lines = {
            U"void f() {",
            U"    if (true) {",
            U"    }",
        };
        const std::vector<Diagnostic> diagnostics = diagnoseCpp(lines);
        if (diagnostics.size() != 1 ||
            !hasMessage(diagnostics, "Unclosed '{'")) {
            return 4;
        }
    }

    {
        const std::vector<std::u32string> lines = {U"void f(] {"};
        const std::vector<Diagnostic> diagnostics = diagnoseCpp(lines);
        if (!hasMessage(diagnostics, "Mismatched ']', expected ')'")) {
            return 5;
        }
    }

    {
        const std::vector<std::u32string> lines = {U"#include stdio"};
        const std::vector<Diagnostic> diagnostics = diagnoseCpp(lines);
        if (diagnostics.size() != 1 ||
            diagnostics[0].message != "Malformed #include") {
            return 6;
        }
    }

    {
        const std::vector<std::u32string> lines = {U"int value = 'a';"};
        if (!diagnoseCpp(lines).empty()) {
            return 7;
        }
    }

    {
        const std::vector<std::u32string> lines = {
            U"int value = 1;",
            U"#include stdio",
        };
        const std::vector<Diagnostic> diagnostics = diagnoseCpp(lines);
        if (diagnostics.size() != 1 || diagnostics[0].line != 1 ||
            diagnostics[0].column != 10 ||
            diagnostics[0].message != "Malformed #include") {
            return 8;
        }
    }

    return 0;
}
