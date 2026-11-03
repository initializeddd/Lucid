#pragma once
#include "editor/document/Document.hpp"
#include <optional>
#include <string_view>
#include <vector>

namespace lucid::editor {
// Search from the given position, wrapping at the document boundary.
[[nodiscard]] std::optional<TextPosition> findText(
    const std::vector<std::u32string>& lines, std::u32string_view query,
    TextPosition from, bool backwards = false);
}
