#pragma once

#include <string_view>

namespace lucid::editor {

enum class CursorStyle {
    Line,
    Block,
    Underscore,
    Kitty,
};

[[nodiscard]] constexpr std::string_view cursorStyleName(CursorStyle style) noexcept {
    switch (style) {
    case CursorStyle::Line:
        return "line";
    case CursorStyle::Block:
        return "block";
    case CursorStyle::Underscore:
        return "underscore";
    case CursorStyle::Kitty:
        return "kitty";
    }
    return "line";
}

} // namespace lucid::editor
