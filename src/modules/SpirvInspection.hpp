#pragma once
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
struct SpirvWorkgroupSize { std::uint32_t x = 0, y = 0, z = 0; };
struct SpirvEntryPoint { std::string name; std::uint32_t id = 0, executionModel = 0; std::optional<SpirvWorkgroupSize> localSize; };
struct SpirvInspection { std::uint32_t version = 0, generator = 0, idBound = 0; std::vector<SpirvEntryPoint> entryPoints; bool incomplete = false; };
// Inspects bounded binary words only; does not validate semantics, execute code,
// infer resource bindings or guarantee GPU compatibility. Unknown execution
// modes/stages and specialization-based workgroup sizes are reported incomplete.
// Errors and cancellation clear the output; callers must check error.
[[nodiscard]] bool inspectSpirv(std::string_view bytes, SpirvInspection& output, std::string& error, const std::atomic_bool* cancel = nullptr);
}
