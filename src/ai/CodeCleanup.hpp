#pragma once
#include "editor/document/Document.hpp"
#include <string>
#include <string_view>
namespace lucid::ai {
// Build a bounded request for the selected code with nearby file style examples.
[[nodiscard]] bool cleanupRequest(const editor::Document& document, std::string_view file,
                                  std::string& request, std::string& error);
[[nodiscard]] bool cleanupResponse(std::string& response, std::string& error);
// Bounded untrusted diagnostic and nearby unsaved source for a local model.
// Line and column are zero-based; an invalid line omits source entirely.
[[nodiscard]] std::string diagnosticContext(const editor::Document& document,
    std::size_t line, std::size_t column, std::string_view diagnostic);
}
