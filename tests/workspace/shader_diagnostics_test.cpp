#include "modules/ShaderDiagnostics.hpp"
#include "modules/ShaderCompiler.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc, char** argv) {
    using namespace lucid::modules;
    if (argc > 1 && std::string_view(argv[1]) == "-V") {
        const auto utf8Path = [](const char* value) { const std::string text(value); return std::filesystem::path(std::u8string(text.begin(), text.end())); };
        if (argc != 7 || std::string_view(argv[2]) != "-S" || std::string_view(argv[3]) != "vert" || std::string_view(argv[4]) != "-o") return 99;
        const std::string_view output(argv[5]);
        if (output != "/dev/null" && output != "NUL") {
            std::ifstream source(utf8Path(argv[6]), std::ios::binary);
            const std::string contents((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
            if (contents.find("FAKE_FAILURE") != std::string::npos) return 1;
#if !defined(_WIN32)
            if (contents.find("FAKE_SYMLINK") != std::string::npos) { std::error_code ec; std::filesystem::create_symlink(std::filesystem::u8path(argv[6]), std::filesystem::u8path(argv[5]), ec); return ec ? 1 : 0; }
#endif
            if (contents.find("FAKE_MUTATE") != std::string::npos) { std::ofstream changed(utf8Path(argv[6]), std::ios::binary | std::ios::trunc); changed << "changed"; }
            std::ofstream artifact(utf8Path(argv[5]), std::ios::binary);
            if (contents.find("FAKE_INVALID") != std::string::npos) artifact << "invalid";
            else { const char spv[20] = {'\x03', '\x02', '\x23', '\x07'}; artifact.write(spv, sizeof spv); }
            return artifact ? 0 : 1;
        }
        std::cout << "ERROR: 0:2: invalid expression\n"; return 1;
    }
    const std::string text = "ERROR: 0:12: 'x' : syntax error\nWARNING: C:\\shaders\\a.vert:4: deprecated\n/dir/test.metal:7:13: error: bad type\nC:\\dir\\test.metal:8: warning: unused\nsource code\n  ^\n";
    auto r = parseShaderDiagnostics(text);
    if (r.incomplete || r.scannedLines != 6 || r.unparsedLines != 2 || r.diagnostics.size() != 4) return 1;
    if (r.diagnostics[0].path != "0" || r.diagnostics[0].line != 12 || r.diagnostics[1].path != "C:\\shaders\\a.vert" || r.diagnostics[2].column != 13 || r.diagnostics[3].line != 8 || r.diagnostics[3].column != 0) return 2;
    r = parseShaderDiagnostics(text, 8); if (!r.incomplete || r.scannedLines) return 3;
    r = parseShaderDiagnostics(text, 10000, 1); if (!r.incomplete || r.scannedLines != 1) return 4;
    r = parseShaderDiagnostics(text, 10000, 100, 1); if (!r.incomplete || r.diagnostics.size() != 1 || r.unparsedLines != 2) return 5;
    r = parseShaderDiagnostics("a:0:3: error: invalid\na:999999999999999999999999: error: invalid\n"); if (!r.diagnostics.empty() || r.unparsedLines != 2) return 6;
    namespace fs = std::filesystem;
    auto root = fs::temp_directory_path() / ("lucid-shader-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    { std::ofstream file(root / "scene.vert"); file << "#version 450\nvoid main(){gl_Position=vec4(0.0);}\n"; }
    bool created = false; std::string error;
    auto project = lucid::workspace::Project::openOrCreate(root, created, error, false, false);
    auto self = fs::absolute(argv[0]);
    auto result = compileGlsl(project, self, "scene.vert", "vert");
    const std::u8string unicodeShader = u8"着色器-é.vert";
    const std::string unicodeName(unicodeShader.begin(), unicodeShader.end());
    if (!project.writeFile(unicodeName, "#version 450\nvoid main(){}\n", error, nullptr, true)) return 24;
    const auto unicodeResult = compileGlsl(project, self, unicodeName, "vert");
    if (!unicodeResult.completed || unicodeResult.process.exitCode != 1 || unicodeResult.diagnostics.diagnostics.size() != 1) return 25;
    auto changed = compileGlsl(project, self, "scene.vert", "vert", std::chrono::seconds(5), nullptr, "different editor buffer");
    if (changed.completed || changed.error.find("changed on disk") == std::string::npos) return 12;
    if (!result.completed || result.succeeded || result.process.exitCode != 1 || result.diagnostics.diagnostics.size() != 1 || result.diagnostics.diagnostics[0].line != 2) return 7;
    if (compileGlsl(project, self, "scene.vert", "-bad").completed || compileGlsl(project, self, "../scene.vert", "vert").completed || compileGlsl(project, "relative", "scene.vert", "vert").completed) return 8;
    std::atomic_bool cancel{true}; if (compileGlsl(project, self, "scene.vert", "vert", std::chrono::seconds(5), &cancel).completed) return 9;
    auto artifact = compileGlslArtifact(project, self, "scene.vert", "vert", "scene.spv");
    if (!artifact.completed || !artifact.succeeded || fs::file_size(root / "scene.spv") != 20) return 13;
    const std::string spv = std::string("\x03\x02\x23\x07", 4) + std::string(16, '\0');
    if (compileGlslArtifact(project, self, "scene.vert", "vert", "scene.spv").succeeded) return 14;
    if (compileGlslArtifact(project, self, "scene.vert", "vert", "scene.spv", "wrong").succeeded) return 15;
    if (!compileGlslArtifact(project, self, "scene.vert", "vert", "scene.spv", spv).succeeded) return 16;
    if (compileGlslArtifact(project, self, "scene.vert", "vert", "scene.vert", "anything").succeeded) return 17;
    { std::ofstream file(root / "scene.vert"); file << "FAKE_INVALID"; }
    if (compileGlslArtifact(project, self, "scene.vert", "vert", "scene.spv", spv).succeeded || fs::file_size(root / "scene.spv") != 20) return 18;
    { std::ofstream file(root / "scene.vert"); file << "FAKE_FAILURE"; }
    auto fail = compileGlslArtifact(project, self, "scene.vert", "vert", "scene.spv", spv);
    if (!fail.completed || fail.succeeded || fs::file_size(root / "scene.spv") != 20) return 19;
    if (compileGlslArtifact(project, self, "scene.vert", "vert", "cancel.spv", std::nullopt, std::chrono::seconds(5), &cancel).succeeded || fs::exists(root / "cancel.spv")) return 20;
    { std::ofstream file(root / "scene.vert"); file << "FAKE_MUTATE"; }
    if (compileGlslArtifact(project, self, "scene.vert", "vert", "scene.spv", spv).succeeded || fs::file_size(root / "scene.spv") != 20) return 22;
#if !defined(_WIN32)
    { std::ofstream file(root / "scene.vert"); file << "FAKE_SYMLINK"; }
    if (compileGlslArtifact(project, self, "scene.vert", "vert", "scene.spv", spv).succeeded || fs::file_size(root / "scene.spv") != 20) return 23;
#endif
    if (const char* glslang = std::getenv("LUCID_TEST_GLSLANG")) {
        { std::ofstream file(root / "scene.vert"); file << "#version 450\nvoid main(){gl_Position=vec4(0.0);}\n"; }
        auto valid = compileGlsl(project, glslang, "scene.vert", "vert");
        if (!valid.completed || !valid.succeeded) { std::cerr << valid.error << '\n' << valid.process.output; return 10; }
        auto realArtifact = compileGlslArtifact(project, glslang, "scene.vert", "vert", "real.spv");
        if (!realArtifact.succeeded || fs::file_size(root / "real.spv") < 20) return 21;
        { std::ofstream file(root / "scene.vert"); file << "#version 450\nvoid main(){syntax error;}\n"; }
        auto invalid = compileGlsl(project, glslang, "scene.vert", "vert");
        if (!invalid.completed || invalid.succeeded || invalid.diagnostics.diagnostics.empty()) return 11;
    }
    fs::remove_all(root);
    return 0;
}
