#include "modules/AppleIpsSymbols.hpp"
int main(){using namespace lucid::modules;
 AppleIpsReport report;report.images.push_back({"App","uuid",0x1000,0x100});IpsFrame frame{0x20,0,"reported"};auto map=parseCrashSourceMap("App\t10\t30\tsrc/main.cpp\t7\tmain\n");auto loc=locateIpsSource(report,frame,map);if(loc.path!="src/main.cpp"||loc.line!=7||loc.moduleOffset!=0x20)return 1;
 frame.imageOffset=0x30;if(!locateIpsSource(report,frame,map).path.empty())return 2;frame.imageOffset=0x100;if(!locateIpsSource(report,frame,map).path.empty())return 3;
 frame.imageOffset=0x20;report.images.push_back({"App","different",0x2000,0x100});loc=locateIpsSource(report,frame,map);if(!loc.moduleAmbiguous||!loc.path.empty())return 4;report.images.pop_back();
 map.ranges.push_back(map.ranges[0]);loc=locateIpsSource(report,frame,map);if(!loc.sourceAmbiguous||!loc.path.empty())return 5;map.ranges.pop_back();
 report.incomplete=true;if(!locateIpsSource(report,frame,map).path.empty())return 6;report.incomplete=false;map.incomplete=true;if(!locateIpsSource(report,frame,map).path.empty())return 7;map.incomplete=false;frame.imageIndex=5;if(!locateIpsSource(report,frame,map).path.empty())return 8;
 report={}; report.architecture="ARM-64";report.images.push_back({"App","00112233-4455-6677-8899-aabbccddeeff",0x1000,0x100});
 auto symbols=parseBreakpadSymbols("MODULE mac arm64 00112233445566778899AABBCCDDEEFF0 App\nFILE 0 src/main.cpp\nFUNC 10 20 0 main\n10 20 7 0\n");
 if(compareIpsBreakpadIdentity(report,symbols)!=BreakpadIdentity::Match)return 9;
 symbols.debugId="00112233445566778899aabbccddeeff0";if(compareIpsBreakpadIdentity(report,symbols)!=BreakpadIdentity::Match)return 17;
 frame={0x20,0,""};loc=locateIpsSource(report,frame,symbols.sourceMap);if(loc.path!="src/main.cpp"||loc.line!=7)return 10;
 report.images[0].uuid="00112233-4455-6677-8899-aabbccddee00";if(compareIpsBreakpadIdentity(report,symbols)!=BreakpadIdentity::Mismatch)return 11;
 report.images[0].uuid="invalid";if(compareIpsBreakpadIdentity(report,symbols)!=BreakpadIdentity::Unavailable)return 12;
 report.images[0].uuid="00112233-4455-6677-8899-aabbccddeeff";report.architecture="X86-64";if(compareIpsBreakpadIdentity(report,symbols)!=BreakpadIdentity::Mismatch)return 13;
 report.architecture="ARM-64";report.images.push_back(report.images[0]);if(compareIpsBreakpadIdentity(report,symbols)!=BreakpadIdentity::Unavailable)return 14;
 report.images.pop_back();report.incomplete=true;if(compareIpsBreakpadIdentity(report,symbols)!=BreakpadIdentity::Unavailable)return 15;
 report.incomplete=false;symbols.operatingSystem="Linux";if(compareIpsBreakpadIdentity(report,symbols)!=BreakpadIdentity::Mismatch)return 16;
 return 0;}
