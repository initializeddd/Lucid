#include "ui/VisualReviewCrop.hpp"
#include <atomic>
#include <limits>
using namespace lucid;
int main(){
 modules::VisualImage baseline{4,3,std::vector<std::uint8_t>(4*3*4,255)}, candidate=baseline;
 const auto i=(1*4+2)*4; baseline.rgba[i]=7; candidate.rgba[i]=8;
 modules::VisualComparison comparison; std::string error; if(!modules::compareVisualImages(baseline,candidate,0,{},comparison,error)) return 1;
 ui::VisualReviewPreview preview; if(!ui::makeVisualReviewCrop(baseline,candidate,comparison,{1,1,2,2},preview,error)) return 2;
 if(preview.baseline.width!=2 || preview.baseline.height!=2 || preview.baseline.rgba[4]!=7 || preview.candidate.rgba[4]!=8 || preview.difference.rgba[4]<=preview.candidate.rgba[4] || preview.difference.rgba[3]!=255 || !preview.generation) return 3;
 modules::VisualComparison masked; if(!modules::compareVisualImages(baseline,candidate,0,{{2,1,1,1}},masked,error)) return 8;
 ui::VisualReviewPreview maskedPreview; if(!ui::makeVisualReviewCrop(baseline,candidate,masked,{2,1,1,1},maskedPreview,error) || maskedPreview.difference.rgba!=maskedPreview.candidate.rgba) return 9;
 const auto generation=preview.generation; if(!ui::makeVisualReviewCrop(baseline,candidate,comparison,{2,1,1,1},preview,error) || preview.generation==generation || preview.baseline.rgba[0]!=7) return 4;
 for(const auto crop:{ui::VisualReviewCrop{0,0,0,1},ui::VisualReviewCrop{4,0,1,1},ui::VisualReviewCrop{3,2,2,1},ui::VisualReviewCrop{0,0,1025,1},ui::VisualReviewCrop{std::numeric_limits<std::size_t>::max(),0,2,1}}) if(ui::makeVisualReviewCrop(baseline,candidate,comparison,crop,preview,error) || preview.generation || error.empty()) return 5;
 std::atomic_bool cancel{true}; if(ui::makeVisualReviewCrop(baseline,candidate,comparison,{0,0,1,1},preview,error,&cancel) || preview.generation || error.empty()) return 6;
 candidate.rgba.pop_back(); if(ui::makeVisualReviewCrop(baseline,candidate,comparison,{0,0,1,1},preview,error)) return 7;
 return 0;
}
