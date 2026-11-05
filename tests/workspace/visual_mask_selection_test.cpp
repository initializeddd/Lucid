#include "ui/VisualMaskSelection.hpp"
#include <cmath>
#include <limits>
using namespace lucid;
int main(){auto pane=ui::visualReviewPaneRect(1200,800,1,400,200); if(pane.width<=0 || pane.height<=0 || pane.x<=24 || pane.y<=24) return 10;
 ui::VisualPreviewRect fitted{10,20,400,200},overlay;ui::VisualReviewCrop visible{100,50,200,100};modules::VisualMask mask;
 if(!ui::visualMaskFromDrag(fitted,400,200,visible,12,22,209,119,mask) || mask.x!=101 || mask.y!=51 || mask.width!=99 || mask.height!=49)return 1;
 modules::VisualMask reverse;if(!ui::visualMaskFromDrag(fitted,400,200,visible,209,119,12,22,reverse) || reverse.x!=mask.x || reverse.y!=mask.y || reverse.width!=mask.width || reverse.height!=mask.height)return 2;
 if(!ui::visualMaskOverlay(fitted,400,200,visible,mask,overlay) || overlay.x!=12 || overlay.y!=22 || overlay.width!=198 || overlay.height!=98)return 3;
 if(!ui::visualMaskFromDrag(fitted,400,200,visible,409.9,219.9,409.9,219.9,mask) || mask.x!=299 || mask.y!=149 || mask.width!=1 || mask.height!=1)return 4;
 if(ui::visualMaskFromDrag(fitted,400,200,visible,410,100,50,50,mask) || mask.width)return 5;
 if(!ui::visualMaskOverlay(fitted,400,200,visible,{50,40,100,20},overlay) || overlay.x!=10 || overlay.y!=20 || overlay.width!=100 || overlay.height!=20)return 6;
 if(ui::visualMaskOverlay(fitted,400,200,visible,{0,0,1,1},overlay) || overlay.width)return 7;
 if(ui::visualMaskFromDrag(fitted,400,200,{399,0,2,1},20,30,21,31,mask) || ui::visualMaskFromDrag(fitted,400,200,visible,std::numeric_limits<double>::quiet_NaN(),0,20,30,mask))return 8;
 if(ui::visualMaskOverlay({10,20,std::numeric_limits<double>::infinity(),20},400,200,visible,{100,50,1,1},overlay))return 9;
 return 0;}
