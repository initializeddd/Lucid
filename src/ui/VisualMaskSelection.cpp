#include "ui/VisualMaskSelection.hpp"
#include <algorithm>
#include <cmath>
namespace lucid::ui {
namespace {
constexpr std::size_t maxPixels=16U*1024U*1024U;
bool validRegion(std::size_t width,std::size_t height,VisualReviewCrop area) {
    return width && height && width<=maxPixels/height && area.width && area.height && area.x<width && area.y<height && area.width<=width-area.x && area.height<=height-area.y;
}
bool validRect(VisualPreviewRect r) {return std::isfinite(r.x)&&std::isfinite(r.y)&&std::isfinite(r.width)&&std::isfinite(r.height)&&r.width>0&&r.height>0 && std::isfinite(r.x+r.width) && std::isfinite(r.y+r.height);}
}
VisualPreviewRect visualReviewPaneRect(double width,double height,std::size_t pane,std::size_t imageWidth,std::size_t imageHeight) {
    if(!std::isfinite(width) || !std::isfinite(height) || pane>=3 || width<=168 || height<=168) return {};
    constexpr double inset=24,gap=12; const double x0=inset,y0=inset,x1=width-inset,y1=height-inset,top=y0+62;
    const double cellWidth=(x1-x0-4*gap)/3,cellHeight=y1-top-gap;
    if(cellWidth<=0 || cellHeight<=0) return {};
    return fitVisualPreview({x0+gap+pane*(cellWidth+gap),top,cellWidth,cellHeight},imageWidth,imageHeight);
}
bool visualMaskFromDrag(VisualPreviewRect fitted,std::size_t sourceWidth,std::size_t sourceHeight,VisualReviewCrop visible,double startX,double startY,double endX,double endY,modules::VisualMask& result) {
    result={}; if(!validRect(fitted) || !validRegion(sourceWidth,sourceHeight,visible)) return false;
    VisualPreviewPoint start,end;
    if(!visualPreviewSourcePoint(fitted,visible.width,visible.height,startX,startY,start) || !visualPreviewSourcePoint(fitted,visible.width,visible.height,endX,endY,end)) return false;
    const auto left=std::min(start.x,end.x), right=std::max(start.x,end.x), top=std::min(start.y,end.y), bottom=std::max(start.y,end.y);
    result={visible.x+left,visible.y+top,right-left+1,bottom-top+1};return true;
}
bool visualMaskOverlay(VisualPreviewRect fitted,std::size_t sourceWidth,std::size_t sourceHeight,VisualReviewCrop visible,const modules::VisualMask& mask,VisualPreviewRect& result) {
    result={}; if(!validRect(fitted) || !validRegion(sourceWidth,sourceHeight,visible) || !validRegion(sourceWidth,sourceHeight,{mask.x,mask.y,mask.width,mask.height})) return false;
    const auto left=std::max(visible.x,mask.x), top=std::max(visible.y,mask.y), right=std::min(visible.x+visible.width,mask.x+mask.width), bottom=std::min(visible.y+visible.height,mask.y+mask.height);
    if(left>=right || top>=bottom) return false;
    const auto sx=fitted.width/static_cast<double>(visible.width), sy=fitted.height/static_cast<double>(visible.height);
    result={fitted.x+static_cast<double>(left-visible.x)*sx,fitted.y+static_cast<double>(top-visible.y)*sy,static_cast<double>(right-left)*sx,static_cast<double>(bottom-top)*sy};
    return validRect(result);
}
}
