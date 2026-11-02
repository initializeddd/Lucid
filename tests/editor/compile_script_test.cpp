#include "editor/compile/CompileScript.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace {

using lucid::editor::CompilePlatform;
using lucid::editor::CompileScript;
using lucid::editor::currentCompilePlatform;
using lucid::editor::expandCompileVariables;
using lucid::editor::findCompileFunctionAt;
using lucid::editor::parseCompilePlatform;
using lucid::editor::parseCompileScript;
using lucid::editor::shellQuote;

} // namespace

int main() {
    const std::vector<std::u32string> lines = {
        U"#include <cstdio>",
        U"/*",
        U"@build:macOS",
        U"$echo %project%",
        U"$make -C %project%",
        U"@end",
        U"@windowsOnly:windows",
        U"$echo nope",
        U"@end",
        U"*/",
        U"int main() { return 0; }",
    };

    const CompileScript script = parseCompileScript(lines);
    if (script.functions.size() != 2) {
        return 1;
    }

    const auto& build = script.functions[0];
    if (build.name != "build" || build.platform != CompilePlatform::MacOS ||
        build.definitionLine != 2 || build.endLine != 5 ||
        build.directiveColumn != 0 || build.directiveLength != 12 ||
        build.commands.size() != 2) {
        return 2;
    }
    if (build.commands[0].text != "echo %project%" ||
        build.commands[0].line != 3 ||
        build.commands[1].text != "make -C %project%") {
        return 3;
    }

    const auto& windows = script.functions[1];
    if (windows.name != "windowsOnly" ||
        windows.platform != CompilePlatform::Windows ||
        windows.definitionLine != 6 || windows.endLine != 8) {
        return 4;
    }
    if (currentCompilePlatform() != CompilePlatform::Windows &&
        windows.runsHere()) {
        return 5;
    }

    if (findCompileFunctionAt(script, 0) != nullptr ||
        findCompileFunctionAt(script, 4) != &build ||
        findCompileFunctionAt(script, 7) != &windows) {
        return 6;
    }

    const std::unordered_map<std::string, std::string> variables = {
        {"project", "/tmp/a b"},
        {"file", "/tmp/a b/main.cpp"},
    };
    if (expandCompileVariables("%project%/build", variables) != "/tmp/a b/build" ||
        expandCompileVariables("%file% %missing%", variables) !=
            "/tmp/a b/main.cpp %missing%" ||
        expandCompileVariables("%project%/%file%", variables) !=
            "/tmp/a b//tmp/a b/main.cpp") {
        return 7;
    }

    if (parseCompilePlatform("macOS") != CompilePlatform::MacOS ||
        parseCompilePlatform("WIN32") != CompilePlatform::Windows ||
        parseCompilePlatform("linux") != CompilePlatform::Linux ||
        parseCompilePlatform("unknown") != CompilePlatform::Any) {
        return 8;
    }

    if (shellQuote("plain") != "'plain'" ||
        shellQuote("a'b") != "'a'\\''b'") {
        return 9;
    }

    const CompileScript empty = parseCompileScript({U"/* regular comment */"});
    if (!empty.functions.empty()) {
        return 10;
    }

    const CompileScript unclosed = parseCompileScript(
        {U"/*", U"@quick", U"$echo hi", U"*/"});
    if (unclosed.functions.size() != 1 ||
        unclosed.functions[0].endLine != 3) {
        return 11;
    }

    return 0;
}
