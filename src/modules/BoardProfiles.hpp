#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
struct BoardProfile { std::string interfaceConfig, targetConfig, firmware; unsigned adapterKhz = 0; std::vector<std::string> warnings; std::string error; bool incomplete = false; };
// Reads a deliberately restricted declarative OpenOCD subset. Never evaluates Tcl,
// opens files, invokes commands, or validates actual hardware availability.
[[nodiscard]] BoardProfile parseBoardProfile(std::string_view text, std::size_t maxBytes = 65536, std::size_t maxLines = 1000);
}
