#include "modules/UnixCrashReport.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
namespace lucid::modules {
namespace {
std::string_view trim(std::string_view s) { while(!s.empty() && (s.front()==' ' || s.front()=='\t' || s.front()=='\r')) s.remove_prefix(1); while(!s.empty() && (s.back()==' ' || s.back()=='\t' || s.back()=='\r')) s.remove_suffix(1); return s; }
bool safe(std::string_view s) { return std::none_of(s.begin(),s.end(),[](unsigned char c){return c<32 || c==127;}); }
template<class T> bool decimal(std::string_view s,T& n) {auto [p,e]=std::from_chars(s.data(),s.data()+s.size(),n);return !s.empty() && e==std::errc{} && p==s.data()+s.size();}
}
UnixCrashReport parseGdbBacktrace(std::string_view text,std::size_t maxBytes,std::size_t maxThreads,std::size_t maxFrames) {
    UnixCrashReport out; maxBytes=std::min<std::size_t>(maxBytes,4*1024*1024); maxThreads=std::min<std::size_t>(maxThreads,256); maxFrames=std::min<std::size_t>(maxFrames,4096);
    if(text.size()>maxBytes) {out.error="Backtrace exceeds byte limit";return out;}
    std::size_t pos=0,frames=0,lines=0; UnixCrashThread* current=nullptr; bool recognized=false;
    while(pos<text.size()) {
        if(++lines>100000) {out.incomplete=true;break;}
        auto end=text.find('\n',pos);if(end==std::string_view::npos)end=text.size();auto line=trim(text.substr(pos,end-pos));pos=end==text.size()?end:end+1;
        if(line.empty())continue;
        if(line.size()>4096 || !safe(line)) {++out.ignoredLines;out.incomplete=true;continue;}
        if(line.starts_with("Program received signal ")) {auto sig=line.substr(24); auto comma=sig.find(',');sig=trim(sig.substr(0,comma));if(!sig.empty() && sig.size()<=64 && std::all_of(sig.begin(),sig.end(),[](unsigned char c){return (c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_';})){out.signal=std::string(sig);recognized=true;}else {++out.ignoredLines;out.incomplete=true;}continue;}
        if(line.starts_with("Thread ")) {current=nullptr;auto rest=line.substr(7);auto space=rest.find_first_of(" \t");std::uint32_t number=0; if(space!=std::string_view::npos && decimal(rest.substr(0,space),number) && number && line.back()==':' && trim(rest.substr(space)).starts_with("(")) {recognized=true;current=nullptr;if(out.threads.size()<maxThreads){out.threads.push_back({number,std::string(line),{}});current=&out.threads.back();}else out.incomplete=true;continue;} }
        if(line.front()=='#') {auto space=line.find_first_of(" \t");std::uint32_t number=0;if(space==std::string_view::npos || !decimal(line.substr(1,space-1),number)){++out.ignoredLines;out.incomplete=true;continue;}recognized=true;if(!current) {if(out.threads.empty() && out.threads.size()<maxThreads){out.threads.push_back({0,"Unlabelled thread",{}});current=&out.threads.back();}else {out.incomplete=true;continue;}} if(frames>=maxFrames){out.incomplete=true;continue;}auto body=trim(line.substr(space));std::optional<std::uint64_t> address;if(body.starts_with("0x")){auto stop=body.find_first_of(" \t");auto hex=body.substr(2,stop==std::string_view::npos?std::string_view::npos:stop-2);std::uint64_t value=0;auto [p,e]=std::from_chars(hex.data(),hex.data()+hex.size(),value,16);if(hex.empty() || e!=std::errc{} || p!=hex.data()+hex.size()){++out.ignoredLines;out.incomplete=true;continue;}address=value;}if(body.empty()){++out.ignoredLines;out.incomplete=true;continue;}current->frames.push_back({number,address,std::string(body)});++frames;continue;}
        ++out.ignoredLines;out.incomplete=true;
    }
    if(!recognized)out.error="No supported GDB backtrace records found";
    return out;
}
UnixCrashReport parseLldbBacktrace(std::string_view text,std::size_t maxBytes,std::size_t maxThreads,std::size_t maxFrames) {
    UnixCrashReport out; maxBytes=std::min<std::size_t>(maxBytes,4*1024*1024); maxThreads=std::min<std::size_t>(maxThreads,256); maxFrames=std::min<std::size_t>(maxFrames,4096);
    if(text.size()>maxBytes) {out.error="Backtrace exceeds byte limit";return out;}
    std::size_t pos=0,frames=0,lines=0; UnixCrashThread* current=nullptr; bool recognized=false;
    while(pos<text.size()) {
        if(++lines>100000) {out.incomplete=true;break;}
        auto end=text.find('\n',pos); if(end==text.npos)end=text.size(); auto line=trim(text.substr(pos,end-pos)); pos=end==text.size()?end:end+1;
        if(line.empty() || line=="(lldb) thread backtrace all" || line=="(lldb) bt all") continue;
        if(line.size()>4096 || !safe(line)) {++out.ignoredLines;out.incomplete=true;current=nullptr;continue;}
        if(line.starts_with("* ")) line=trim(line.substr(2));
        if(line.starts_with("thread #")) {
            current=nullptr; auto rest=line.substr(8); auto sep=rest.find_first_of(",:"); std::uint32_t number=0;
            if(sep==rest.npos || !decimal(rest.substr(0,sep),number) || !number) {++out.ignoredLines;out.incomplete=true;continue;}
            recognized=true;
            if(std::any_of(out.threads.begin(),out.threads.end(),[&](const auto& t){return t.number==number;}) || out.threads.size()>=maxThreads) {out.incomplete=true;continue;}
            out.threads.push_back({number,std::string(line),{}}); current=&out.threads.back(); continue;
        }
        if(line.starts_with("frame #")) {
            auto rest=line.substr(7);auto colon=rest.find(':');std::uint32_t number=0;
            if(colon==rest.npos || !decimal(rest.substr(0,colon),number) || !current) {++out.ignoredLines;out.incomplete=true;continue;}
            auto body=trim(rest.substr(colon+1)); if(body.empty()) {++out.ignoredLines;out.incomplete=true;continue;}
            std::optional<std::uint64_t> address;
            if(body.starts_with("0x")) {auto stop=body.find_first_of(" \t");auto hex=body.substr(2,stop==body.npos?body.npos:stop-2);std::uint64_t value=0;auto [p,e]=std::from_chars(hex.data(),hex.data()+hex.size(),value,16);if(hex.empty() || e!=std::errc{} || p!=hex.data()+hex.size()){++out.ignoredLines;out.incomplete=true;continue;}address=value;}
            if(frames>=maxFrames) {out.incomplete=true;continue;}
            current->frames.push_back({number,address,std::string(body)});++frames;continue;
        }
        ++out.ignoredLines;out.incomplete=true;
    }
    if(!recognized)out.error="No supported LLDB backtrace records found";
    return out;
}
}
