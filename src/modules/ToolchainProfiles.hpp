#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
struct ToolchainProfile {
    std::string name, targetTriple;
    std::filesystem::path cmakeToolchainFile, cCompiler, cppCompiler, sysroot;
};
// Input: {"profiles":[{"name":"...","target":"...",
// "toolchainFile":"absolute path", "cCompiler":"absolute path",
// "cppCompiler":"absolute path", "sysroot":"absolute path"}]}.
// Path fields are optional, but a toolchain file or compiler is required.
// Validation is lexical; it does not inspect paths or execute compilers.
[[nodiscard]] bool parseToolchainProfiles(std::string_view json, std::vector<ToolchainProfile>& profiles, std::string& error);
}
