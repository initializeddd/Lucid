#include "modules/CrashSymbols.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
namespace lucid::modules {
namespace {
bool clean(std::string_view s) { return !s.empty() && s.size() <= 4096 && std::none_of(s.begin(),s.end(),[](unsigned char c) { return c < 32 || c == 127; }); }
bool safePath(std::string_view s) {
    if (!clean(s) || s.front()=='/' || s.front()=='\\') return false;
    std::size_t start=0;
    for (std::size_t i=0; i<=s.size(); ++i) {
        if (i==s.size() || s[i]=='/' || s[i]=='\\') { auto part=s.substr(start,i-start); if (part.empty() || part=="." || part=="..") return false; start=i+1; }
        else if (s[i]==':') return false;
    }
    return true;
}
bool number(std::string_view s, int base, std::uint64_t& value) { if (base==16 && s.starts_with("0x")) s.remove_prefix(2); if (s.empty()) return false; auto [p,e]=std::from_chars(s.data(),s.data()+s.size(),value,base); return e==std::errc{} && p==s.data()+s.size(); }
}
CrashSourceMap parseCrashSourceMap(std::string_view text, std::size_t maxBytes, std::size_t maxRanges) {
    CrashSourceMap out; maxBytes=std::min<std::size_t>(maxBytes,16*1024*1024); maxRanges=std::min<std::size_t>(maxRanges,100000);
    if (text.size()>maxBytes) { out.error="Source map exceeds byte limit"; return out; }
    std::size_t pos=0, lineNo=0;
    while (pos<text.size()) {
        auto end=text.find('\n',pos); auto line=text.substr(pos,end==std::string_view::npos ? text.size()-pos : end-pos); pos=end==std::string_view::npos ? text.size() : end+1; ++lineNo;
        if (!line.empty() && line.back()=='\r') line.remove_suffix(1);
        if (line.empty() || line.front()=='#') continue;
        std::array<std::string_view,6> fields{}; std::size_t p=0; bool valid=true;
        for (std::size_t i=0; i<fields.size(); ++i) { auto tab=line.find('\t',p); if (i+1<fields.size() && tab==std::string_view::npos) {valid=false;break;} if (i+1==fields.size() && tab!=std::string_view::npos) {valid=false;break;} fields[i]=line.substr(p,tab==std::string_view::npos ? line.size()-p : tab-p); p=tab==std::string_view::npos ? line.size() : tab+1; }
        std::uint64_t start=0, finish=0, sourceLine=0;
        if (!valid || !clean(fields[0]) || !safePath(fields[3]) || !clean(fields[5]) || !number(fields[1],16,start) || !number(fields[2],16,finish) || finish<=start || !number(fields[4],10,sourceLine) || sourceLine==0 || sourceLine>std::numeric_limits<std::size_t>::max()) { out.ranges.clear(); out.error="Invalid source map line " + std::to_string(lineNo); return out; }
        if (out.ranges.size()>=maxRanges) {out.incomplete=true; break;}
        out.ranges.push_back({std::string(fields[0]),std::string(fields[3]),std::string(fields[5]),start,finish,static_cast<std::size_t>(sourceLine)});
    }
    return out;
}
CrashLocation locateCrashAddress(std::uint64_t address, const std::vector<CrashModule>& modules, const CrashSourceMap& map) {
    CrashLocation out; const CrashModule* match=nullptr;
    for (const auto& m:modules) if (address>=m.base && address-m.base<m.size) { if (match) { out.moduleAmbiguous=true; return out; } match=&m; }
    if (!match) return out;
    out.module=match->name; out.moduleOffset=address-match->base;
    // A truncated or malformed map cannot establish that a match is unique.
    if (!map.error.empty() || map.incomplete) return out;
    const CrashSourceRange* source=nullptr;
    for (const auto& range:map.ranges) if (range.module==match->name && out.moduleOffset>=range.start && out.moduleOffset<range.end) {
        if (source) {out.sourceAmbiguous=true; return out;} source=&range;
    }
    if (source) {out.path=source->path; out.line=source->line; out.symbol=source->symbol;}
    return out;
}
}
