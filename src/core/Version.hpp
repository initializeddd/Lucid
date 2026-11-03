#pragma once

#include "core/Types.hpp"

#include <string_view>

namespace lucid {

inline constexpr std::string_view kAppName = "Lucid";
inline constexpr std::string_view kAppDisplayName = "Lucid (Beta)";
inline constexpr std::string_view kReleaseChannel = "beta";
inline constexpr bool kIsPrerelease = true;
inline constexpr std::string_view kVersionString = "0.2.1-lucid.1";
inline constexpr u32 kVersionMajor = 0;
inline constexpr u32 kVersionMinor = 2;
inline constexpr u32 kVersionPatch = 1;

[[nodiscard]] constexpr u32 packVersion(u32 major, u32 minor, u32 patch) noexcept {
    return (major << 22U) | (minor << 12U) | patch;
}

inline constexpr u32 kVersion = packVersion(kVersionMajor, kVersionMinor, kVersionPatch);

} // namespace lucid
