#pragma once
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::modules {
struct LocalizationEntry {
    struct PlaceholderCount { std::string name; std::size_t source = 0, translation = 0; };
    std::string key; // JSON Pointer, with ~0 and ~1 escaping.
    bool inSource = false, inTranslation = false;
    std::string sourceText, translationText; // Full text; bounded by the 1 MiB per-file limit.
    std::size_t sourceOffset = 0, translationOffset = 0; // Value byte offsets in the compared snapshots.
    std::size_t sourceLength = 0, translationLength = 0; // Unicode code points, not rendered width.
    bool sourceHasRtl = false, translationHasRtl = false; // Heuristic script ranges, not direction or language detection.
    bool sourceLong = false, translationLong = false; // More than 120 Unicode code points.
    // Simple {identifier} and {0} placeholders only; not ICU message validation.
    std::vector<std::string> missingPlaceholders, extraPlaceholders;
    std::vector<PlaceholderCount> differingPlaceholderCounts;
};
// Compare JSON text already read by the caller. Each input is limited to 1 MiB.
[[nodiscard]] bool compareLocalizationText(std::string_view source, std::string_view translation,
    std::vector<LocalizationEntry>& entries, std::string& error);
// Compare two project-contained JSON object files. Objects and arrays may contain objects, arrays, or strings only. Array indexes are JSON Pointer segments.
// On failure, entries is empty. This function never writes files or accesses the network.
[[nodiscard]] bool compareLocalizationFiles(const std::filesystem::path& projectRoot,
    const std::filesystem::path& source, const std::filesystem::path& translation,
    std::vector<LocalizationEntry>& entries, std::string& error);

struct LocalizationEditPlan {
    std::string sourceSnapshot, translationSnapshot, pointer, value, updatedTranslation;
    LocalizationEntry preview;
    bool inserted = false;
};
// Prepare an edit to a source key's translation, using exact compared snapshots.
// A missing leaf can be inserted only into an already existing JSON object;
// missing parents and array growth are deliberately not created implicitly.
// The preview flags simple placeholder differences, not full ICU semantics.
[[nodiscard]] bool prepareLocalizationEdit(std::string_view source, std::string_view translation,
    std::string_view pointer, std::string_view value, LocalizationEditPlan& plan, std::string& error);
// Rechecks both exact snapshots and reconstructs the edit before returning text.
// This does not write files: callers must also check project/document identity,
// document revision and external changes, then use one undoable editor edit.
[[nodiscard]] bool applyLocalizationEdit(const LocalizationEditPlan& plan, std::string_view currentSource,
    std::string_view currentTranslation, std::string& updated, std::string& error);
}
