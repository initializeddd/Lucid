#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
namespace lucid::platform {
// The caller supplies identity for the immutable reviewed context, including a
// fingerprint of the reviewed module manifest. Paths/names are identity values,
// not evidence of file safety. All calls belong to the same UI thread.
struct WebPreviewContext {
    std::string project, document, module, moduleFingerprint;
    std::uint64_t documentRevision=0, uiGeneration=0;
    bool operator==(const WebPreviewContext&) const = default;
};
class WebPreviewContextGate {
public:
    [[nodiscard]] bool begin(const WebPreviewContext& context,bool authorized,bool trusted,std::uint64_t& token) {
        if (active_ || !authorized || !trusted || context.project.empty() || context.document.empty() || context.module.empty() || context.moduleFingerprint.empty() ||
            context.project.size()>4096 || context.document.size()>4096 || context.module.size()>512 || context.moduleFingerprint.size()!=64 ||
            !std::all_of(context.moduleFingerprint.begin(),context.moduleFingerprint.end(),[](char c) { return (c>='0' && c<='9') || (c>='a' && c<='f'); }) || generation_==std::numeric_limits<std::uint64_t>::max()) return false;
        context_=context; active_=true; token=++generation_; return true;
    }
    [[nodiscard]] bool matches(const WebPreviewContext& context,bool authorized,std::uint64_t token) const noexcept { return active_ && authorized && token==generation_ && context==context_; }
    [[nodiscard]] bool active() const noexcept { return active_; }
    void cancel() noexcept { active_=false; }
private:
    WebPreviewContext context_;
    std::uint64_t generation_=0;
    bool active_=false;
};
}
