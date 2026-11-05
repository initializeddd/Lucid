#include "modules/DesignTokenAliases.hpp"
#include <map>
namespace lucid::modules {
namespace {
constexpr std::size_t maxTokens=10000,maxBytes=4U*1024U*1024U,maxHops=64,noIndex=DesignAliasResult::noIndex;
enum class Kind { Literal, Alias, Unsupported };
Kind parseAlias(std::string_view value,std::string& name) {
    name.clear();
    if(value.find_first_of("{}") == value.npos) return Kind::Literal;
    if(value.size()<3 || value.front()!='{' || value.back()!='}') return Kind::Unsupported;
    const auto path=value.substr(1,value.size()-2);
    if(path.empty() || path.front()=='.' || path.back()=='.') return Kind::Unsupported;
    bool lastDot=false;
    for(unsigned char c:path) {
        if(c<=32 || c==127 || c=='{' || c=='}' || c=='[' || c==']' || c=='/' || c=='\\') return Kind::Unsupported;
        if(c=='.' && lastDot) return Kind::Unsupported;
        lastDot=c=='.';
    }
    name.assign(path); return Kind::Alias;
}
}
bool resolveDesignTokenAliases(const std::vector<DesignToken>& tokens,std::vector<DesignAliasResult>& results,std::string& error,const std::atomic_bool* cancel) {
    results.clear();error.clear(); const auto fail=[&](const char* message){results.clear();error=message;return false;};
    if(cancel && cancel->load()) return fail("Alias resolution cancelled");
    if(tokens.size()>maxTokens) return fail("Too many design tokens");
    std::size_t total=0; std::map<std::string,std::size_t> names;
    for(std::size_t i=0;i<tokens.size();++i) {
        if(cancel && cancel->load()) return fail("Alias resolution cancelled");
        const auto& token=tokens[i];
        if(token.name.empty() || token.name.size()>4096 || token.value.size()>4096 || token.name.size()>maxBytes-total) return fail("Invalid or excessive design tokens");
        total+=token.name.size(); if(token.value.size()>maxBytes-total) return fail("Invalid or excessive design tokens"); total+=token.value.size();
        auto [it,inserted]=names.emplace(token.name,i); if(!inserted) it->second=noIndex;
    }
    std::vector<DesignAliasResult> output(tokens.size()); std::vector<unsigned char> state(tokens.size());
    for(std::size_t i=0;i<tokens.size();++i) {
        auto& item=output[i];
        if(names.find(tokens[i].name)->second==noIndex) {item.status=DesignAliasStatus::Ambiguous; state[i]=2;continue;}
        std::string target; const auto kind=parseAlias(tokens[i].value,target);
        if(kind==Kind::Unsupported) {item.status=DesignAliasStatus::Unsupported;state[i]=2;}
        else if(kind==Kind::Literal) {item.status=DesignAliasStatus::Literal;item.resolvedIndex=i;state[i]=2;}
        else { const auto found=names.find(target); if(found==names.end()) {item.status=DesignAliasStatus::Missing;state[i]=2;} else if(found->second==noIndex) {item.status=DesignAliasStatus::Ambiguous;state[i]=2;} else item.targetIndex=found->second; }
    }
    for(std::size_t i=0;i<tokens.size();++i) {
        if(cancel && cancel->load()) return fail("Alias resolution cancelled");
        if(state[i]==2) continue;
        std::vector<std::size_t> trail; std::size_t at=i;
        while(state[at]==0) { if(cancel && cancel->load()) return fail("Alias resolution cancelled"); state[at]=1;trail.push_back(at);at=output[at].targetIndex; }
        if(state[at]==1) { for(auto index:trail) {output[index].status=DesignAliasStatus::Cycle; state[index]=2;} continue; }
        for(auto it=trail.rbegin();it!=trail.rend();++it) {
            auto& item=output[*it]; const auto& target=output[item.targetIndex];
            if(target.status==DesignAliasStatus::Literal || target.status==DesignAliasStatus::Resolved) {
                item.hops=target.hops+1;
                if(item.hops>maxHops) item.status=DesignAliasStatus::DepthLimit;
                else {item.status=DesignAliasStatus::Resolved;item.resolvedIndex=target.resolvedIndex;}
            } else item.status=target.status;
            state[*it]=2;
        }
    }
    results=std::move(output);return true;
}
}
