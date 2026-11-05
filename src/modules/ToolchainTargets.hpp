#pragma once
#include "modules/ToolchainProfiles.hpp"
#include <string>
#include <string_view>
namespace lucid::modules {
enum class TargetEditKind { Add, Replace, Remove };
struct TargetEditDraft { std::string original, updated, error; };
// Creates a reviewable in-memory JSON replacement. Replace/remove require an exact
// existing name; add refuses collisions. No paths are opened and no writes occur.
// Callers must save explicitly using the original bytes as an expected snapshot.
[[nodiscard]] TargetEditDraft prepareTargetEdit(std::string_view source, TargetEditKind kind,
    std::string_view existingName, const ToolchainProfile& replacement = {});
}
