#include "modules/AppleIpsSymbols.hpp"
namespace lucid::modules {
CrashLocation locateIpsSource(const AppleIpsReport& report,const IpsFrame& frame,const CrashSourceMap& map){
 CrashLocation out;
 if(!report.error.empty()||frame.imageIndex>=report.images.size())return out;
 const auto& image=report.images[frame.imageIndex];
 if(image.name.empty()||frame.imageOffset>=image.size)return out;
 out.module=image.name;out.moduleOffset=frame.imageOffset;
 for(std::size_t i=0;i<report.images.size();++i)if(i!=frame.imageIndex&&report.images[i].name==image.name){out.moduleAmbiguous=true;return out;}
 // If images were truncated, a second identically named image could be missing.
 if(report.incomplete||!map.error.empty()||map.incomplete)return out;
 const CrashSourceRange* match=nullptr;
 for(const auto& range:map.ranges)if(range.module==image.name&&frame.imageOffset>=range.start&&frame.imageOffset<range.end){if(match){out.sourceAmbiguous=true;return out;}match=&range;}
 if(match){out.path=match->path;out.line=match->line;out.symbol=match->symbol;}
 return out;
}
}
