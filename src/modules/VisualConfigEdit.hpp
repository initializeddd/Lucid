#pragma once
#include "modules/VisualComparison.hpp"
#include <atomic>
#include <optional>
namespace lucid::modules {
enum class VisualConfigOperation { AddMask, RemoveMask, SetThreshold };
struct VisualConfigChange { VisualConfigOperation operation; VisualMask mask{}; std::size_t index=0; unsigned threshold=0; };
struct VisualComparisonCounts { std::size_t compared=0, masked=0, changed=0; };
struct VisualConfigDraft {
    std::string original, replacement;
    VisualReviewConfig before, after;
    VisualComparisonCounts beforeCounts, afterCounts;
};
// Pure preview: parses exact input, validates against both image snapshots,
// applies one operation, and compares before/after counts. The replacement is
// normalized JSON and does not preserve whitespace. Never writes files.
[[nodiscard]] bool prepareVisualConfigDraft(std::string_view original, const VisualImage& baseline,
    const VisualImage& candidate, const VisualConfigChange& change, VisualConfigDraft& draft,
    std::string& error, const std::atomic_bool* cancel=nullptr);
// Returns reviewed replacement only if current bytes still exactly match the
// original. The caller must also verify document identity/revision, image and
// project context, then apply as one undoable document edit; external file
// changes still require the editor's normal save conflict checks.
[[nodiscard]] bool applyVisualConfigDraft(const VisualConfigDraft& draft, std::string_view current,
    std::string& replacement, std::string& error);
}
