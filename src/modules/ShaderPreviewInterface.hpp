#pragma once
#include <string>
#include <string_view>
namespace lucid::modules {
// Restrictive interface policy for the fullscreen preview: one fragment main,
// one float32 vec4 color output at location 0, optional float32 vec2 input at
// location 0, no descriptors, push constants, built-ins or extensions. This is
// NOT complete SPIR-V validation, a termination proof, or a GPU sandbox.
[[nodiscard]] bool validatePreviewFragmentInterface(std::string_view bytes, std::string& error);
}
