#include "logging/Log.hpp"

#include <cstdio>
#include <iostream>
#include <mutex>
#include <string>

#if defined(_WIN32)
#include <io.h>
#define LUCID_ISATTY _isatty
#define LUCID_FILENO _fileno
#else
#include <unistd.h>
#define LUCID_ISATTY isatty
#define LUCID_FILENO fileno
#endif

namespace lucid::log {
namespace {

std::mutex g_mutex;
Level g_level = Level::Info;

[[nodiscard]] constexpr std::string_view levelName(Level level) noexcept {
    switch (level) {
    case Level::Trace:
        return "TRACE";
    case Level::Debug:
        return "DEBUG";
    case Level::Info:
        return "INFO ";
    case Level::Warn:
        return "WARN ";
    case Level::Error:
        return "ERROR";
    case Level::Fatal:
        return "FATAL";
    }
    return "?????";
}

[[nodiscard]] constexpr const char* levelColor(Level level) noexcept {
    switch (level) {
    case Level::Trace:
        return "\x1b[90m";
    case Level::Debug:
        return "\x1b[36m";
    case Level::Info:
        return "\x1b[37m";
    case Level::Warn:
        return "\x1b[33m";
    case Level::Error:
        return "\x1b[31m";
    case Level::Fatal:
        return "\x1b[35m";
    }
    return "\x1b[0m";
}

[[nodiscard]] bool colorEnabled() {
    static const bool enabled = LUCID_ISATTY(LUCID_FILENO(stderr)) != 0;
    return enabled;
}

} // namespace

void setLevel(Level level) noexcept {
    const std::lock_guard lock(g_mutex);
    g_level = level;
}

Level level() noexcept {
    const std::lock_guard lock(g_mutex);
    return g_level;
}

void write(Level level, std::string_view message) {
    if (static_cast<int>(level) < static_cast<int>(g_level)) {
        return;
    }

    const std::lock_guard lock(g_mutex);
    std::ostream& stream = std::cerr;
    stream << '[';
    if (colorEnabled()) {
        stream << levelColor(level);
    }
    stream << levelName(level);
    if (colorEnabled()) {
        stream << "\x1b[0m";
    }
    stream << "] " << message << '\n';
    stream.flush();
}

} // namespace lucid::log
