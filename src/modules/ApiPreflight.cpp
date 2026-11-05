#include "modules/ApiPreflight.hpp"
#include "editor/json/Json.hpp"
#include "text/Utf8.hpp"
#include <map>
#include <set>
namespace lucid::modules {
namespace {
using editor::JsonType;using editor::JsonValue;
constexpr std::size_t maxInput=1024U*1024U,maxRequests=100,maxVariables=128,maxReferences=128;
bool validName(std::string_view name) {if(name.empty() || name.size()>128)return false;for(unsigned char c:name)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return false;return true;}
bool unique(const JsonValue& value,std::size_t depth=0) {if(depth>32)return false;if(value.type==JsonType::Object){std::set<std::string> keys;for(const auto& child:value.children)if(!keys.insert(child.key).second || !unique(child,depth+1))return false;}else if(value.type==JsonType::Array)for(const auto& child:value.children)if(!unique(child,depth+1))return false;return true;}
bool safeName(std::string_view name) {if(name.empty() || name.size()>256)return false;for(std::size_t i=0;i<name.size();) {const auto start=i;const auto c=text::nextUtf8(name,i);if(c<32 || c==127 || (c==U'\ufffd' && i==start+1 && static_cast<unsigned char>(name[start])>=0x80U))return false;}return true;}
}
bool preflightApiEnvironment(std::string_view collection,const ApiEnvironment& environment,ApiPreflightReport& report,std::string& error,const std::atomic_bool* cancel) {
    report={};error.clear();const auto fail=[&](const char* message){error=message;return false;};
    if(cancel && cancel->load())return fail("Environment preflight cancelled");
    if(collection.size()>maxInput || environment.variables.size()>maxVariables)return fail("Environment preflight exceeds limits");
    editor::JsonError parseError;auto root=editor::parseJson(collection,parseError);
    if(!root || root->type!=JsonType::Object || !unique(*root))return fail("Invalid collection JSON");
    const auto* requests=root->find("requests");if(!requests || requests->type!=JsonType::Array || requests->children.size()>maxRequests)return fail("Invalid request list");
    std::map<std::string,bool> variables;
    if(const auto* defaults=root->find("variables")) {
        if(defaults->type!=JsonType::Object || defaults->children.size()>100)return fail("Invalid collection variables");
        for(const auto& item:defaults->children) {if(!validName(item.key) || item.type!=JsonType::String)return fail("Invalid collection variables");variables.emplace(item.key,false);}
    }
    std::size_t totalValues=0;
    for(const auto& [name,value]:environment.variables) {
        if(!validName(name) || value.value.size()>256U*1024U || value.value.size()>maxInput-totalValues)return fail("Invalid environment variables");
        totalValues+=value.value.size();variables[name]=value.secret;
    }
    ApiPreflightReport output;std::set<std::string> used;
    for(const auto& entry:requests->children) {
        if(cancel && cancel->load())return fail("Environment preflight cancelled");
        if(entry.type!=JsonType::Object)return fail("Invalid request fields");
        const auto* name=entry.find("name"), *method=entry.find("method"), *url=entry.find("url"), *headers=entry.find("headers"), *body=entry.find("body");
        if(!name || name->type!=JsonType::String || !safeName(name->stringValue) || !method || method->type!=JsonType::String || !url || url->type!=JsonType::String || (headers && (headers->type!=JsonType::Object || headers->children.size()>100)) || (body && body->type!=JsonType::String))return fail("Invalid request fields");
        ApiRequestPreflight item;item.name=name->stringValue;std::map<std::string,std::size_t> references;
        const auto scan=[&](std::string_view value,unsigned location) {
            for(std::size_t i=0;i<value.size();) {
                if(value.substr(i,2)=="{{") {
                    const auto end=value.find("}}",i+2);if(end==value.npos) {item.invalidPlaceholder=true;return true;}
                    auto name=value.substr(i+2,end-i-2);
                    if(name.starts_with("urlencode:")) {if(location!=ApiRefUrl) {item.invalidPlaceholder=true;return true;} name.remove_prefix(10);}
                    if(!validName(name)) {item.invalidPlaceholder=true;return true;}
                    auto [it,inserted]=references.emplace(std::string(name),item.references.size());
                    if(inserted) {if(item.references.size()>=maxReferences)return false;const auto found=variables.find(it->first);item.references.push_back({it->first,0,found==variables.end(),found!=variables.end() && found->second});}
                    auto& ref=item.references[it->second];ref.locations|=location;if(ref.secret && location==ApiRefUrl)item.secretInUrl=true;
                    if(environment.variables.find(it->first)!=environment.variables.end())used.insert(it->first);
                    i=end+2;
                } else if(value.substr(i,2)=="}}") {item.invalidPlaceholder=true;return true;} else ++i;
            }
            return true;
        };
        if(!scan(url->stringValue,ApiRefUrl))return fail("Too many request variable references");
        if(headers)for(const auto& header:headers->children) {if(header.type!=JsonType::String)return fail("Invalid request headers");if(!scan(header.stringValue,ApiRefHeader))return fail("Too many request variable references");}
        if(body && !scan(body->stringValue,ApiRefBody))return fail("Too many request variable references");
        if(item.invalidPlaceholder) output.unusedReliable=false;
        output.requests.push_back(std::move(item));
    }
    if(output.unusedReliable) for(const auto& [name,_]:environment.variables) if(!used.count(name))output.unusedEnvironmentVariables.push_back(name);
    report=std::move(output);return true;
}
}
