#pragma once
#include "workspace/Project.hpp"
#include <atomic>
#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
// Resolved means the project-contained target existed when checked; a later
// read may still fail, so consult incomplete and snapshots as well.
enum class ShaderIncludeStatus { Resolved, Cycle, Unresolved, Unsupported };
struct ShaderIncludeEdge {
    std::string source, target;
    std::size_t line = 0; // One-based source line.
    ShaderIncludeStatus status = ShaderIncludeStatus::Unresolved;
};
struct ShaderIncludeGraph {
    std::vector<ShaderIncludeEdge> edges;
    std::map<std::string, std::string> snapshots; // Exact bytes read; use for stale-source checks.
    bool incomplete = false, cancelled = false;
};
// Advisory static graph: quoted relative includes are followed within the project.
// Conditions and macro expansion are not evaluated; angle/macro/continued includes
// are reported unsupported. Reads only; no compiler or external file execution.
// Fixed bounds: 128 files, 1 MiB/file, 4 MiB total, depth 16, 1024 edges.
[[nodiscard]] ShaderIncludeGraph inspectShaderIncludes(const workspace::Project& project,
    std::string_view relativeFile, const std::atomic_bool* cancel = nullptr);
}
