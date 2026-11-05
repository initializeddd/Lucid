#include "modules/JUnitImport.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
using namespace lucid::modules;
int main(){
 const auto old=R"(<testsuites><testsuite name="suite"><testcase classname="A" name="test_x" time="0.1"/><testcase classname="B" name="test_x"><failure message="failed">trace &amp; details</failure></testcase><testcase classname="C" name="skip"><skipped/></testcase><system-out><![CDATA[suite output]]></system-out></testsuite></testsuites>)";
 const auto next=R"(<testsuite><testcase classname="A" name="test_x"><failure>bad</failure></testcase><testcase classname="B" name="test_x"/><testcase classname="D" name="new"/><testcase classname="D" name="new"/></testsuite>)";
 JUnitReport a,b;std::string error;if(!importJUnitReport(old,a,error) || a.results.tests.size()!=3 || a.summary.passed!=1 || a.summary.failed!=1 || a.summary.skipped!=1 || a.summary.ambiguous || a.results.tests[1].failureText!="trace & details")return 1;
 if(!importJUnitReport(next,b,error) || b.summary.ambiguous!=1)return 2;
 std::vector<JUnitComparisonEntry> result;if(!compareJUnitReports(a,b,result,error) || result.size()!=4 || result[0].className!="A" || result[0].status!=TestHistoryStatus::Changed || result[1].className!="B" || result[1].status!=TestHistoryStatus::Changed || result[2].status!=TestHistoryStatus::PreviousOnly || result[3].status!=TestHistoryStatus::Ambiguous)return 3;
 if(importJUnitReport("<!DOCTYPE x [<!ENTITY e SYSTEM 'file:///etc/passwd'>]><testsuite/>",b,error) || !b.results.tests.empty())return 4;
 if(importJUnitReport("<testsuite><testcase name='x'></testsuite>",b,error) || !b.results.tests.empty())return 5;
 for(const auto* invalid:{"<testsuite><testcase name='&#1;'/></testsuite>","<testsuite><testcase name='&#xFFFE;'/></testsuite>","<testsuite><testcase name='<bad'/></testsuite>"}) if(importJUnitReport(invalid,b,error) || !b.results.tests.empty())return 8;
 if(importJUnitReport(std::string("<testsuite>")+char(1)+"</testsuite>",b,error) || importJUnitReport(std::string("<testsuite>")+char(0xff)+"</testsuite>",b,error))return 9;
 for(const auto* unsupported:{"<testsuite><testcase name='x'><flakyFailure>bad</flakyFailure></testcase></testsuite>","<testsuite><testcase name='x' status='failed'/></testsuite>","<testsuite><testcase name='x'><custom>bad</custom></testcase></testsuite>","<testsuite><unknown><testcase name='x'/></unknown></testsuite>","<testsuite><failure>bad</failure></testsuite>"}) if(importJUnitReport(unsupported,b,error) || !b.results.tests.empty())return 10;
 if(!importJUnitReport("<testsuite><properties><property name='k' value='v'/></properties><testcase name='x' classname='C' time='1' file='x.cpp' line='2' assertions='1'/><system-err>suite output</system-err></testsuite>",b,error) || b.summary.passed!=1)return 11;
 const auto fixtureDir=std::filesystem::path(__FILE__).parent_path().parent_path()/"fixtures/junit";
 for(const auto* name:{"pytest.xml","pytest-legacy.xml","jest-junit.xml","junit-platform.xml"}) {std::ifstream file(fixtureDir/name,std::ios::binary);std::string xml((std::istreambuf_iterator<char>(file)),{});if(!file.is_open() || !importJUnitReport(xml,b,error) || b.summary.passed!=1 || b.summary.failed!=1 || b.summary.skipped!=1)return 12;}
 {std::ifstream file(fixtureDir/"pytest-call-teardown.xml",std::ios::binary);std::string xml((std::istreambuf_iterator<char>(file)),{});if(!file.is_open() || !importJUnitReport(xml,b,error) || b.results.tests.size()!=2 || b.summary.failed!=2 || b.summary.ambiguous!=1)return 15;}
 const auto multiFailure="<testsuite tests='1' failures='1' errors='0' skipped='0'><testcase name='x'><failure>first</failure><failure>second</failure></testcase></testsuite>";
 if(!importJUnitReport(multiFailure,b,error) || b.summary.failed!=1 || b.results.tests[0].failureText.find("first\nsecond")==std::string::npos)return 13;
 for(const auto* invalid:{"<testsuite tests='1' failures='1'><testcase name='x'/></testsuite>","<testsuite tests='2'><testcase name='x'/></testsuite>","<testsuite failures='0'><testcase name='x'><failure/></testcase></testsuite>","<testsuite><testcase name='x'><failure/><error/></testcase></testsuite>"}) if(importJUnitReport(invalid,b,error) || !b.results.tests.empty())return 14;
 std::atomic_bool cancelled{true};if(importJUnitReport(old,b,error,&cancelled) || !b.results.tests.empty() || compareJUnitReports(a,a,result,error,&cancelled) || !result.empty())return 6;
 if(importJUnitReport(std::string(8U*1024U*1024U+1,'x'),b,error) || !b.results.tests.empty())return 7;
 return 0;
}
