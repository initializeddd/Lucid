#pragma once
#include "modules/Modules.hpp"
#include <string>
namespace lucid::modules {
// Fingerprints the reviewed module's effective parsed fields and command order.
// Unknown manifest fields ignored by the parser are not represented.
[[nodiscard]] std::string moduleFingerprint(const Module& module);
}
