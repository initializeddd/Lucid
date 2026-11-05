#include "modules/BreakpadSymbols.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <unordered_map>
namespace lucid::modules {
namespace {
std::string_view token(std::string_view& s) { while (!s.empty() && s.front()==' ') s.remove_prefix(1); auto p=s.find(' '); auto t=s.substr(0,p); s=p==std::string_view::npos ? std::string_view{} : s.substr(p+1); return t; }
bool numeric(std::string_view s, int base, std::uint64_t& n) { if (s.empty()) return false; auto [p,e]=std::from_chars(s.data(),s.data()+s.size(),n,base); return e==std::errc{} && p==s.data()+s.size(); }
bool clean(std::string_view s) { return !s.empty() && s.size()<=4096 && std::none_of(s.begin(),s.end(),[](unsigned char c){return c<32 || c==127;}); }
bool pathSafe(std::string_view s) {
    if (!clean(s) || s.front()=='/' || s.front()=='\\') return false; std::size_t start=0;
    for (std::size_t i=0; i<=s.size(); ++i) { if (i==s.size() || s[i]=='/' || s[i]=='\\') { auto p=s.substr(start,i-start); if (p.empty() || p=="." || p=="..") return false; start=i+1; } else if (s[i]==':') return false; } return true;
}
struct Pending { std::uint64_t start,end,file,line; std::string symbol; };
}
BreakpadSymbols parseBreakpadSymbols(std::string_view text, std::size_t maxBytes, std::size_t maxRanges) {
    BreakpadSymbols out; maxBytes=std::min<std::size_t>(maxBytes,16*1024*1024); maxRanges=std::min<std::size_t>(maxRanges,100000);
    auto fail=[&](const std::string& error) { out.error=error; out.sourceMap={}; return out; };
    if (text.size()>maxBytes) return fail("Breakpad symbols exceed byte limit");
    std::unordered_map<std::uint64_t,std::string> files; std::vector<Pending> pending; std::string module, function; std::uint64_t functionStart=0,functionEnd=0; bool haveFunction=false; std::size_t pos=0, lines=0;
    while(pos<text.size()) {
        if (++lines>200000) {out.incomplete=true; break;}
        auto end=text.find('\n',pos); auto row=text.substr(pos,end==std::string_view::npos ? text.size()-pos : end-pos); pos=end==std::string_view::npos ? text.size() : end+1; if (!row.empty() && row.back()=='\r') row.remove_suffix(1); if (row.empty()) continue;
        auto rest=row; auto kind=token(rest);
        if (kind=="MODULE") {
            auto os=token(rest), arch=token(rest), id=token(rest); while(!rest.empty() && rest.front()==' ') rest.remove_prefix(1);
            if (!module.empty() || !clean(os) || !clean(arch) || !clean(id) || !clean(rest)) return fail("Invalid Breakpad MODULE");
            module=std::string(rest); out.module=module; out.architecture=std::string(arch); out.debugId=std::string(id); haveFunction=false;
        } else if (kind=="FILE") {
            std::uint64_t id=0; auto number=token(rest); while(!rest.empty() && rest.front()==' ') rest.remove_prefix(1);
            if (!numeric(number,10,id) || !pathSafe(rest) || files.size()>=100000 || !files.emplace(id,std::string(rest)).second) return fail("Invalid Breakpad FILE"); haveFunction=false;
        } else if (kind=="FUNC") {
            auto a=token(rest); if (a=="m") a=token(rest); auto size=token(rest), params=token(rest); while(!rest.empty() && rest.front()==' ') rest.remove_prefix(1);
            std::uint64_t start=0,length=0,ignored=0;
            if (!numeric(a,16,start) || !numeric(size,16,length) || !length || length>std::numeric_limits<std::uint64_t>::max()-start || !numeric(params,16,ignored) || !clean(rest)) return fail("Invalid Breakpad FUNC");
            functionStart=start; functionEnd=start+length; function=std::string(rest); haveFunction=true;
        } else if (haveFunction && !kind.empty() && ((kind.front()>='0' && kind.front()<='9') || (kind.front()>='a' && kind.front()<='f') || (kind.front()>='A' && kind.front()<='F'))) {
            auto size=token(rest), line=token(rest), file=token(rest); std::uint64_t start=0,length=0,lineNumber=0,fileId=0;
            if (!rest.empty() || !numeric(kind,16,start) || !numeric(size,16,length) || !length || length>std::numeric_limits<std::uint64_t>::max()-start || start<functionStart || start+length>functionEnd || !numeric(line,10,lineNumber) || !lineNumber || lineNumber>std::numeric_limits<std::size_t>::max() || !numeric(file,10,fileId)) return fail("Invalid Breakpad line record");
            if (pending.size()>=maxRanges) {out.incomplete=true; break;}
            pending.push_back({start,start+length,fileId,lineNumber,function});
        } else { haveFunction=false; }
    }
    if (module.empty()) return fail("Breakpad MODULE missing");
    for (const auto& p:pending) { auto f=files.find(p.file); if (f==files.end()) return fail("Breakpad line references missing FILE"); out.sourceMap.ranges.push_back({module,f->second,p.symbol,p.start,p.end,static_cast<std::size_t>(p.line)}); }
    out.sourceMap.incomplete=out.incomplete; return out;
}
BreakpadIdentity compareBreakpadIdentity(const BreakpadSymbols& symbols, const CrashModule& module, std::string_view dumpArchitecture) {
    if (!symbols.error.empty() || symbols.incomplete || symbols.module.empty() || symbols.debugId.empty() || module.debugId.empty() || dumpArchitecture.empty()) return BreakpadIdentity::Unavailable;
    auto basename=[](std::string_view s) { auto p=s.find_last_of("/\\"); return p==std::string_view::npos ? s : s.substr(p+1); };
    auto arch=[](std::string_view a)->std::string_view { if (a=="x86_64" || a=="amd64" || a=="x86-64") return "x86-64"; if (a=="arm64" || a=="AArch64") return "AArch64"; if (a=="x86") return "x86"; if (a=="arm" || a=="ARM") return "ARM"; return {}; };
    auto a=arch(symbols.architecture), b=arch(dumpArchitecture);
    if (a.empty() || b.empty()) return BreakpadIdentity::Unavailable;
    return basename(module.name)==symbols.module && module.debugId==symbols.debugId && a==b ? BreakpadIdentity::Match : BreakpadIdentity::Mismatch;
}

}
