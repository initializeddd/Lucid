#include "modules/ShaderCompiler.hpp"
#include <algorithm>
#include <array>
#include <system_error>

namespace lucid::modules {
ShaderCompilation compileGlsl(const workspace::Project& project,
    const std::filesystem::path& executable, std::string_view relativeFile,
    std::string_view stage, std::chrono::milliseconds timeout, const std::atomic_bool* cancel, std::optional<std::string_view> expectedSource) {
    ShaderCompilation result;
    constexpr std::array<std::string_view, 6> stages{"vert", "frag", "geom", "tesc", "tese", "comp"};
    if (std::find(stages.begin(), stages.end(), stage) == stages.end()) { result.error = "Unsupported shader stage"; return result; }
    std::error_code ec;
    if (!executable.is_absolute() || !std::filesystem::is_regular_file(executable, ec) || ec || timeout.count() <= 0) {
        result.error = "Select an absolute compiler executable and a positive timeout"; return result;
    }
    // Project::resolve performs native UTF-8 conversion and rejects rooted or
    // escaping paths. Avoid constructing a narrow path here on Windows.
    if (relativeFile.empty() || relativeFile.find('\0') != std::string_view::npos) {
        result.error = "Select a project-relative shader file"; return result;
    }
    std::string before, after, readError;
    constexpr std::size_t maxSource = 1024U * 1024U;
    if (!project.readFileBounded(relativeFile, before, readError, maxSource, cancel)) { result.error = "Cannot read shader: " + readError; return result; }
    if (expectedSource && before != *expectedSource) { result.error = "Shader changed on disk; reload before compiling"; return result; }
    const auto source = project.resolve(relativeFile, readError);
    if (source.empty() || !source.is_absolute()) { result.error = "Cannot resolve shader: " + readError; return result; }
#if defined(_WIN32)
    constexpr auto nullDevice = "NUL";
#else
    constexpr auto nullDevice = "/dev/null";
#endif
    const auto path = source.u8string();
    result.process = runModuleProcess(executable, {"-V", "-S", std::string(stage), "-o", nullDevice,
        std::string(path.begin(), path.end())}, timeout, 1024U * 1024U, cancel);
    result.diagnostics = parseShaderDiagnostics(result.process.output);
    if (!result.process.error.empty()) result.error = "Compiler process failed: " + result.process.error;
    else if (result.process.cancelled) result.error = "Shader compilation cancelled";
    else if (result.process.timedOut) result.error = "Shader compilation timed out";
    else if (result.process.outputTruncated) result.error = "Shader compiler output exceeded the limit";
    else if (!project.readFileBounded(relativeFile, after, readError, maxSource, cancel) || before != after)
        result.error = "Shader changed or could not be read after compilation";
    else { result.completed = true; result.succeeded = result.process.exitCode == 0; }
    return result;
}
}
