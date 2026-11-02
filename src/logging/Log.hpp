#pragma once

#include <string_view>

namespace lucid::log {

enum class Level : int {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Fatal = 5,
};

void setLevel(Level level) noexcept;
[[nodiscard]] Level level() noexcept;

void write(Level level, std::string_view message);

inline void trace(std::string_view message) { write(Level::Trace, message); }
inline void debug(std::string_view message) { write(Level::Debug, message); }
inline void info(std::string_view message) { write(Level::Info, message); }
inline void warn(std::string_view message) { write(Level::Warn, message); }
inline void error(std::string_view message) { write(Level::Error, message); }
inline void fatal(std::string_view message) { write(Level::Fatal, message); }

} // namespace lucid::log
