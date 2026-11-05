#include "modules/PythonRuntime.hpp"
#include "modules/PythonEnvironments.hpp"
#include "modules/ModuleProcess.hpp"
#include <chrono>
#include <system_error>
namespace lucid::modules {
namespace fs = std::filesystem;
namespace {
bool runBounded(const fs::path& executable, const std::vector<std::string>& arguments,
                std::string& output, std::string& error, std::chrono::seconds timeout) {
    auto result = runModuleProcess(executable, arguments, timeout, 16384);
    // Process output is intentionally not included in errors: it can contain private data.
    if (result.timedOut) { error = "Python process timed out."; return false; }
    if (result.outputTruncated) { error = "Python process exceeded the output limit."; return false; }
    if (!result.error.empty()) { error = "Python process could not be run."; return false; }
    if (result.exitCode != 0) { error = "Python process failed (exit " + std::to_string(result.exitCode) + ")."; return false; }
    output = std::move(result.output); return true;
}
bool validationRunner(const fs::path& exe, const std::vector<std::string>& args, std::string& out, std::string& error) {
    return runBounded(exe, args, out, error, std::chrono::seconds(10));
}
bool creationRunner(const fs::path& exe, const std::vector<std::string>& args, std::string& out, std::string& error) {
    return runBounded(exe, args, out, error, std::chrono::seconds(120));
}
}
PythonRuntimeResult validatePythonRuntime(const fs::path& interpreter) {
    PythonRuntimeResult result; result.interpreter = interpreter;
    result.ok = validatePythonInterpreter(interpreter, validationRunner, result.version, result.error);
    return result;
}
PythonRuntimeResult createAndValidatePythonRuntime(const fs::path& projectRoot, const fs::path& interpreter, const fs::path& destination) {
    auto base = validatePythonRuntime(interpreter);
    if (!base.ok) return base;
    PythonRuntimeResult result;
    std::error_code ec;
    const auto before = fs::symlink_status(destination, ec);
    const bool absentBefore = ec == std::errc::no_such_file_or_directory || (!ec && before.type() == fs::file_type::not_found);
    if (!createPythonEnvironment(projectRoot, interpreter, destination, creationRunner, result.error)) {
        ec.clear(); auto after = fs::symlink_status(destination, ec);
        result.destinationMayRemain = absentBefore && !ec && after.type() != fs::file_type::not_found;
        return result;
    }
    result.destinationMayRemain = true;
#ifdef _WIN32
    result.interpreter = destination / "Scripts" / "python.exe";
#else
    result.interpreter = destination / "bin" / "python";
#endif
    result.ok = validatePythonInterpreter(result.interpreter, validationRunner, result.version, result.error);
    if (result.ok) result.destinationMayRemain = false;
    return result;
}
std::string_view pythonLanguageServerRequirement() noexcept { return "python-lsp-server[pyflakes,autopep8]==1.15.0"; }
PythonServerProvisionResult provisionPythonLanguageServer(const fs::path& interpreter, bool bootstrapPip,
                                                         const std::atomic_bool* cancel) {
    PythonServerProvisionResult result;
    std::error_code ec;
    if (!interpreter.is_absolute() || !fs::is_regular_file(interpreter, ec) || ec) {
        result.error = "Select an existing absolute Python interpreter."; return result;
    }
    auto execute = [&](std::string phase, const std::vector<std::string>& arguments,
                       std::chrono::seconds timeout, bool changesEnvironment) {
        result.phase = std::move(phase);
        if (cancel && cancel->load()) { result.cancelled = true; result.error = "Python server setup cancelled."; return false; }
        if (changesEnvironment) result.mayHaveChanged = true;
        const auto process = runModuleProcess(interpreter, arguments, timeout, 64U * 1024U, cancel);
        if (process.cancelled || (cancel && cancel->load())) { result.cancelled = true; result.error = "Python server setup cancelled."; return false; }
        if (process.timedOut) { result.error = "Python server setup timed out during " + result.phase + "."; return false; }
        if (process.outputTruncated) { result.error = "Python server setup output exceeded the limit during " + result.phase + "."; return false; }
        if (!process.error.empty()) { result.error = "Could not run Python server setup during " + result.phase + "."; return false; }
        if (process.exitCode != 0) { result.error = "Python server setup failed during " + result.phase + " (exit " + std::to_string(process.exitCode) + ")."; return false; }
        return true;
    };
    if (bootstrapPip) {
        if (!execute("bootstrap", {"-I", "-m", "ensurepip", "--upgrade"}, std::chrono::seconds(120), true)) return result;
    } else if (!execute("check pip", {"-I", "-m", "pip", "--version"}, std::chrono::seconds(15), false)) {
        if (!result.cancelled) result.error = "pip is unavailable or could not run; choose the explicit pip bootstrap option if appropriate.";
        return result;
    }
    if (!execute("install", {"-I", "-m", "pip", "--isolated", "install", "--no-user", "--disable-pip-version-check", "--no-input", "--only-binary=:all:", "--index-url", "https://pypi.org/simple", std::string(pythonLanguageServerRequirement())}, std::chrono::seconds(300), true)) return result;
    if (!execute("verify", {"-I", "-c", "import pylsp.__main__"}, std::chrono::seconds(15), false)) return result;
    result.ok = true; return result;
}
}
