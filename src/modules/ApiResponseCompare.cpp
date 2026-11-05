#include "modules/ApiResponseCompare.hpp"
#include "editor/document/Document.hpp"
#include <algorithm>
namespace lucid::modules {
namespace {
constexpr std::size_t maxBody=4U*1024U*1024U;
bool plainText(std::string_view input) {
    std::string error;
    return std::none_of(input.begin(),input.end(),[](unsigned char c){return (c<32 && c!='\n' && c!='\r' && c!='\t') || c==127;}) &&
           editor::Document::canEditLosslessly(input,error);
}
std::size_t lineAt(std::string_view input,std::size_t offset) {
    return 1+static_cast<std::size_t>(std::count(input.begin(),input.begin()+static_cast<std::ptrdiff_t>(offset),'\n'));
}
}
ApiResponseComparison compareApiResponseBody(std::string_view baseline,const ApiResponse& response,const std::atomic_bool* cancel) {
    ApiResponseComparison result;
    const auto cancelled=[&]{return cancel && cancel->load();};
    if(cancelled()) {result.error="Response comparison cancelled";return result;}
    if(!response.error.empty() || response.status<100 || response.status>599 || baseline.size()>maxBody || response.body.size()>maxBody) {
        result.error="A complete bounded HTTP response and baseline are required";return result;
    }
    result.baselineBytes=baseline.size();result.currentBytes=response.body.size();
    const auto minSize=std::min(baseline.size(),response.body.size());
    while(result.commonPrefix<minSize && baseline[result.commonPrefix]==response.body[result.commonPrefix]) {
        if((result.commonPrefix&4095U)==0 && cancelled()) {result={};result.error="Response comparison cancelled";return result;}
        ++result.commonPrefix;
    }
    result.identical=result.commonPrefix==baseline.size() && result.commonPrefix==response.body.size();
    if(result.identical) return result;
    while(result.commonSuffix<minSize-result.commonPrefix && baseline[baseline.size()-1-result.commonSuffix]==response.body[response.body.size()-1-result.commonSuffix]) {
        if((result.commonSuffix&4095U)==0 && cancelled()) {result={};result.error="Response comparison cancelled";return result;}
        ++result.commonSuffix;
    }
    if(cancelled()) {result={};result.error="Response comparison cancelled";return result;}
    result.text=plainText(baseline) && plainText(response.body);
    if(result.text) {
        result.baselineFirstLine=lineAt(baseline,result.commonPrefix);
        result.currentFirstLine=lineAt(response.body,result.commonPrefix);
        result.baselineLastLine=lineAt(baseline,baseline.size()-result.commonSuffix);
        result.currentLastLine=lineAt(response.body,response.body.size()-result.commonSuffix);
    }
    return result;
}
}
