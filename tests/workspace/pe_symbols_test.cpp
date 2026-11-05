#include "modules/BinaryInspector.hpp"
#include <cstdint>
#include <string>
using namespace lucid::modules;
static std::string fixture(bool wide) {
    std::string p(0x900, '\0');
    auto put = [&](std::size_t at, std::uint64_t n, unsigned w) { for (unsigned i=0;i<w;++i) p[at+i]=static_cast<char>(n>>(8*i)); };
    auto r = [](std::size_t v) { return 0x200 + v - 0x1000; };
    p.replace(0,2,"MZ"); put(60,0x40,4); p.replace(0x40,4,std::string("PE\0\0",4)); put(0x44,wide?0x8664:0x14c,2); put(0x46,1,2); put(0x54,wide?240:224,2);
    const auto opt=0x58, dirs=opt+(wide?112:96), section=opt+(wide?240:224);
    put(opt,wide?0x20b:0x10b,2); put(opt+(wide?108:92),16,4); put(dirs,0x1100,4); put(dirs+4,0x80,4); put(dirs+8,0x1300,4); put(dirs+12,40,4);
    p.replace(section,5,".data"); put(section+8,0x600,4); put(section+12,0x1000,4); put(section+16,0x600,4); put(section+20,0x200,4);
    const auto e=r(0x1100); put(e+16,10,4); put(e+20,2,4); put(e+24,1,4); put(e+28,0x1200,4); put(e+32,0x1210,4); put(e+36,0x1220,4);
    put(r(0x1200),0x1400,4); put(r(0x1204),0x1160,4); put(r(0x1210),0x1230,4); put(r(0x1220),0,2); p.replace(r(0x1230),7,std::string("export\0",7)); p.replace(r(0x1160),11,std::string("other.Func\0",11));
    const auto d=r(0x1300); put(d,0x1340,4); put(d+12,0x1380,4); put(d+16,0x1340,4); p.replace(r(0x1380),7,std::string("foo.dll",7));
    put(r(0x1340),0x13a0,wide?8:4); put(r(0x1340)+(wide?8:4),(wide?(std::uint64_t(1)<<63):(std::uint64_t(1)<<31))|7,wide?8:4); p.replace(r(0x13a2),7,std::string("import\0",7));
    return p;
}
int main(){
    for (bool wide : {false,true}) {
        auto p=fixture(wide); auto x=inspectBinary(p);
        if (!x.error.empty() || x.incomplete || x.peImports.size()!=2 || x.peExports.size()!=2 || x.libraries.size()!=1 ||
            x.peImports[0].name!="import" || x.peImports[0].library!="foo.dll" || !x.peImports[1].byOrdinal || x.peImports[1].ordinal!=7 ||
            x.peExports[0].name!="export" || x.peExports[0].ordinal!=10 || x.peExports[0].rva!=0x1400 ||
            !x.peExports[1].byOrdinal || x.peExports[1].ordinal!=11 || x.peExports[1].forwarder!="other.Func") return 1;
        auto limited=inspectBinary(p,1); if (!limited.incomplete || limited.peImports.size()!=1 || limited.peExports.size()!=1) return 2;
        p[0x200+0x220]=2; if (inspectBinary(p).error.empty()) return 3;
        p=fixture(wide); p[0x200+0x340]=static_cast<char>(0xff); p[0x200+0x341]=static_cast<char>(0xff); p[0x200+0x342]=0x7f; if (inspectBinary(p).error.empty()) return 4;
        // A forwarder must terminate inside the export directory, not merely
        // somewhere in its backing section.
        p=fixture(wide); p.replace(0x200+0x160, 32, std::string(32, 'x'));
        if (inspectBinary(p).error.empty()) return 5;
        // A complete import directory must contain a terminating descriptor.
        p=fixture(wide); p[0x200+0x314]=1;
        if (inspectBinary(p).error.empty()) return 6;
    }
    return 0;
}
