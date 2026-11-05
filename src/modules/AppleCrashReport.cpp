#include "modules/AppleCrashReport.hpp"
#include <algorithm>
#include <charconv>
namespace lucid::modules {
namespace {
std::string_view trim(std::string_view s) {while(!s.empty()&&(s.front()==' '||s.front()=='\t'||s.front()=='\r'))s.remove_prefix(1);while(!s.empty()&&(s.back()==' '||s.back()=='\t'||s.back()=='\r'))s.remove_suffix(1);return s;}
bool safe(std::string_view s){return std::none_of(s.begin(),s.end(),[](unsigned char c){return (c<32 && c!='\t') || c==127;});}
template<class T> bool number(std::string_view s,T& value,int base=10){auto [p,e]=std::from_chars(s.data(),s.data()+s.size(),value,base);return !s.empty()&&e==std::errc{}&&p==s.data()+s.size();}
std::string_view token(std::string_view& s){s=trim(s);auto end=s.find_first_of(" \t");auto result=s.substr(0,end);s=end==std::string_view::npos?std::string_view{}:s.substr(end);return result;}
}
AppleCrashReport parseAppleCrashReport(std::string_view text,std::size_t maxBytes,std::size_t maxThreads,std::size_t maxFrames){
 AppleCrashReport out;maxBytes=std::min<std::size_t>(maxBytes,4*1024*1024);maxThreads=std::min<std::size_t>(maxThreads,256);maxFrames=std::min<std::size_t>(maxFrames,4096);if(text.size()>maxBytes){out.error="Crash report exceeds byte limit";return out;}
 AppleCrashThread* current=nullptr;std::size_t pos=0,frames=0,lines=0;bool sawHeader=false,sawThread=false;
 while(pos<text.size()){
  if(++lines>100000){out.incomplete=true;break;}auto end=text.find('\n',pos);if(end==std::string_view::npos)end=text.size();auto line=trim(text.substr(pos,end-pos));pos=end==text.size()?end:end+1;if(line.empty())continue;
  if(line.size()>4096||!safe(line)){++out.ignoredLines;out.incomplete=true;current=nullptr;continue;}
  if(line.starts_with("Exception Type:")){auto value=trim(line.substr(15));if(!value.empty()&&value.size()<=256){out.exceptionType=std::string(value);sawHeader=true;}else{out.incomplete=true;++out.ignoredLines;}current=nullptr;continue;}
  if(line.starts_with("Code Type:")){auto value=trim(line.substr(10));if(!value.empty()&&value.size()<=256){out.architecture=std::string(value);sawHeader=true;}else{out.incomplete=true;++out.ignoredLines;}current=nullptr;continue;}
  if(line.starts_with("Thread ")){current=nullptr;auto rest=line.substr(7);auto space=rest.find_first_of(" :\t");std::uint32_t n=0;if(space!=std::string_view::npos&&number(rest.substr(0,space),n)){auto suffix=trim(rest.substr(space));const bool crashed=suffix=="Crashed:"||suffix.starts_with("Crashed:: ");if(suffix==":"||suffix.starts_with(":: ")||crashed){sawThread=true;if(out.threads.size()<maxThreads){out.threads.push_back({n,crashed,{}});current=&out.threads.back();}else out.incomplete=true;continue;}}++out.ignoredLines;out.incomplete=true;continue;}
  if(current){auto rest=line;const auto indexText=token(rest);std::uint32_t index=0;if(number(indexText,index)){auto image=token(rest);auto addressText=token(rest);std::uint64_t address=0;if(image.empty()||image.size()>256||!addressText.starts_with("0x")||!number(addressText.substr(2),address,16)||trim(rest).empty()){++out.ignoredLines;out.incomplete=true;continue;}if(frames>=maxFrames){out.incomplete=true;continue;}current->frames.push_back({index,std::string(image),std::string(trim(rest)),address});++frames;continue;}current=nullptr;}
  // Legacy crash reports contain many unsupported sections; report omission.
  ++out.ignoredLines;out.incomplete=true;
 }
 if(!sawHeader||!sawThread)out.error="No supported Apple crash report records found";
 return out;
}
}
