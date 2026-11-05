#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::modules {
enum class PatchLineKind { Context, Added, Removed, NoNewline };
struct PatchLine {
    PatchLineKind kind = PatchLineKind::Context;
    std::string text;
    std::size_t oldLine = 0, newLine = 0; // One-based; zero means not present.
};
struct PatchHunk { std::string heading; std::vector<PatchLine> lines; };
// Parses GitHub's per-file unified patch, without opening or changing files.
// Incomplete/truncated hunks are rejected, not presented as a complete diff.
[[nodiscard]] bool parsePullRequestPatch(std::string_view text, std::vector<PatchHunk>& hunks, std::string& error);
}
