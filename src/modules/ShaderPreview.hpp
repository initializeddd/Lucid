#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
struct ShaderPreviewImage { std::uint32_t width=0,height=0; std::vector<unsigned char> rgb; };
// Runs a trusted, packaged helper on a caller-staged immutable SPIR-V artifact.
// Only after explicit confirmation: the shader executes GPU code and may hang or
// harm the driver/device. A helper process and timeout are not a GPU sandbox.
// The caller must stage bytes in a private directory, trust the helper executable,
// and ensure the artifact does not change before/during its read. No automatic
// project/network access control is provided. Bounds: 512x512, 30 s, exact PPM.
[[nodiscard]] bool runShaderPreviewHelper(const std::filesystem::path& helper,
    const std::filesystem::path& stagedArtifact, std::uint32_t width, std::uint32_t height,
    bool confirmed, ShaderPreviewImage& output, std::string& error,
    std::chrono::milliseconds timeout=std::chrono::seconds(15), const std::atomic_bool* cancel=nullptr);
// Copies a bounded, reviewed SPIR-V snapshot into an owner-private temporary
// directory before invoking the helper. Other processes running as the same
// user can still interfere; this is not an immutable or sandboxed boundary.
[[nodiscard]] bool runStagedShaderPreview(const std::filesystem::path& helper,
    std::string_view shaderBytes, std::uint32_t width, std::uint32_t height, bool confirmed,
    ShaderPreviewImage& output, std::string& error,
    std::chrono::milliseconds timeout=std::chrono::seconds(15), const std::atomic_bool* cancel=nullptr);
}
