#pragma once

#include "core/Types.hpp"

#include <string_view>

namespace lucid {

inline constexpr std::string_view kAppName = "Lucid";
inline constexpr u32 kVersionMajor = 0;
inline constexpr u32 kVersionMinor = 1;
inline constexpr u32 kVersionPatch = 0;

[[nodiscard]] constexpr u32 packVersion(u32 major, u32 minor, u32 patch) noexcept {
    return (major << 22U) | (minor << 12U) | patch;
}

inline constexpr u32 kVersion = packVersion(kVersionMajor, kVersionMinor, kVersionPatch);

} // namespace lucid
