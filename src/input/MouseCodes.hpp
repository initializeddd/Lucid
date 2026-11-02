#pragma once

#include "input/KeyCodes.hpp"

namespace lucid::input {

enum class MouseButton {
    Unknown,
    Left,
    Right,
    Middle,
};

struct MouseButtonEvent {
    double x = 0.0;
    double y = 0.0;
    MouseButton button = MouseButton::Unknown;
    KeyAction action = KeyAction::Press;
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool super = false;
};

struct MouseMoveEvent {
    double x = 0.0;
    double y = 0.0;
};

struct ScrollEvent {
    double xOffset = 0.0;
    double yOffset = 0.0;
};

} // namespace lucid::input
