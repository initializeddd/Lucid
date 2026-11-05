#include "modules/JUnitImport.hpp"
#include <map>
namespace lucid::modules {
namespace {
constexpr std::size_t maxTests=10000,maxName=4096,maxBytes=8U*1024U*1024U;
using Identity=std::pair<std::string,std::string>;
bool valid(const TestResults& results) {
    if(results.tests.size()>maxTests)return false;
    std::size_t bytes=0;
    for(const auto& t:results.tests) {
        if(t.name.empty() || t.name.size()>maxName || t.className.size()>maxName || t.name.find('\0')!=t.name.npos || t.className.find('\0')!=t.className.npos) return false;
        if(t.name.size()>maxBytes-bytes)return false;bytes+=t.name.size();
        if(t.className.size()>maxBytes-bytes)return false;bytes+=t.className.size();
        if(t.status!=TestResultStatus::Passed && t.status!=TestResultStatus::Failed && t.status!=TestResultStatus::Skipped)return false;
    }
    return true;
}
}
bool importJUnitReport(std::string_view xml,JUnitReport& report,std::string& error,const std::atomic_bool* cancel) {
    report={};error.clear();if(cancel && cancel->load()){error="JUnit import cancelled";return false;}
    JUnitReport output;if(!parseStrictJUnit(xml,output.results,error))return false;
    if(cancel && cancel->load()){error="JUnit import cancelled";return false;}
    if(!valid(output.results)){error="Invalid or excessive JUnit identities";return false;}
    std::map<Identity,std::size_t> counts;
    for(const auto& test:output.results.tests) {if(cancel && cancel->load()){error="JUnit import cancelled";return false;} ++counts[{test.className,test.name}];switch(test.status){case TestResultStatus::Passed:++output.summary.passed;break;case TestResultStatus::Failed:++output.summary.failed;break;case TestResultStatus::Skipped:++output.summary.skipped;break;}}
    for(const auto& [_,count]:counts) if(count>1)++output.summary.ambiguous;
    report=std::move(output);return true;
}
bool compareJUnitReports(const JUnitReport& previous,const JUnitReport& current,std::vector<JUnitComparisonEntry>& comparison,std::string& error,const std::atomic_bool* cancel) {
    comparison.clear();error.clear();if(cancel && cancel->load()){error="JUnit comparison cancelled";return false;}
    if(!valid(previous.results) || !valid(current.results)){error="Invalid or excessive JUnit identities";return false;}
    std::map<Identity,JUnitComparisonEntry> entries;
    for(std::size_t i=0;i<previous.results.tests.size();++i) {if(cancel && cancel->load()){error="JUnit comparison cancelled";return false;}const auto& t=previous.results.tests[i];auto& e=entries[{t.className,t.name}];e.className=t.className;e.name=t.name;e.previousIndices.push_back(i);}
    for(std::size_t i=0;i<current.results.tests.size();++i) {if(cancel && cancel->load()){error="JUnit comparison cancelled";return false;}const auto& t=current.results.tests[i];auto& e=entries[{t.className,t.name}];e.className=t.className;e.name=t.name;e.currentIndices.push_back(i);}
    std::vector<JUnitComparisonEntry> output;output.reserve(entries.size());
    for(auto& [_,e]:entries) {
        if(cancel && cancel->load()){error="JUnit comparison cancelled";return false;}
        if(e.previousIndices.size()>1 || e.currentIndices.size()>1)e.status=TestHistoryStatus::Ambiguous;
        else if(e.previousIndices.empty())e.status=TestHistoryStatus::CurrentOnly;
        else if(e.currentIndices.empty())e.status=TestHistoryStatus::PreviousOnly;
        else e.status=previous.results.tests[e.previousIndices[0]].status==current.results.tests[e.currentIndices[0]].status?TestHistoryStatus::Unchanged:TestHistoryStatus::Changed;
        output.push_back(std::move(e));
    }
    comparison=std::move(output);return true;
}
}
