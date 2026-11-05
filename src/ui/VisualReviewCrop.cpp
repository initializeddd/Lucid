#include "ui/VisualReviewCrop.hpp"
#include <algorithm>
namespace lucid::ui {
namespace {
constexpr std::size_t maxPixels=16U*1024U*1024U;
bool valid(const modules::VisualImage& image) {
    return image.width && image.height && image.width<=maxPixels/image.height && image.rgba.size()==image.width*image.height*4;
}
}
bool makeVisualReviewCrop(const modules::VisualImage& baseline,const modules::VisualImage& candidate,
    const modules::VisualComparison& comparison,const VisualReviewCrop& crop,VisualReviewPreview& preview,
    std::string& error,const std::atomic_bool* cancel) {
    preview={}; error.clear();
    if(cancel && cancel->load()) {error="Visual crop cancelled";return false;}
    if(!valid(baseline) || !valid(candidate) || !valid(comparison.diff) || std::max(baseline.width,candidate.width)!=comparison.diff.width || std::max(baseline.height,candidate.height)!=comparison.diff.height || !crop.width || !crop.height || crop.width>1024 || crop.height>1024 || crop.x>=comparison.diff.width || crop.y>=comparison.diff.height || crop.width>comparison.diff.width-crop.x || crop.height>comparison.diff.height-crop.y) {error="Invalid visual crop or comparison";return false;}
    modules::VisualImage a{crop.width,crop.height,{}}, b{crop.width,crop.height,{}}; modules::VisualComparison subset; subset.diff={crop.width,crop.height,{}};
    for(auto* image:{&a,&b,&subset.diff}) image->rgba.resize(crop.width*crop.height*4);
    for(std::size_t y=0;y<crop.height;++y) {
        if(cancel && cancel->load()) {error="Visual crop cancelled";return false;}
        const auto dest=y*crop.width*4, sy=crop.y+y;
        for(const auto& pair:{std::pair{&baseline,&a},std::pair{&candidate,&b},std::pair{&comparison.diff,&subset.diff}}) {
            if(sy>=pair.first->height || crop.x>=pair.first->width) continue;
            const auto count=std::min(crop.width,pair.first->width-crop.x)*4, source=(sy*pair.first->width+crop.x)*4;
            std::copy_n(pair.first->rgba.begin()+static_cast<std::ptrdiff_t>(source),count,pair.second->rgba.begin()+static_cast<std::ptrdiff_t>(dest));
        }
    }
    VisualReviewPreview result;
    if(!makeVisualReviewPreview(a,b,subset,crop.width,crop.height,result,error)) return false;
    if(cancel && cancel->load()) {error="Visual crop cancelled";return false;}
    preview=std::move(result); return true;
}
}
