#include "platform/browser/WindowsWebPreviewRouter.hpp"

namespace lucid::platform {
WindowsPreviewResponse routeWindowsWebPreviewRequest(const modules::WebPreviewAssets& assets,
                                                       std::string_view method, std::string_view absoluteUrl) {
    // WebView2 adapters should intercept every URI, including non-HTTP schemes.
    // A fixed, reserved host avoids proxying arbitrary origins into the snapshot.
    constexpr std::string_view origin = "https://lucid-preview.invalid";
    if (absoluteUrl.size() > 8192 || !absoluteUrl.starts_with(origin) ||
        absoluteUrl.size() <= origin.size() || absoluteUrl[origin.size()] != '/' ||
        absoluteUrl.find('#') != std::string_view::npos) return {};
    auto response = assets.respond(method, absoluteUrl.substr(origin.size()));
    return {WindowsPreviewRoute::Snapshot, std::move(response)};
}
bool allowWindowsWebPreviewNavigation(const modules::WebPreviewAssets& assets, std::string_view url) {
    if (url.size()>8192) return false;
    const auto fragment=url.find('#');
    if (fragment!=url.npos) {
        for (const unsigned char c : url.substr(fragment+1)) if (c<0x20 || c==0x7f || c=='\\' || c=='#') return false;
        url=url.substr(0,fragment);
    }
    const auto route=routeWindowsWebPreviewRequest(assets,"GET",url);
    return route.route==WindowsPreviewRoute::Snapshot && route.response.status==200 && route.response.contentType.starts_with("text/html;");
}
}
