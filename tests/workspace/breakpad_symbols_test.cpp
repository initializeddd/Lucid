#include "modules/BreakpadSymbols.hpp"
int main() {
    using namespace lucid::modules;
    const std::string text="MODULE windows x86_64 ABC123 app.exe\nFILE 0 src/main.cpp\nFUNC 100 20 0 main function\n100 10 4 0\n110 10 5 0\nPUBLIC 200 0 other\n";
    auto s=parseBreakpadSymbols(text); if (!s.error.empty() || s.debugId!="ABC123" || s.architecture!="x86_64" || s.sourceMap.ranges.size()!=2) return 1;
    auto l=locateCrashAddress(0x1115,{{"app.exe",0x1000,0x1000}},s.sourceMap); if (l.path!="src/main.cpp" || l.line!=5 || l.symbol!="main function") return 2;
    if (parseBreakpadSymbols("MODULE x x id a\nFILE 0 ../x\n").error.empty()) return 3;
    if (parseBreakpadSymbols("MODULE x x id a\nFILE 0 a\nFUNC 10 10 0 f\n1f 2 1 0\n").error.empty()) return 4;
    if (parseBreakpadSymbols("MODULE x x id a\nFUNC 10 10 0 f\n10 1 1 2\n").error.empty()) return 5;
    s=parseBreakpadSymbols(text,1000,1); if (!s.incomplete || !s.sourceMap.incomplete || !locateCrashAddress(0x1101,{{"app.exe",0x1000,0x1000}},s.sourceMap).path.empty()) return 6;
    auto symbols=parseBreakpadSymbols(text); CrashModule module{"C:\\bin\\app.exe",0x1000,0x1000,"ABC123"};
    if (compareBreakpadIdentity(symbols,module,"x86-64")!=BreakpadIdentity::Match) return 7;
    module.debugId="OTHER"; if (compareBreakpadIdentity(symbols,module,"x86-64")!=BreakpadIdentity::Mismatch) return 8;
    module.debugId.clear(); if (compareBreakpadIdentity(symbols,module,"x86-64")!=BreakpadIdentity::Unavailable) return 9;
    module.debugId="ABC123"; if (compareBreakpadIdentity(symbols,module,"AArch64")!=BreakpadIdentity::Mismatch || compareBreakpadIdentity(symbols,module,"unknown")!=BreakpadIdentity::Unavailable) return 10;
    return 0;
}
