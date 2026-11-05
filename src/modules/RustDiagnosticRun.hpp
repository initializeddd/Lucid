#pragma once
#include "modules/RustOwnership.hpp"
#include "modules/ModuleProcess.hpp"
#include "workspace/Project.hpp"
#include <atomic>
#include <filesystem>
#include <string>
#include <vector>
namespace lucid::modules {
struct RustDiagnosticPlan { std::filesystem::path root, cargo; std::string manifestSnapshot, error; std::vector<std::string> arguments; };
struct RustDiagnosticRun { std::vector<RustOwnershipDiagnostic> diagnostics; ModuleProcessResult process; std::string error; bool incomplete=false; std::size_t ignoredLines=0; };
// Creates a read-only review plan for cargo check --locked --offline. No process.
[[nodiscard]] RustDiagnosticPlan prepareRustDiagnosticRun(const workspace::Project& project,const std::filesystem::path& cargo);
// Explicit confirmation executes Cargo and project build scripts/proc macros with
// user privileges; --offline constrains Cargo only, not those scripts or children.
// Output and runtime are bounded; commands are not sandboxed.
[[nodiscard]] RustDiagnosticRun runRustDiagnostic(const workspace::Project& project,const RustDiagnosticPlan& plan,bool confirmed,const std::atomic_bool* cancel=nullptr);
}
