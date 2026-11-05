#pragma once
#include "editor/json/Json.hpp"
#include <string>
#include <string_view>
#include <filesystem>
#include <optional>
#include <vector>
namespace lucid::workspace { class Project; }
namespace lucid::editor::lsp {
// Applies non-overlapping LSP UTF-16 edits to an LF-normalized UTF-8 buffer.
[[nodiscard]] bool applyTextEdits(std::string_view source, const JsonValue& edits, std::string& output, std::string& error);
// Parameter labels may be strings or UTF-16 offsets into a signature label.
[[nodiscard]] std::optional<std::string> signatureParameterLabel(const JsonValue& signature, std::size_t parameter);
struct WorkspaceTextChange { std::size_t line = 0; std::string before, after; bool truncated = false; };
struct WorkspaceFileEdit { std::string path, original, updated; std::vector<WorkspaceTextChange> changes; };
// Prepares edits without writing. The caller must review and recheck every file before applying.
// A versioned edit is accepted only for the active document at its current LSP version.
[[nodiscard]] bool prepareWorkspaceEdits(const workspace::Project& project, const JsonValue& edit,
    std::string_view activePath, std::string_view activeText, int activeVersion,
    std::vector<WorkspaceFileEdit>& files, std::string& error);
// Applies reviewed edits after rechecking all originals and recording backups.
// On a write failure, changedPaths identifies files already committed.
[[nodiscard]] bool applyWorkspaceEdits(workspace::Project& project, const std::vector<WorkspaceFileEdit>& files,
    const std::filesystem::path& historyRoot, std::vector<std::string>& changedPaths, std::string& error);
}
