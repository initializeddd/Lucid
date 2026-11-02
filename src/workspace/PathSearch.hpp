#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::workspace {

[[nodiscard]] std::vector<std::string_view> filterPaths(const std::vector<std::string>& paths,
                                                        std::string_view query,
                                                        std::size_t limit);

} // namespace lucid::workspace
