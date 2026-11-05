#pragma once

#include "modules/WebPreviewAssets.hpp"
#include <functional>
#include <memory>
#include <string>

namespace lucid::platform {
struct MacWebPreviewResult { std::string png, error; };

// macOS-only, asynchronous WKWebView snapshot of an explicit asset set. This
// executes project JavaScript and is NOT a security sandbox; callers must ask
// the user to trust it before starting. All methods and the callback must run on
// the macOS main thread, after NSApplication has been initialized. No callback
// occurs after cancel() or destruction. Requires WebKit and ARC when linked.
class MacWebPreview {
public:
    using Completion = std::function<void(MacWebPreviewResult)>;
    MacWebPreview();
    ~MacWebPreview();
    MacWebPreview(const MacWebPreview&) = delete;
    MacWebPreview& operator=(const MacWebPreview&) = delete;
    [[nodiscard]] bool start(modules::WebPreviewAssets assets, std::string target,
                             unsigned width, unsigned height, Completion completion,
                             std::string& error);
    void cancel();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
