#include "platform/browser/WindowsWebPreviewRouter.hpp"
#include <string>

int main() {
    using namespace lucid;
    modules::WebPreviewAssets assets; std::string error;
    if (!assets.load({{"/index.html", "<p>ok</p>"}, {"/app.js", "run()"}}, error)) return 1;
    auto ok = platform::routeWindowsWebPreviewRequest(assets, "GET", "https://lucid-preview.invalid/index.html?state=1");
    if (ok.route != platform::WindowsPreviewRoute::Snapshot || ok.response.status != 200 || ok.response.body != "<p>ok</p>") return 2;
    auto head = platform::routeWindowsWebPreviewRequest(assets, "HEAD", "https://lucid-preview.invalid/app.js");
    if (head.route != platform::WindowsPreviewRoute::Snapshot || head.response.status != 200 || !head.response.body.empty()) return 3;
    auto post = platform::routeWindowsWebPreviewRequest(assets, "POST", "https://lucid-preview.invalid/index.html");
    if (post.route != platform::WindowsPreviewRoute::Snapshot || post.response.status != 405) return 4;
    auto missing = platform::routeWindowsWebPreviewRequest(assets, "GET", "https://lucid-preview.invalid/missing.html");
    if (missing.route != platform::WindowsPreviewRoute::Snapshot || missing.response.status != 404) return 5;
    for (const auto* url : {"http://lucid-preview.invalid/index.html", "https://lucid-preview.invalid.evil.test/index.html",
                            "https://lucid-preview.invalid@evil.test/index.html", "https://lucid-preview.invalid:443/index.html",
                            "file:///etc/passwd", "https://lucid-preview.invalid/index.html#fragment"}) {
        if (platform::routeWindowsWebPreviewRequest(assets, "GET", url).route != platform::WindowsPreviewRoute::Blocked) return 6;
    }
    auto encoded = platform::routeWindowsWebPreviewRequest(assets, "GET", "https://lucid-preview.invalid/%2e%2e/app.js");
    if (encoded.route != platform::WindowsPreviewRoute::Snapshot || encoded.response.status != 400) return 7;
    if (!platform::allowWindowsWebPreviewNavigation(assets,"https://lucid-preview.invalid/index.html?tab=1#section") ||
        !platform::allowWindowsWebPreviewNavigation(assets,"https://lucid-preview.invalid/index.html#")) return 8;
    for (const auto* url : {"https://evil.test/index.html#https://lucid-preview.invalid/index.html", "https://lucid-preview.invalid.evil.test/index.html#ok", "https://lucid-preview.invalid/missing.html#ok", "https://lucid-preview.invalid/app.js#ok", "https://lucid-preview.invalid/index.html#bad\\fragment", "https://lucid-preview.invalid/index.html#bad#fragment", "https://lucid-preview.invalid/%2e%2e/index.html#ok"})
        if (platform::allowWindowsWebPreviewNavigation(assets,url)) return 9;
    if (platform::allowWindowsWebPreviewNavigation(assets,std::string(8193,'x'))) return 10;
    return 0;
}
