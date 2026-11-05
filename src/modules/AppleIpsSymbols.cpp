#include "modules/AppleIpsSymbols.hpp"
#include <algorithm>
namespace lucid::modules {
BreakpadIdentity compareIpsBreakpadIdentity(const AppleIpsReport& report,const BreakpadSymbols& symbols) {
 if(!report.error.empty() || report.incomplete || !symbols.error.empty() || symbols.incomplete || symbols.module.empty() || symbols.debugId.empty()) return BreakpadIdentity::Unavailable;
 if(symbols.operatingSystem!="mac") return BreakpadIdentity::Mismatch;
 const auto normalizeArch=[](std::string_view value)->std::string_view {
  if(value=="ARM-64" || value=="arm64" || value=="AArch64") return "arm64";
  if(value=="X86-64" || value=="x86_64" || value=="x86-64" || value=="amd64") return "x86_64";
  if(value=="X86" || value=="x86") return "x86";
  return {};
 };
 const auto a=normalizeArch(report.architecture), b=normalizeArch(symbols.architecture);
 if(a.empty() || b.empty()) return BreakpadIdentity::Unavailable;
 const IpsImage* image=nullptr;
 for(const auto& candidate:report.images) if(candidate.name==symbols.module) {if(image)return BreakpadIdentity::Unavailable;image=&candidate;}
 if(!image || image->uuid.empty()) return BreakpadIdentity::Unavailable;
 const auto& uuid=image->uuid;
 if(uuid.size()!=36 || uuid[8]!='-' || uuid[13]!='-' || uuid[18]!='-' || uuid[23]!='-') return BreakpadIdentity::Unavailable;
 std::string normalized;normalized.reserve(33);
 for(std::size_t i=0;i<uuid.size();++i) {
  if(i==8 || i==13 || i==18 || i==23) continue;
  const char c=uuid[i]; if(!((c>='0'&&c<='9') || (c>='a'&&c<='f') || (c>='A'&&c<='F'))) return BreakpadIdentity::Unavailable;
  normalized+=c>='a' && c<='f' ? static_cast<char>(c-'a'+'A') : c;
 }
 normalized+='0';
 std::string debugId=symbols.debugId;
 for(char& c:debugId) if(c>='a' && c<='f') c=static_cast<char>(c-'a'+'A');
 return a==b && normalized==debugId ? BreakpadIdentity::Match : BreakpadIdentity::Mismatch;
}
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
