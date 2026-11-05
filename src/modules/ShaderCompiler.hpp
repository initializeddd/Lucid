#pragma once
#include "modules/ModuleProcess.hpp"
#include "modules/ShaderDiagnostics.hpp"
#include "workspace/Project.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace lucid::modules {
struct ShaderCompilation {
    ModuleProcessResult process;
    ShaderDiagnostics diagnostics;
    std::string error;
    bool completed = false;
    bool succeeded = false;
};
// Explicitly invokes the user-selected glslangValidator on one saved project
// file, compiling Vulkan SPIR-V to the OS null device. No shell is involved.
// This executes the selected program, which can read files and have side effects;
// the caller must obtain consent. This is not a sandbox. Stage is one of vert,
// frag, geom, tesc, tese or comp. Output and time are bounded. A changing source
// is rejected after the run, although change-and-restore races cannot be detected.
[[nodiscard]] ShaderCompilation compileGlsl(const workspace::Project& project,
    const std::filesystem::path& executable, std::string_view relativeFile,
    std::string_view stage, std::chrono::milliseconds timeout = std::chrono::seconds(30),
    const std::atomic_bool* cancel = nullptr, std::optional<std::string_view> expectedSource = std::nullopt);

// Explicitly compile to a private temporary SPIR-V file and save to a project
// path only after success and source revalidation. If expectedOutput is absent,
// creation is exclusive; if present, replacement requires matching those bytes.
// The selected compiler is trusted and may have arbitrary side effects. Files can
// still change between final checks and replacement by another process.
[[nodiscard]] ShaderCompilation compileGlslArtifact(workspace::Project& project,
    const std::filesystem::path& executable, std::string_view relativeFile,
    std::string_view stage, std::string_view relativeOutput,
    std::optional<std::string_view> expectedOutput = std::nullopt,
    std::chrono::milliseconds timeout = std::chrono::seconds(30),
    const std::atomic_bool* cancel = nullptr, std::optional<std::string_view> expectedSource = std::nullopt);
}
