#include "platform/browser/WebView2LoaderPolicy.hpp"
#include <string>
#include <fstream>
using namespace lucid::platform;
int main(int argc, char** argv) {
    if (isPinnedWebView2Loader({},WebView2LoaderArch::X64)) return 1;
    if (isPinnedWebView2Loader("not a loader",WebView2LoaderArch::X86)) return 2;
    if (isPinnedWebView2Loader(std::string(1024U*1024U+1,'x'),WebView2LoaderArch::Arm64)) return 3;
    if (isPinnedWebView2Loader("not a loader",static_cast<WebView2LoaderArch>(99))) return 4;
    if (argc==2) {
        std::ifstream file(argv[1],std::ios::binary); const std::string bytes((std::istreambuf_iterator<char>(file)),{});
        if (!isPinnedWebView2Loader(bytes,WebView2LoaderArch::X64) || isPinnedWebView2Loader(bytes,WebView2LoaderArch::X86)) return 5;
        auto changed=bytes; changed[0]^=1; if (isPinnedWebView2Loader(changed,WebView2LoaderArch::X64)) return 6;
    }
    return 0;
}
