#pragma once

#include <stdexcept>
#include <string>

namespace lucid {

[[noreturn]] inline void assertionFailed(const char* expression, const char* file, int line) {
    throw std::runtime_error(std::string("Assertion failed: ") + expression + " (" + file + ":" +
                             std::to_string(line) + ")");
}

} // namespace lucid

#ifndef NDEBUG
#define LUCID_ASSERT(expression)                                                                   \
    ((expression) ? static_cast<void>(0)                                                           \
                  : ::lucid::assertionFailed(#expression, __FILE__, __LINE__))
#else
#define LUCID_ASSERT(expression) static_cast<void>(0)
#endif
