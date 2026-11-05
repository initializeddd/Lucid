#include "modules/DesignTokenAliases.hpp"
#include <atomic>
#include <string>
using namespace lucid::modules;
int main(){
 const std::string json=R"({"color":{"base":{"$value":"#abc","$type":"color"},"primary":{"$value":"{color.base}","$type":"color"},"button":{"$value":"{color.primary}"}},"missing":{"$value":"{none}"},"cycleA":{"$value":"{cycleB}"},"cycleB":{"$value":"{cycleA}"},"dependent":{"$value":"{cycleA}"},"expression":{"$value":"prefix {color.base}"},"n":4})";
 DesignTokenReport report; std::string error; if(!extractJsonDesignTokens(json,report,error)) return 1;
 std::vector<DesignAliasResult> result; if(!resolveDesignTokenAliases(report.tokens,result,error) || result.size()!=9) return 2;
 if(result[0].status!=DesignAliasStatus::Literal || result[1].status!=DesignAliasStatus::Resolved || result[1].targetIndex!=0 || result[1].resolvedIndex!=0 || result[2].hops!=2 || result[2].resolvedIndex!=0 || result[3].status!=DesignAliasStatus::Missing || result[4].status!=DesignAliasStatus::Cycle || result[5].status!=DesignAliasStatus::Cycle || result[6].status!=DesignAliasStatus::Cycle || result[7].status!=DesignAliasStatus::Unsupported) return 3;
 std::vector<DesignToken> duplicate={{"x","1","number"},{"x","2","number"},{"alias","{x}","string"}};
 if(!resolveDesignTokenAliases(duplicate,result,error) || result[0].status!=DesignAliasStatus::Ambiguous || result[2].status!=DesignAliasStatus::Ambiguous) return 4;
 std::vector<DesignToken> chain; for(int i=0;i<70;++i) chain.push_back({"t"+std::to_string(i), i==69?"value":"{t"+std::to_string(i+1)+"}","string"});
 if(!resolveDesignTokenAliases(chain,result,error) || result[0].status!=DesignAliasStatus::DepthLimit || result[5].status!=DesignAliasStatus::Resolved || result[5].hops!=64 || result[5].resolvedIndex!=69) return 5;
 for(const auto* expression:{"{a..b}","{a/../b}","{a b}","{a}{b}","{a", "}"}) { if(!resolveDesignTokenAliases({{"x",expression,"string"}},result,error) || result[0].status!=DesignAliasStatus::Unsupported) return 6; }
 std::vector<DesignToken> excessive(10001,{"x","1","number"}); if(resolveDesignTokenAliases(excessive,result,error) || !result.empty() || error.empty()) return 7;
 std::atomic_bool cancelled{true}; if(resolveDesignTokenAliases(report.tokens,result,error,&cancelled) || !result.empty()) return 8;
 return 0;
}
