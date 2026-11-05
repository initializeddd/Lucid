#include "modules/WebPreviewConfig.hpp"
#include "modules/WebPreviewAssets.hpp"
#include "modules/WebPreviewBundle.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
using namespace lucid::modules;
int main() {
    namespace fs = std::filesystem;
    auto root = fs::canonical(fs::temp_directory_path()) / ("lucid-web-preview-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "pages"); std::ofstream(root / "index.html") << "<html>"; std::ofstream(root / "pages/a.htm") << "ok";
    WebPreviewConfig config{"index.html", 1280, 720, {{"default", "?view=dark#card"}, {"alternate", "pages/a.htm?x=1"}}}; std::string error;
    bool ok = validateWebPreviewConfig(root, config, error);
    config.states[1].url = "https://example.com/"; ok = ok && !validateWebPreviewConfig(root, config, error);
    config.states[1].url = "//example.com/a"; ok = ok && !validateWebPreviewConfig(root, config, error);
    config.states[1].url = "%2e%2e/index.html"; ok = ok && !validateWebPreviewConfig(root, config, error);
    config.states[1].url = "../index.html"; ok = ok && !validateWebPreviewConfig(root, config, error);
    config.states[1].url = "?x=%0a"; ok = ok && !validateWebPreviewConfig(root, config, error);
    config.states[1].url = "javascript:alert(1)"; ok = ok && !validateWebPreviewConfig(root, config, error);
    config.states[1].url = "pages/a.htm"; config.width = 4096; config.height = 4096; ok = ok && !validateWebPreviewConfig(root, config, error);
    config.width = 1280; config.height = 720; config.entry = "../index.html"; ok = ok && !validateWebPreviewConfig(root, config, error);
#ifndef _WIN32
    config.entry = "link.html"; fs::create_symlink(root / "index.html", root / "link.html"); ok = ok && !validateWebPreviewConfig(root, config, error);
#endif
    WebPreviewAssets snapshot;
    ok = ok && snapshot.load({{"/index.html", "<script src=\"app.js\"></script>"}, {"/app.js", "customElements.define('x-a', class extends HTMLElement {});"}}, error);
    const auto page = snapshot.respond("GET", "/index.html?theme=dark");
    ok = ok && page.status == 200 && page.contentType == "text/html; charset=utf-8" && !page.body.empty();
    const auto head = snapshot.respond("HEAD", "/app.js");
    ok = ok && head.status == 200 && head.body.empty() && head.contentType == "text/javascript; charset=utf-8";
    ok = ok && snapshot.respond("POST", "/index.html").status == 405;
    ok = ok && snapshot.respond("GET", "/%2e%2e/secrets").status == 400;
    ok = ok && snapshot.respond("GET", "/../secrets").status == 400;
    ok = ok && snapshot.respond("GET", "/.env").status == 400;
    ok = ok && snapshot.respond("GET", "/other.js").status == 404;
    ok = ok && !snapshot.load({{"/ok.js", "x"}, {"/ok.js", "y"}}, error);
    ok = ok && snapshot.respond("GET", "/ok.js").status == 404;
    ok = ok && !snapshot.load({{"/secret.txt", "x"}}, error);
    ok = ok && snapshot.load({{"/shape.svg", "<svg xmlns=\"http://www.w3.org/2000/svg\"/>"}, {"/type.ttf", "font"}, {"/type.otf", "font"}}, error);
    const auto svg = snapshot.respond("GET", "/shape.svg");
    ok = ok && svg.status == 200 && svg.contentType == "image/svg+xml" && !svg.body.empty();
    bool svgPolicy = false;
    for (const auto& header : svg.headers) if (header.first == "Content-Security-Policy") svgPolicy = header.second == "default-src 'none'; style-src 'unsafe-inline'; sandbox";
    ok = ok && svgPolicy && snapshot.respond("HEAD", "/shape.svg").body.empty();
    ok = ok && snapshot.respond("GET", "/type.ttf").contentType == "font/ttf" && snapshot.respond("GET", "/type.otf").contentType == "font/otf";
    bool created = false; auto project = lucid::workspace::Project::openOrCreate(root, created, error, false, false); WebPreviewBundle bundle;
    ok = ok && error.empty() && loadWebPreviewBundle(project, R"({"entry":"index.html","assets":["index.html","pages/a.htm"],"width":400,"height":300,"state":"?dark=1"})", bundle, error);
    ok = ok && bundle.width == 400 && bundle.height == 300 && bundle.target == "/index.html?dark=1" && bundle.assets.respond("GET", "/index.html").body == "<html>";
    ok = ok && !loadWebPreviewBundle(project, R"({"entry":"index.html","assets":["../index.html"]})", bundle, error);
    ok = ok && !loadWebPreviewBundle(project, R"({"entry":"index.html","entry":"index.html","assets":["index.html"]})", bundle, error);
    ok = ok && !loadWebPreviewBundle(project, R"({"entry":"index.html","assets":["pages/a.htm"]})", bundle, error);
    fs::remove_all(root); return ok ? 0 : 1;
}
