#pragma once
#include "editor/json/Json.hpp"
#include <string>
#include <string_view>
namespace lucid::editor::lsp {
// Applies non-overlapping LSP UTF-16 edits to an LF-normalized UTF-8 buffer.
[[nodiscard]] bool applyTextEdits(std::string_view source, const JsonValue& edits, std::string& output, std::string& error);
}
