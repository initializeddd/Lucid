#include "editor/diagnostics/CompilerDiagnostics.hpp"

#include <string>

int main() {
    using namespace lucid::editor;

    const std::string output =
        "/tmp/lucid.cpp:4:9:{4:9-4:12}: error: use of undeclared identifier 'foo' [-Wundeclared-var]\n"
        "/tmp/lucid.cpp:5:1: warning: unused variable 'x' [-Wunused-variable]\n"
        "/tmp/other.cpp:2:2: error: should be ignored\n"
        "/tmp/lucid.cpp:6:3: note: declared here\n";

    const auto diagnostics = parseCompilerDiagnostics(output, "/tmp/lucid.cpp");
    if (diagnostics.size() != 1) {
        return 1;
    }
    const Diagnostic& diagnostic = diagnostics.front();
    if (diagnostic.line != 3 || diagnostic.column != 9 ||
        diagnostic.length != 3 ||
        diagnostic.message != "use of undeclared identifier 'foo'" ||
        diagnostic.code != "-Wundeclared-var") {
        return 2;
    }

    CompilerRequest request;
    request.filePath = "/tmp/lucid_compiler_diagnostics_test.cpp";
    request.content = "int main() {\n    int value = missing_name;\n}\n";
    request.cpp = true;
    const CompilerResult result = runCompilerDiagnostics(request);
    if (!result.error.empty() || result.diagnostics.empty()) {
        return 3;
    }
    bool found = false;
    for (const Diagnostic& item : result.diagnostics) {
        if (item.message.find("missing_name") != std::string::npos) {
            found = true;
            break;
        }
    }
    if (!found) {
        return 4;
    }

    const CompilerResult clean = runCompilerDiagnostics(CompilerRequest{
        .filePath = "/tmp/lucid_compiler_diagnostics_clean.cpp",
        .projectRoot = {},
        .content = "int main() { return 0; }\n",
        .cpp = true});
    if (!clean.error.empty() || !clean.diagnostics.empty()) {
        return 5;
    }

    return 0;
}
