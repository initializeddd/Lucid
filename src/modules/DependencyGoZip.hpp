#pragma once
#include <atomic>
#include <string>
#include <string_view>
namespace lucid::modules {
// Computes Go's dirhash h1 over a bounded, canonical module ZIP. Unsupported
// ZIP variants fail closed. This does not authenticate the module or extract files.
[[nodiscard]] bool hashGoModuleZip(std::string_view bytes, std::string_view module, std::string_view version,
    std::string& hash, std::string& error, const std::atomic_bool* cancel = nullptr);
}
