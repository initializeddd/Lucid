#include "modules/WebPreviewAssets.hpp"
#include <algorithm>
#include <set>
#include <utility>

namespace lucid::modules {
namespace {
constexpr std::size_t kMaxTotal = 16U * 1024U * 1024U;
bool validPath(std::string_view path) {
    if (path.empty() || path.size() > 4096 || path.front() != '/' || path.back() == '/') return false;
    std::size_t start = 1;
    for (std::size_t i = 1; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            const auto part = path.substr(start, i - start);
            if (part.empty() || part.front() == '.') return false;
            start = i + 1;
        } else {
            const auto c = static_cast<unsigned char>(path[i]);
            // No percent-encoding, non-ASCII, control characters, or URL delimiters:
            // a server must pass the untouched origin-form path to this router.
            if (c < 0x21 || c > 0x7e || c == '%' || c == '\\' || c == '?' || c == '#' || c == ':') return false;
        }
    }
    return true;
}
std::string contentType(std::string_view path) {
    const auto dot = path.rfind('.');
    if (dot == std::string_view::npos) return {};
    std::string ext(path.substr(dot));
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : static_cast<char>(c); });
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js" || ext == ".mjs") return "text/javascript; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".webp") return "image/webp";
    if (ext == ".woff") return "font/woff";
    if (ext == ".woff2") return "font/woff2";
    return {};
}
WebPreviewResponse response(int status) {
    WebPreviewResponse out; out.status = status;
    out.headers = {{"Cache-Control", "no-store"}, {"X-Content-Type-Options", "nosniff"},
        {"Referrer-Policy", "no-referrer"}, {"Cross-Origin-Resource-Policy", "same-origin"},
        {"Content-Security-Policy", "default-src 'none'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; font-src 'self'; connect-src 'none'; object-src 'none'; base-uri 'none'; frame-src 'none'; form-action 'none'"}};
    return out;
}
}
bool WebPreviewAssets::load(std::vector<WebPreviewAsset> assets, std::string& error) {
    m_assets.clear(); error.clear();
    if (assets.empty() || assets.size() > 256) { error = "Preview requires 1–256 explicit assets."; return false; }
    std::size_t total = 0; std::set<std::string> paths;
    for (const auto& asset : assets) {
        if (!validPath(asset.path) || contentType(asset.path).empty() || !paths.insert(asset.path).second) {
            error = "Invalid, unsupported or duplicate preview asset path."; return false;
        }
        if (asset.bytes.size() > kMaxTotal - total) { error = "Preview assets exceed 16 MiB."; return false; }
        total += asset.bytes.size();
    }
    m_assets = std::move(assets); return true;
}
WebPreviewResponse WebPreviewAssets::respond(std::string_view method, std::string_view target) const {
    if (method != "GET" && method != "HEAD") { auto out = response(405); out.headers.emplace_back("Allow", "GET, HEAD"); return out; }
    const auto query = target.find('?'); const auto path = target.substr(0, query);
    if (!validPath(path) || target.size() > 8192 || target.find('#') != std::string_view::npos) return response(400);
    for (unsigned char c : target) if (c < 0x20 || c == 0x7f) return response(400);
    const auto it = std::find_if(m_assets.begin(), m_assets.end(), [&](const auto& asset) { return asset.path == path; });
    if (it == m_assets.end()) return response(404);
    auto out = response(200); out.contentType = contentType(it->path);
    out.headers.emplace_back("Content-Length", std::to_string(it->bytes.size()));
    if (method == "GET") out.body = it->bytes;
    return out;
}
}
