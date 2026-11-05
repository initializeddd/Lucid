#include "modules/ApiPreflight.hpp"
#include <atomic>
#include <string>
using namespace lucid::modules;
int main(){
 const std::string collection=R"({"variables":{"DEFAULT":"base","TOKEN":"nonsecret"},"requests":[{"name":"one","method":"POST","url":"https://example.com/?token={{TOKEN}}&d={{DEFAULT}}&x={{MISSING}}","headers":{"Authorization":"Bearer {{TOKEN}}"},"body":"{{TOKEN}}"},{"name":"bad","method":"GET","url":"https://example.com/{{BROKEN"}]})";
 ApiEnvironment env{"dev",{{"TOKEN",{"sensitive-value",true}},{"UNUSED",{"other-secret",true}}}};ApiPreflightReport report;std::string error;
 if(!preflightApiEnvironment(collection,env,report,error) || report.requests.size()!=2 || !report.requests[0].secretInUrl || report.requests[0].references.size()!=3 || !report.requests[1].invalidPlaceholder || report.unusedReliable || !report.unusedEnvironmentVariables.empty())return 1;
 const auto& token=report.requests[0].references[0];if(token.name!="TOKEN" || token.missing || !token.secret || token.locations!=(ApiRefUrl|ApiRefHeader|ApiRefBody))return 2;
 if(report.requests[0].references[1].missing || !report.requests[0].references[2].missing)return 3;
 for(const auto& request:report.requests) {if(request.name.find("sensitive-value")!=std::string::npos)return 4;for(const auto& ref:request.references)if(ref.name.find("sensitive-value")!=std::string::npos)return 5;}
 for(const auto* invalid:{R"({"requests":[{"name":"x","method":"GET","url":"x","headers":{"X":42}}]})",R"({"requests":[],"requests":[]})",R"({"variables":{"BAD!":"secret"},"requests":[]})"}) if(preflightApiEnvironment(invalid,env,report,error) || !report.requests.empty() || error.find("secret")!=std::string::npos)return 6;
 if(!preflightApiEnvironment(R"({"requests":[{"name":"x","method":"GET","url":"x}}"}]})",env,report,error) || !report.requests[0].invalidPlaceholder)return 7;
 if(!preflightApiEnvironment(R"({"requests":[{"name":"ok","method":"GET","url":"https://x/{{TOKEN}}"}]})",env,report,error) || !report.unusedReliable || !report.requests[0].secretInUrl || report.unusedEnvironmentVariables.size()!=1 || report.unusedEnvironmentVariables[0]!="UNUSED")return 10;
 if(!preflightApiEnvironment(R"({"requests":[{"name":"encoded","method":"GET","url":"https://x/?q={{urlencode:TOKEN}}"}]})",env,report,error) || report.requests[0].invalidPlaceholder || !report.requests[0].secretInUrl || report.requests[0].references.size()!=1 || report.requests[0].references[0].name!="TOKEN" || !report.requests[0].references[0].secret)return 11;
 if(!preflightApiEnvironment(R"({"requests":[{"name":"bad","method":"POST","url":"https://x/","body":"{{urlencode:TOKEN}}"}]})",env,report,error) || !report.requests[0].invalidPlaceholder)return 12;
 std::atomic_bool cancelled{true};if(preflightApiEnvironment(collection,env,report,error,&cancelled) || !report.requests.empty())return 8;
 env.variables["TOKEN"].value=std::string(256U*1024U+1,'x');if(preflightApiEnvironment(collection,env,report,error) || !report.requests.empty())return 9;
 return 0;
}
