#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
struct BoardMapping { std::string name, profile; std::uint16_t vendor = 0, product = 0; };
struct BoardMappings { std::vector<BoardMapping> entries; std::string error; };
struct ObservedUsbDevice { std::uint16_t vendor = 0, product = 0; };
struct BoardIdentity { enum class State { Unknown, Match, Ambiguous } state = State::Unknown; std::string name, profile; };
// Project-controlled mappings are declarations, not proof of board identity.
// Input IDs must come from an independently trusted OS descriptor source.
// No device enumeration, access or process execution occurs here.
[[nodiscard]] BoardMappings parseBoardMappings(std::string_view json);
[[nodiscard]] BoardIdentity identifyBoard(ObservedUsbDevice device, const BoardMappings& mappings);
}
