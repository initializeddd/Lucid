#include "modules/ApiResponseCompare.hpp"
#include <atomic>
using namespace lucid::modules;
int main() {
    ApiResponse response; response.status=200; response.body="one\ntwo\nthree";
    auto report=compareApiResponseBody("one\ntwo\nthree",response);
    if(!report.error.empty() || !report.identical || report.baselineBytes!=13 || report.currentBytes!=13) return 1;
    report=compareApiResponseBody("one\nold\nthree",response);
    if(!report.error.empty() || report.identical || !report.text || report.commonPrefix!=4 || report.commonSuffix!=6 || report.baselineFirstLine!=2 || report.currentFirstLine!=2 || report.baselineLastLine!=2 || report.currentLastLine!=2) return 2;
    response.body="abcX";report=compareApiResponseBody("abc",response);
    if(report.identical || report.commonPrefix!=3 || report.commonSuffix!=0 || !report.text) return 3;
    response.body=std::string("abc\0x",5); report=compareApiResponseBody("abc",response);
    if(report.identical || report.text || report.commonPrefix!=3) return 4;
    response.error="sensitive";report=compareApiResponseBody("",response);
    if(report.error.empty() || report.error.find("sensitive")!=std::string::npos) return 5;
    response.error.clear(); std::atomic_bool cancelled{true};report=compareApiResponseBody("",response,&cancelled);
    if(report.error.empty() || report.identical) return 6;
    response.body.assign(4U*1024U*1024U+1,'a');report=compareApiResponseBody("",response);
    if(report.error.empty()) return 7;
    response.body="\xc3\xa9";report=compareApiResponseBody("\xc3\xa8",response);
    if(!report.text || report.commonPrefix!=1 || report.identical) return 8;
    return 0;
}
