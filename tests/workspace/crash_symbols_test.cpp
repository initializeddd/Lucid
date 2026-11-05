#include "modules/CrashSymbols.hpp"
#include <limits>
int main() {
    using namespace lucid::modules;
    auto map=parseCrashSourceMap("# map\napp.exe\t0x10\t0x20\tsrc/main.cpp\t7\tmain\n");
    if (!map.error.empty() || map.ranges.size()!=1) return 1;
    std::vector<CrashModule> modules{{"app.exe",0x1000,0x100}};
    auto loc=locateCrashAddress(0x1015,modules,map); if (loc.module!="app.exe" || loc.moduleOffset!=0x15 || loc.path!="src/main.cpp" || loc.line!=7 || loc.symbol!="main") return 2;
    if (!locateCrashAddress(0x1020,modules,map).path.empty() || !locateCrashAddress(0x1100,modules,map).module.empty()) return 3;
    modules.push_back({"other",0x1010,32}); loc=locateCrashAddress(0x1015,modules,map); if (!loc.moduleAmbiguous || !loc.path.empty()) return 4;
    modules.pop_back(); auto duplicate=parseCrashSourceMap("app.exe\t10\t20\ta.cpp\t1\ta\napp.exe\t11\t21\tb.cpp\t2\tb\n"); loc=locateCrashAddress(0x1015,modules,duplicate); if (!loc.sourceAmbiguous || !loc.path.empty()) return 5;
    if (parseCrashSourceMap("app.exe\t10\t20\t../x\t1\tx\n").error.empty() || parseCrashSourceMap("x\t20\t10\tx\t1\tx\n").error.empty()) return 6;
    map=parseCrashSourceMap("app.exe\t10\t20\ta.cpp\t1\ta\napp.exe\t20\t30\tb.cpp\t2\tb\n",1000,1); if (!map.incomplete || !locateCrashAddress(0x1015,modules,map).path.empty()) return 7;
    return 0;
}
