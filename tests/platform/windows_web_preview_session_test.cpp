#include "platform/browser/WindowsWebPreviewSession.hpp"
#include <filesystem>
#include <string>
int main() {
    lucid::modules::WebPreviewAssets assets; std::string error;
    if (!assets.load({{"/index.html","<p>test</p>"}},error)) return 1;
    lucid::platform::WindowsWebPreviewSession session;
    const auto loader=std::filesystem::temp_directory_path()/"missing-webview2-loader.dll";
    if (session.start(nullptr,loader,assets,"/index.html",32,32,false,error) || error.find("trust")==std::string::npos || !session.retainedProfile().empty() || session.running()) return 2;
    if (session.start(nullptr,loader,assets,"/index.html",32,32,true,error) || error.empty() || !session.retainedProfile().empty() || session.running()) return 3;
    if (session.start(reinterpret_cast<void*>(1),loader,assets,std::string(5000,'x'),32,32,true,error) || error.empty() || !session.retainedProfile().empty()) return 5;
    if (session.start(reinterpret_cast<void*>(1),loader,assets,"/missing.html",32,32,true,error) || error.find("target")==std::string::npos || !session.retainedProfile().empty()) return 6;
    session.poll(); session.cancel(); if (session.finished() || !session.result().png.empty()) return 4;
    return 0;
}
