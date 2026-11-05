#pragma once
#include <atomic>
#include <filesystem>
#include <string>
#include <string_view>
namespace lucid::modules {
struct PythonRuntimeResult {
    bool ok = false;
    // May be present after a failed creation or failed validation. Never deleted automatically.
    bool destinationMayRemain = false;
    std::filesystem::path interpreter;
    std::string version, error;
};
// These run the user-selected interpreter and can execute arbitrary code. Call only
// after explicit user intent, in a worker thread. No shell or network calls are made by Lucid.
[[nodiscard]] PythonRuntimeResult validatePythonRuntime(const std::filesystem::path& interpreter);
[[nodiscard]] PythonRuntimeResult createAndValidatePythonRuntime(const std::filesystem::path& projectRoot,
    const std::filesystem::path& interpreter, const std::filesystem::path& destination);

struct PythonServerProvisionResult {
    bool ok = false, cancelled = false, mayHaveChanged = false;
    std::string phase, error;
};
// Explicit user-authorized operation only. Runs the selected interpreter and
// pip, may access PyPI, execute downloaded code and change its environment;
// failed or cancelled runs may leave partial changes. bootstrapPip additionally
// runs ensurepip and must be disclosed by the caller. pip dependencies are
// resolved by pip and are not pinned or independently verified by Lucid.
[[nodiscard]] std::string_view pythonLanguageServerRequirement() noexcept;
[[nodiscard]] PythonServerProvisionResult provisionPythonLanguageServer(
    const std::filesystem::path& interpreter, bool bootstrapPip,
    const std::atomic_bool* cancel = nullptr);
}
