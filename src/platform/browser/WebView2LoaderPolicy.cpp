#include "platform/browser/WebView2LoaderPolicy.hpp"
#include <picosha2.h>
namespace lucid::platform {
bool isPinnedWebView2Loader(std::string_view bytes,WebView2LoaderArch arch) {
    if (bytes.empty() || bytes.size()>1024U*1024U) return false;
    const char* expected=nullptr;
    switch (arch) {
        case WebView2LoaderArch::X86: expected="6f59ad578fde1f65c44b6935dab0901f014b5268e056b930fb34c0919f1bf4b9"; break;
        case WebView2LoaderArch::X64: expected="462b36fd1be6ca9f7563466a89e57c41ef4a4def3e0a84fa885d203aea4a3aaf"; break;
        case WebView2LoaderArch::Arm64: expected="cb561ae4537e39bba5dd8af1f648f189d9562b1003fd110a9e889ace527fb4e8"; break;
    }
    return expected && picosha2::hash256_hex_string(bytes.begin(),bytes.end())==expected;
}
}
