#pragma once
#include "modules/CMakeModel.hpp"
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
struct CMakeCacheValue { std::string name, type, value; std::vector<std::string> choices; };
// Reads saved cache metadata; STRING/PATH/FILEPATH only. A supported -STRINGS
// property constrains values; unsupported CMake list escapes are rejected.
[[nodiscard]] bool listCMakeCacheValues(const workspace::Project& project, std::string_view cachePath,
    std::vector<CMakeCacheValue>& values, std::string& error);
[[nodiscard]] bool prepareCMakeCacheValueUpdate(const workspace::Project& project, std::string_view cachePath,
    std::string_view name, std::string_view type, std::string_view expected, std::string_view replacement,
    CMakeCacheUpdate& update, std::string& error);
[[nodiscard]] bool saveCMakeCacheValueUpdate(workspace::Project& project, const CMakeCacheUpdate& update, std::string& error);
}
