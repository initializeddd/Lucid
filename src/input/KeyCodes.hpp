#pragma once

namespace lucid::input {

enum class Key {
    Unknown,
    Escape,
    Backspace,
    Enter,
    Tab,
    Delete,
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    A,
    C,
    Comma,
    D,
    K,
    N,
    O,
    P,
    R,
    S,
    Slash,
    V,
    W,
    X,
    Y,
    Z,
};

enum class KeyAction {
    Press,
    Repeat,
    Release,
};

struct KeyEvent {
    Key key = Key::Unknown;
    KeyAction action = KeyAction::Press;
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool super = false;
};

} // namespace lucid::input
