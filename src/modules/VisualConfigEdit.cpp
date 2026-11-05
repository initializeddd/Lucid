#include "modules/VisualConfigEdit.hpp"
#include <algorithm>
namespace lucid::modules {
namespace {
VisualComparisonCounts counts(const VisualComparison& result) { return {result.comparedPixels,result.maskedPixels,result.changedPixels}; }
}
bool prepareVisualConfigDraft(std::string_view original,const VisualImage& baseline,const VisualImage& candidate,
    const VisualConfigChange& change,VisualConfigDraft& draft,std::string& error,const std::atomic_bool* cancel) {
    draft={}; error.clear(); if(cancel && cancel->load()) {error="Visual config edit cancelled";return false;}
    VisualConfigDraft result;
    const auto commonWidth=std::min(baseline.width,candidate.width), commonHeight=std::min(baseline.height,candidate.height);
    if(!parseVisualReviewConfig(original,result.before,error) || !validateVisualReviewConfig(result.before,commonWidth,commonHeight,error)) return false;
    result.after=result.before;
    switch(change.operation) {
    case VisualConfigOperation::AddMask: result.after.masks.push_back(change.mask); break;
    case VisualConfigOperation::RemoveMask:
        if(change.index>=result.after.masks.size()) {error="Mask no longer exists";return false;}
        result.after.masks.erase(result.after.masks.begin()+static_cast<std::ptrdiff_t>(change.index)); break;
    case VisualConfigOperation::SetThreshold:
        if(change.threshold>255) {error="Threshold must be between 0 and 255";return false;}
        if(change.threshold==result.after.threshold) {error="Threshold is unchanged";return false;}
        result.after.threshold=static_cast<std::uint8_t>(change.threshold); break;
    default: error="Unsupported visual config change"; return false;
    }
    if(!validateVisualReviewConfig(result.after,commonWidth,commonHeight,error) || !serializeVisualReviewConfig(result.after,result.replacement,error)) return false;
    VisualComparison comparison;
    if(!compareVisualImages(baseline,candidate,result.before.threshold,result.before.masks,comparison,error,cancel)) return false;
    result.beforeCounts=counts(comparison); comparison={};
    if(!compareVisualImages(baseline,candidate,result.after.threshold,result.after.masks,comparison,error,cancel)) return false;
    if(cancel && cancel->load()) {error="Visual config edit cancelled";return false;}
    result.afterCounts=counts(comparison); result.original=std::string(original); draft=std::move(result); return true;
}
bool applyVisualConfigDraft(const VisualConfigDraft& draft,std::string_view current,std::string& replacement,std::string& error) {
    replacement.clear(); error.clear();
    if(draft.original.empty() || draft.replacement.empty()) {error="Visual config draft is unavailable";return false;}
    if(current!=draft.original) {error="Visual configuration changed; review again";return false;}
    replacement=draft.replacement; return true;
}
}
