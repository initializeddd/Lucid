#include "platform/browser/WindowsWebPreviewHost.hpp"
#include <string>
int main() {
    lucid::modules::WebPreviewAssets assets; std::string error;
    if (!assets.load({{"/index.html","<p>ok</p>"}},error)) return 1;
    lucid::platform::WindowsWebPreviewHost host;
    if (host.start(nullptr,assets,"/index.html",32,32,false,error) || error.find("trust")==std::string::npos || host.running() || !host.retainedProfile().empty()) return 2;
    if (host.start(nullptr,assets,"/index.html",32,32,true,error) || error.empty() || host.running() || !host.retainedProfile().empty()) return 3;
    host.poll(); host.cancel(); if (host.finished() || !host.result().png.empty()) return 4;
    return 0;
}
