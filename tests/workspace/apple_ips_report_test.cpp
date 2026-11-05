#include "modules/AppleIpsReport.hpp"
int main(){using lucid::modules::parseAppleIpsReport;
 const std::string payload=R"({"cpuType":"ARM-64","exception":{"type":"EXC_BAD_ACCESS","signal":"SIGSEGV"},"usedImages":[{"name":"MyApp","path":"/private/secret","base":4294967297,"size":100,"uuid":"ABC"}],"threads":[{"triggered":true,"frames":[{"imageIndex":0,"imageOffset":3,"symbol":"main"}]}]})";
 auto r=parseAppleIpsReport("{\"bug_type\":\"309\"}\n"+payload);if(!r.error.empty()||r.incomplete||r.architecture!="ARM-64"||r.signal!="SIGSEGV"||r.images.size()!=1||r.images[0].base!=4294967297ULL||r.threads.size()!=1||!r.threads[0].triggered||r.threads[0].frames[0].imageOffset!=3)return 1;
 r=parseAppleIpsReport(payload,4096,1,0);if(!r.error.empty()||!r.incomplete||!r.threads[0].frames.empty())return 2;
 if(parseAppleIpsReport(R"({"threads":[],"threads":[],"usedImages":[]})").error.empty())return 3;
 r=parseAppleIpsReport(R"({"usedImages":[{"base":9007199254740993,"size":100}],"threads":[{"frames":[{"imageIndex":0,"imageOffset":101}]}]})");if(!r.error.empty()||r.images[0].base!=9007199254740993ULL||!r.incomplete||!r.threads[0].frames.empty())return 4;
 if(parseAppleIpsReport(R"({"usedImages":[{"base":1.5,"size":100}],"threads":[]})").error.empty())return 5;
 if(parseAppleIpsReport(payload,10).error.empty())return 6;
 r=parseAppleIpsReport(R"({"usedImages":[{"base":0,"size":5}],"threads":[{"frames":[{"imageIndex":4,"imageOffset":1}]}]})");if(!r.error.empty()||!r.incomplete||!r.threads[0].frames.empty())return 7;
 return 0;}
