#pragma once

#include "modules/WebPreviewAssets.hpp"
#include <string_view>

namespace lucid::platform {

// Routing policy for a future Windows WebView2 adapter. The adapter must install
// a catch-all resource handler, deny external navigation/popups, and supply a
// synthetic error for blocked requests. This helper does not instantiate a
// browser and is not a network or process sandbox. Explicitly trusting the
// project is required before any browser executes its JavaScript.
enum class WindowsPreviewRoute { Snapshot, Blocked };
struct WindowsPreviewResponse {
    WindowsPreviewRoute route = WindowsPreviewRoute::Blocked;
    modules::WebPreviewResponse response;
};
[[nodiscard]] WindowsPreviewResponse routeWindowsWebPreviewRequest(
    const modules::WebPreviewAssets& assets, std::string_view method, std::string_view absoluteUrl);
// Navigation may carry a fragment, which is never sent as a resource request.
// Only an explicitly snapshotted HTML resource on the reserved origin is valid.
[[nodiscard]] bool allowWindowsWebPreviewNavigation(const modules::WebPreviewAssets& assets,
                                                    std::string_view absoluteUrl);

}
