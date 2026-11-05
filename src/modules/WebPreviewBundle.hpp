#pragma once
#include "modules/WebPreviewAssets.hpp"
#include "workspace/Project.hpp"
#include <string>
#include <string_view>

namespace lucid::modules {
struct WebPreviewBundle { WebPreviewAssets assets; std::string target; unsigned width = 800, height = 600; };
// Parse a saved or unsaved JSON configuration and explicitly snapshot only the
// listed project-relative assets. No scripts execute during this operation.
[[nodiscard]] bool loadWebPreviewBundle(const workspace::Project& project, std::string_view json,
                                        WebPreviewBundle& bundle, std::string& error);
}
