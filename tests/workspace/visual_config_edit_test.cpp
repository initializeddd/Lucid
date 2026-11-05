#include "modules/VisualConfigEdit.hpp"
#include <atomic>
using namespace lucid::modules;
int main(){
 VisualImage before{2,2,std::vector<std::uint8_t>(16,255)},after=before; after.rgba[0]=0; after.rgba[4]=20;
 const std::string original="{\n  \"version\":1, \"threshold\":0, \"masks\":[]\n}"; std::string error,replacement; VisualConfigDraft draft;
 if(!prepareVisualConfigDraft(original,before,after,{VisualConfigOperation::AddMask,{0,0,1,1}},draft,error) || draft.beforeCounts.changed!=2 || draft.afterCounts.changed!=1 || draft.afterCounts.masked!=1 || draft.after.masks.size()!=1) return 1;
 if(applyVisualConfigDraft(draft,original+" ",replacement,error) || !replacement.empty()) return 2;
 if(!applyVisualConfigDraft(draft,original,replacement,error)) return 3;
 VisualReviewConfig parsed; if(!parseVisualReviewConfig(replacement,parsed,error) || parsed.masks.size()!=1 || parsed.masks[0].x!=0) return 4;
 VisualConfigDraft removed; if(!prepareVisualConfigDraft(replacement,before,after,{VisualConfigOperation::RemoveMask,{},0},removed,error) || removed.afterCounts.changed!=2 || removed.afterCounts.masked!=0) return 5;
 VisualConfigDraft threshold; if(!prepareVisualConfigDraft(original,before,after,{VisualConfigOperation::SetThreshold,{},0,240},threshold,error) || threshold.afterCounts.changed!=1 || threshold.after.threshold!=240) return 6;
 for(const auto change:{VisualConfigChange{VisualConfigOperation::AddMask,{2,0,1,1}},VisualConfigChange{VisualConfigOperation::AddMask,{0,0,0,1}},VisualConfigChange{VisualConfigOperation::RemoveMask,{},0},VisualConfigChange{VisualConfigOperation::SetThreshold,{},0,256},VisualConfigChange{VisualConfigOperation::SetThreshold,{},0,0}}) if(prepareVisualConfigDraft(original,before,after,change,draft,error) || !draft.replacement.empty() || error.empty()) return 7;
 if(prepareVisualConfigDraft("{bad}",before,after,{VisualConfigOperation::SetThreshold,{},0,2},draft,error)) return 8;
 std::atomic_bool cancel{true}; if(prepareVisualConfigDraft(original,before,after,{VisualConfigOperation::SetThreshold,{},0,2},draft,error,&cancel) || !draft.original.empty()) return 9;
 return 0;
}
