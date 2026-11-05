#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lucid::modules {
// An explicit in-memory snapshot supplied by the host. Files are never read on a
// request; callers must separately choose and read project-contained assets.
struct WebPreviewAsset { std::string path, bytes; };
struct WebPreviewResponse { int status = 404; std::string contentType, body; std::vector<std::pair<std::string, std::string>> headers; };
class WebPreviewAssets {
public:
    // All-or-nothing. Allows at most 256 unique, explicitly named assets and 16 MiB
    // total. A failed load invalidates any previous snapshot.
    [[nodiscard]] bool load(std::vector<WebPreviewAsset> assets, std::string& error);
    // Routes only GET and HEAD against the snapshot, never filesystem or network.
    [[nodiscard]] WebPreviewResponse respond(std::string_view method, std::string_view target) const;
private:
    std::vector<WebPreviewAsset> m_assets;
};
}
