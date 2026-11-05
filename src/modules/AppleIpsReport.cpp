#include "modules/AppleIpsReport.hpp"
#include "editor/json/Json.hpp"
#include <algorithm>
#include <charconv>
#include <set>
namespace lucid::modules {
namespace {
using editor::JsonType;using editor::JsonValue;
bool unique(const JsonValue& v){if(v.type==JsonType::Object){std::set<std::string_view> keys;for(const auto& c:v.children)if(!keys.insert(c.key).second||!unique(c))return false;}else if(v.type==JsonType::Array)for(const auto& c:v.children)if(!unique(c))return false;return true;}
const JsonValue* member(const JsonValue& obj,const char* name,JsonType type){if(obj.type!=JsonType::Object)return nullptr;auto v=obj.find(name);return v&&v->type==type?v:nullptr;}
bool integer(const JsonValue* v,std::string_view source,std::uint64_t& result){if(!v||v->type!=JsonType::Number||v->byteOffset>=source.size())return false;auto s=source.substr(v->byteOffset);std::size_t n=0;while(n<s.size()&&s[n]>='0'&&s[n]<='9')++n;if(!n)return false;auto [p,e]=std::from_chars(s.data(),s.data()+n,result);return e==std::errc{}&&p==s.data()+n&&(n==s.size()||s[n]==','||s[n]=='}'||s[n]==']'||s[n]==' '||s[n]=='\r'||s[n]=='\n'||s[n]=='\t');}
bool copyText(const JsonValue* v,std::string& dest,std::size_t max){if(!v||v->type!=JsonType::String||v->stringValue.size()>max||std::any_of(v->stringValue.begin(),v->stringValue.end(),[](unsigned char c){return c<32||c==127;}))return false;dest=v->stringValue;return true;}
}
AppleIpsReport parseAppleIpsReport(std::string_view text,std::size_t maxBytes,std::size_t maxThreads,std::size_t maxFrames,std::size_t maxImages){
 AppleIpsReport out;maxBytes=std::min<std::size_t>(maxBytes,4*1024*1024);maxThreads=std::min<std::size_t>(maxThreads,256);maxFrames=std::min<std::size_t>(maxFrames,4096);maxImages=std::min<std::size_t>(maxImages,4096);if(text.size()>maxBytes){out.error="IPS report exceeds byte limit";return out;}
 editor::JsonError error;auto payload=text;auto root=editor::parseJson(payload,error);
 if(!root){auto newline=text.find('\n');if(newline==std::string_view::npos){out.error="Invalid IPS JSON";return out;}auto header=editor::parseJson(text.substr(0,newline),error);if(!header||header->type!=JsonType::Object||!unique(*header)){out.error="Invalid IPS metadata header";return out;}payload=text.substr(newline+1);root=editor::parseJson(payload,error);}
 if(!root||root->type!=JsonType::Object||!unique(*root)){out.error="Invalid or ambiguous IPS JSON";return out;}
 auto threads=member(*root,"threads",JsonType::Array);auto images=member(*root,"usedImages",JsonType::Array);if(!threads||!images){out.error="IPS threads or images missing";return out;}
 if(auto arch=root->find("cpuType");arch&&!copyText(arch,out.architecture,256))out.incomplete=true;
 if(auto exception=member(*root,"exception",JsonType::Object)){if(auto type=exception->find("type");type&&!copyText(type,out.exceptionType,256))out.incomplete=true;if(auto signal=exception->find("signal");signal&&!copyText(signal,out.signal,256))out.incomplete=true;}
 for(const auto& image:images->children){if(out.images.size()>=maxImages){out.incomplete=true;break;}IpsImage item;if(image.type!=JsonType::Object||!integer(image.find("base"),payload,item.base)||!integer(image.find("size"),payload,item.size)){out.error="Invalid IPS image entry";return out;}if(auto name=image.find("name");name&&!copyText(name,item.name,256))out.incomplete=true;if(auto uuid=image.find("uuid");uuid&&!copyText(uuid,item.uuid,128))out.incomplete=true;out.images.push_back(std::move(item));}
 std::size_t count=0;for(const auto& thread:threads->children){if(out.threads.size()>=maxThreads){out.incomplete=true;break;}if(thread.type!=JsonType::Object){out.error="Invalid IPS thread entry";return out;}IpsThread item;if(auto triggered=thread.find("triggered")){if(triggered->type==JsonType::Bool)item.triggered=triggered->boolValue;else out.incomplete=true;}auto entries=member(thread,"frames",JsonType::Array);if(!entries){out.incomplete=true;out.threads.push_back(std::move(item));continue;}for(const auto& frame:entries->children){if(count>=maxFrames){out.incomplete=true;break;}std::uint64_t index=0,offset=0;if(frame.type!=JsonType::Object||!integer(frame.find("imageIndex"),payload,index)||!integer(frame.find("imageOffset"),payload,offset)||index>=images->children.size()){out.incomplete=true;continue;}if(index>=out.images.size()){out.incomplete=true;continue;}if(offset>=out.images[static_cast<std::size_t>(index)].size){out.incomplete=true;continue;}IpsFrame f;f.imageIndex=static_cast<std::size_t>(index);f.imageOffset=offset;if(auto symbol=frame.find("symbol");symbol&&!copyText(symbol,f.symbol,1024))out.incomplete=true;item.frames.push_back(std::move(f));++count;}out.threads.push_back(std::move(item));}
 return out;
}
}
