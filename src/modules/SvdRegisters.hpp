#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
struct SvdEnumValue { std::string name, usage; std::optional<std::uint64_t> value; bool isDefault=false; };
struct SvdField { std::string name, access; unsigned offset = 0, width = 0; std::string description = {}; std::vector<SvdEnumValue> values = {}; };
struct SvdRegister { std::string peripheral, name, access; std::uint64_t address = 0; unsigned sizeBits = 0; std::optional<std::uint64_t> resetValue; std::vector<SvdField> fields; std::string description = {}; };
struct SvdRegisters { std::vector<SvdRegister> registers; std::vector<std::string> warnings; std::string error; bool incomplete = false; };
// A bounded, read-only CMSIS-SVD subset. No DTDs, custom/external entities,
// field derivedFrom, derived peripheral arrays, nested/derived clusters, or hardware access.
// Single-level register clusters and simple cluster arrays are expanded.
// Registers may derive from a unique earlier register in the same peripheral.
// Peripherals may derive from a unique earlier complete peripheral with an explicit
// name/base and no register or default-property overrides.
// Peripheral, register, field and cluster arrays support literal indexes and single
// inclusive decimal or ASCII-letter dimIndex ranges. Device and
// peripheral size, access and resetValue defaults are inherited.
[[nodiscard]] SvdRegisters parseSvdRegisters(std::string_view xml, std::size_t maxBytes = 4*1024*1024, std::size_t maxRegisters = 10000);
}
