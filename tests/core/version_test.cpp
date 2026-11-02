#include "core/Version.hpp"

#include <array>
#include <cstdint>
#include <string_view>

int main() {
    using namespace lucid;

    static_assert(kVersion == ((kVersionMajor << 22U) | (kVersionMinor << 12U) | kVersionPatch));
    static_assert(packVersion(1, 2, 3) == 0x00402003U);
    static_assert(kAppName == std::string_view("Lucid"));

    constexpr std::array<std::uint32_t, 3> components{kVersionMajor, kVersionMinor, kVersionPatch};
    return components[0] == 0 && components[1] == 1 && components[2] == 0 ? 0 : 1;
}
