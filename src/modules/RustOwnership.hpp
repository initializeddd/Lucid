#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "editor/document/Document.hpp"
namespace lucid::modules {
struct RustOwnershipSourceLine {
    std::string text;
    std::size_t highlightStart = 0, highlightEnd = 0; // rustc 1-based positions, end exclusive.
};
struct RustOwnershipSpan {
    std::string file, label, replacement, applicability;
    std::size_t lineStart = 0, columnStart = 0, lineEnd = 0, columnEnd = 0;
    bool primary = false, hasReplacement = false;
    std::vector<RustOwnershipSourceLine> sourceLines;
};
struct RustOwnershipNote { std::string level, message; std::vector<RustOwnershipSpan> spans; };
struct RustOwnershipDiagnostic {
    std::string code, level, message, explanation, rendered;
    std::vector<RustOwnershipSpan> spans;
    std::vector<RustOwnershipNote> notes;
};
// Parse newline-delimited cargo --message-format=json or rustc JSON diagnostics.
// Captures a fixed set of Rust ownership/borrow diagnostic codes, including compiler
// notes and spans. Columns are rustc's 1-based character positions, not byte offsets.
// No project file access, process execution, network, inference, or fix application.
[[nodiscard]] bool parseRustOwnershipDiagnostics(std::string_view input,
    std::vector<RustOwnershipDiagnostic>& diagnostics, std::string& error);

struct RustSuggestionEdit {
    editor::TextPosition start{}, end{};
    std::string original, replacement;
};
// Prepares a single compiler-suggested edit, without mutating the document.
// The caller must independently ensure this is not part of a multipart fix,
// verify project/file identity, and check the same document revision on apply.
// Every covered compiler source line must exactly match the current document.
[[nodiscard]] bool prepareRustSuggestion(const RustOwnershipSpan& span,
    const editor::Document& document, RustSuggestionEdit& edit, std::string& error);
}
