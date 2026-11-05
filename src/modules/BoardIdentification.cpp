#include "modules/BoardIdentification.hpp"
#include "editor/json/Json.hpp"
#include <algorithm>
#include <set>
namespace lucid::modules {
namespace {
bool safeName(std::string_view s) { return !s.empty() && s.size()<=256 && std::none_of(s.begin(),s.end(),[](unsigned char c) {return c<32 || c==127;}); }
bool safePath(std::string_view s) {
    if (s.empty() || s.size()>1024 || s.front()=='/' || s.front()=='\\') return false;
    std::size_t start=0; for (std::size_t i=0;i<=s.size();++i) { if(i==s.size() || s[i]=='/' || s[i]=='\\') {auto part=s.substr(start,i-start); if(part.empty() || part=="." || part=="..") return false; start=i+1;} else if(s[i]==':' || static_cast<unsigned char>(s[i])<32 || s[i]==127) return false; } return true;
}
bool hexId(const editor::JsonValue* value, std::uint16_t& out) {
    if(!value || value->type!=editor::JsonType::String || value->stringValue.size()!=4) return false;
    unsigned n=0; for(char c:value->stringValue) {unsigned d=0; if(c>='0'&&c<='9') d=c-'0'; else if(c>='a'&&c<='f') d=c-'a'+10; else if(c>='A'&&c<='F') d=c-'A'+10; else return false; n=n*16+d; } if(!n) return false; out=static_cast<std::uint16_t>(n); return true;
}
}
BoardMappings parseBoardMappings(std::string_view json) {
    BoardMappings out; auto fail=[&](const char* e) {out.entries.clear();out.error=e;return out;};
    if(json.size()>256*1024) return fail("Board mapping exceeds byte limit"); editor::JsonError error; auto root=editor::parseJson(json,error);
    if(!root || root->type!=editor::JsonType::Object || root->children.size()!=1 || root->children[0].key!="boards") return fail("Invalid board mapping root");
    const auto& boards=root->children[0]; if(boards.type!=editor::JsonType::Array || boards.children.size()>1000) return fail("Invalid board mapping list");
    for(const auto& item:boards.children) {
        if(item.type!=editor::JsonType::Object) return fail("Invalid board mapping entry"); std::set<std::string> keys;
        for(const auto& field:item.children) if((field.key!="name" && field.key!="profile" && field.key!="vendorId" && field.key!="productId") || !keys.insert(field.key).second) return fail("Unknown or duplicate board mapping field");
        const auto* name=item.find("name"), *profile=item.find("profile"); BoardMapping entry;
        if(!name || name->type!=editor::JsonType::String || !safeName(name->stringValue) || !profile || profile->type!=editor::JsonType::String || !safePath(profile->stringValue) || !hexId(item.find("vendorId"),entry.vendor) || !hexId(item.find("productId"),entry.product)) return fail("Invalid board mapping fields");
        entry.name=name->stringValue; entry.profile=profile->stringValue; out.entries.push_back(std::move(entry));
    }
    return out;
}
BoardIdentity identifyBoard(ObservedUsbDevice device, const BoardMappings& mappings) {
    BoardIdentity out; if (!mappings.error.empty() || !device.vendor || !device.product) return out;
    const BoardMapping* match=nullptr;
    for (const auto& entry:mappings.entries) if(entry.vendor==device.vendor && entry.product==device.product) {if(match){out.state=BoardIdentity::State::Ambiguous;return out;}match=&entry;}
    if(match) {out.state=BoardIdentity::State::Match;out.name=match->name;out.profile=match->profile;} return out;
}
}
