#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace lucid::modules {
struct PythonEnvironment {
    std::filesystem::path interpreter;
    std::filesystem::path environmentRoot; // Empty for a standalone interpreter.
    std::string version; // Metadata only; may be empty or stale.
    bool projectLocal = false;
};
struct PythonEnvironmentSearch {
    std::filesystem::path projectRoot;
    // Each root contains environment directories (for example ~/.virtualenvs).
    std::vector<std::filesystem::path> environmentRoots;
    // Directories to search for standalone Python executables, in priority order.
    std::vector<std::filesystem::path> interpreterDirectories;
};
// Populate standard user environment roots and PATH without running any programs.
[[nodiscard]] PythonEnvironmentSearch defaultPythonEnvironmentSearch(const std::filesystem::path& projectRoot);
// Bounded, best-effort discovery. Never follows directory symlinks during enumeration or runs interpreters.
[[nodiscard]] std::vector<PythonEnvironment> discoverPythonEnvironments(const PythonEnvironmentSearch& search);
}

namespace lucid::modules {
// The runner must launch executable directly with separate arguments, without a shell,
// close stdin, bound output and execution time, and return false on launch, timeout,
// or nonzero exit. Arguments are UTF-8, including paths; executable is a native path.
using PythonProcessRunner = bool (*)(const std::filesystem::path& executable,
                                    const std::vector<std::string>& arguments,
                                    std::string& output, std::string& error);
// Explicitly executes the selected interpreter in isolated mode. Version is reported by it.
[[nodiscard]] bool validatePythonInterpreter(const std::filesystem::path& interpreter,
                                             PythonProcessRunner runner, std::string& version,
                                             std::string& error);
// Explicitly creates a new, absent destination inside projectRoot. Never installs pip.
// Caller must obtain user intent and run this potentially long operation off the UI thread.
// The destination is reserved first; failures can leave a partial directory. This is not
// a security boundary against another process modifying the project concurrently.
[[nodiscard]] bool createPythonEnvironment(const std::filesystem::path& projectRoot,
                                           const std::filesystem::path& interpreter,
                                           const std::filesystem::path& destination,
                                           PythonProcessRunner runner, std::string& error);
}
