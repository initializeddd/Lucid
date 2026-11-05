#include "modules/ApiAssertions.hpp"
#include <atomic>
#include <string>
#include <vector>
using namespace lucid::modules;
int main() {
 std::string error; std::vector<ApiAssertion> tests;
 const std::string spec=R"({"assertions":[{"type":"status","value":200},{"type":"body_contains","value":"secret"},{"type":"json_string_equals","pointer":"/a~1b/~0key/0","value":"secret"},{"type":"json_string_equals","pointer":"/missing","value":"secret"}]})";
 if(!parseApiAssertions(spec,tests,error) || tests.size()!=4) return 1;
 ApiResponse response{200,"Content-Type: application/json\r\n",R"({"a/b":{"~key":["secret"]}})",""};
 auto report=evaluateApiAssertions(response,tests); if(!report.error.empty() || report.results.size()!=4 || !report.results[0].passed || !report.results[1].passed || !report.results[2].passed || report.results[3].passed) return 2;
 for(const auto& r:report.results) if(r.detail.find("secret")!=std::string::npos || r.detail.find("a/b")!=std::string::npos) return 3;
 for(const auto* invalid:{R"({"assertions":[]})", R"({"assertions":[{"type":"status","value":99}]})", R"({"assertions":[{"type":"status","value":200.5}]})", R"({"assertions":[{"type":"status","value":200,"value":201}]})", R"({"assertions":[{"type":"body_contains","value":""}]})", R"({"assertions":[{"type":"json_string_equals","pointer":"/~2","value":"secret"}]})", R"({"assertions":[{"type":"json_string_equals","pointer":"bad","value":"secret"}]})", R"({"assertions":[],"assertions":[]})"}) if(parseApiAssertions(invalid,tests,error) || !tests.empty() || error.find("secret")!=std::string::npos) return 4;
 if(!parseApiAssertions(R"({"assertions":[{"type":"json_string_equals","pointer":"/x","value":"secret"}]})",tests,error)) return 5;
 response.body=R"({"x":"secret","x":"secret"})"; report=evaluateApiAssertions(response,tests); if(!report.error.empty() || report.results[0].passed) return 6;
 response.body=std::string(1024U*1024U+1,' '); report=evaluateApiAssertions(response,tests); if(!report.error.empty() || report.results[0].passed) return 7;
 response.body="{}"; response.error="secret"; report=evaluateApiAssertions(response,tests); if(report.error.empty() || !report.results.empty() || report.error.find("secret")!=std::string::npos) return 8;
 response.error.clear(); std::atomic_bool cancelled{true}; if(evaluateApiAssertions(response,tests,&cancelled).error.empty()) return 9;
 tests[0].pointer="/~"; if(evaluateApiAssertions(response,tests).error.empty()) return 10;
 if(!parseApiAssertions(R"({"assertions":[{"type":"json_bool_equals","pointer":"/ok","value":true},{"type":"json_bool_equals","pointer":"/no","value":false},{"type":"json_null","pointer":"/empty","value":null},{"type":"json_null","pointer":"/missing","value":null}]})",tests,error)) return 11;
 response.body=R"({"ok":true,"no":false,"empty":null})"; report=evaluateApiAssertions(response,tests);
 if(!report.error.empty() || report.results.size()!=4 || !report.results[0].passed || !report.results[1].passed || !report.results[2].passed || report.results[3].passed) return 12;
 response.body=R"({"ok":1,"no":null,"empty":false})"; report=evaluateApiAssertions(response,tests);
 if(report.results[0].passed || report.results[1].passed || report.results[2].passed) return 13;
 for(const auto* invalid:{R"({"assertions":[{"type":"json_bool_equals","pointer":"/x","value":"true"}]})",R"({"assertions":[{"type":"json_null","pointer":"/x","value":false}]})"}) if(parseApiAssertions(invalid,tests,error)) return 14;
 if(!parseApiAssertions(R"({"assertions":[{"type":"header_equals","name":"Content-Type","value":"application/json"}]})",tests,error)) return 15;
 response.headers="HTTP/1.1 100 Continue\r\nContent-Type: wrong\r\n\r\nHTTP/2 200\r\ncontent-type: \tapplication/json \t\r\n\r\n";
 report=evaluateApiAssertions(response,tests); if(!report.error.empty() || !report.results[0].passed) return 16;
 response.headers="HTTP/2 200\r\nContent-Type: application/json\r\ncontent-type: application/json\r\n\r\n";
 if(evaluateApiAssertions(response,tests).results[0].passed) return 17;
 response.headers="HTTP/2 201\r\nContent-Type: application/json\r\n\r\n";
 if(evaluateApiAssertions(response,tests).results[0].passed) return 18;
 response.headers="HTTP/2 200\r\nContent-Type: application/json\r\n";
 if(evaluateApiAssertions(response,tests).results[0].passed) return 19;
 for(const auto* invalid:{R"({"assertions":[{"type":"header_equals","name":"bad name","value":"x"}]})",R"({"assertions":[{"type":"header_equals","name":"x","value":"bad\nvalue"}]})",R"({"assertions":[{"type":"status","name":"x","value":200}]})"}) if(parseApiAssertions(invalid,tests,error)) return 20;
 if(!parseApiAssertions(R"({"assertions":[{"type":"json_number_equals","pointer":"/a","value":9007199254740993},{"type":"json_number_equals","pointer":"/b","value":1.2300e2},{"type":"json_number_equals","pointer":"/c","value":-0.00}]})",tests,error)) return 21;
 response.body=R"({"a":9007199254740992,"b":123,"c":0})"; report=evaluateApiAssertions(response,tests);
 if(!report.error.empty() || report.results[0].passed || !report.results[1].passed || !report.results[2].passed) return 22;
 response.body=R"({"a":9007199254740993.0,"b":123.00001,"c":"0"})"; report=evaluateApiAssertions(response,tests);
 if(!report.results[0].passed || report.results[1].passed || report.results[2].passed) return 23;
 if(parseApiAssertions(R"({"assertions":[{"type":"json_number_equals","pointer":"/x","value":1e1000001}]})",tests,error)) return 24;
 return 0;
}
