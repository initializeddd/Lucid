#pragma once
#include <string_view>
namespace lucid::platform {
enum class WebView2LoaderArch { X86, X64, Arm64 };
// Matches only the pinned Microsoft WebView2 1.0.2903.40 loader bytes for the
// specified target. This checks integrity against the build pin; it does not
// establish a sandbox or defeat same-user path races during dynamic loading.
[[nodiscard]] bool isPinnedWebView2Loader(std::string_view bytes, WebView2LoaderArch arch);
}
