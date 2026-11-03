#pragma once

namespace lucid::input {

enum class Key {
    Unknown,
    Escape,
    Backspace,
    Enter,
    Tab,
    Space,
    Delete,
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    F12,
    A,
    B,
    C,
    Comma,
    Equal,
    Minus,
    Zero,
    D,
    E,
    F,
    G,
    H,
    K,
    L,
    M,
    N,
    O,
    P,
    R,
    S,
    T,
    U,
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
