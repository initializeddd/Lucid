#pragma once

namespace lucid::platform {

// Selects a single MoltenVK ICD on macOS unless the user already chose a driver.
// Keeping the driver list explicit avoids loading duplicate MoltenVK copies when
// both Homebrew and the LunarG SDK are installed. No-op on other platforms.
void configureVulkanDriverEnvironment();

} // namespace lucid::platform
