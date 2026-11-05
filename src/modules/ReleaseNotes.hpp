#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
struct ReleaseNoteEntry { std::string title, description, reference, category; bool selected = false; };
struct ReleaseNoteDraft { std::string markdown; std::size_t included = 0, omitted = 0; bool incomplete = false; };
// Builds an editable Markdown draft only from selected entries; categories are
// Features, Fixes, Changes, and Other. Unknown categories go into Other.
[[nodiscard]] ReleaseNoteDraft buildReleaseNotes(std::string_view version, std::string_view date,
    const std::vector<ReleaseNoteEntry>& entries, std::size_t maxEntries = 1000, std::size_t maxBytes = 1024 * 1024);
}
