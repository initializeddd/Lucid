#include "modules/AppleIpsSymbols.hpp"
int main(){using namespace lucid::modules;
 AppleIpsReport report;report.images.push_back({"App","uuid",0x1000,0x100});IpsFrame frame{0x20,0,"reported"};auto map=parseCrashSourceMap("App\t10\t30\tsrc/main.cpp\t7\tmain\n");auto loc=locateIpsSource(report,frame,map);if(loc.path!="src/main.cpp"||loc.line!=7||loc.moduleOffset!=0x20)return 1;
 frame.imageOffset=0x30;if(!locateIpsSource(report,frame,map).path.empty())return 2;frame.imageOffset=0x100;if(!locateIpsSource(report,frame,map).path.empty())return 3;
 frame.imageOffset=0x20;report.images.push_back({"App","different",0x2000,0x100});loc=locateIpsSource(report,frame,map);if(!loc.moduleAmbiguous||!loc.path.empty())return 4;report.images.pop_back();
 map.ranges.push_back(map.ranges[0]);loc=locateIpsSource(report,frame,map);if(!loc.sourceAmbiguous||!loc.path.empty())return 5;map.ranges.pop_back();
 report.incomplete=true;if(!locateIpsSource(report,frame,map).path.empty())return 6;report.incomplete=false;map.incomplete=true;if(!locateIpsSource(report,frame,map).path.empty())return 7;map.incomplete=false;frame.imageIndex=5;if(!locateIpsSource(report,frame,map).path.empty())return 8;
 return 0;}
