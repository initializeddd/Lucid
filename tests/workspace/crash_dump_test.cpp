#include "modules/CrashDump.hpp"
#include <string>
int main() {
    using lucid::modules::readCrashDump; std::string d(32+24+52+168, '\0');
    auto put = [&](std::size_t p, unsigned long long v, unsigned n) { for (unsigned i = 0; i<n; ++i) d[p+i] = static_cast<char>(v >> (8*i)); };
    d.replace(0,4,"MDMP"); put(4,0xa793,4); put(8,2,4); put(12,32,4);
    put(32,3,4); put(36,52,4); put(40,56,4); put(44,6,4); put(48,168,4); put(52,108,4);
    put(56,1,4); put(60,42,4); put(84,0x1234,8);
    put(108,42,4); put(116,0xc0000005,4); put(132,0xdeadbeef,8);
    auto r = readCrashDump(d); if (!r.error.empty() || r.threads.size()!=1 || r.threads[0].id!=42 || r.threads[0].stackStart!=0x1234 || !r.exception.present || r.exception.code!=0xc0000005 || r.exception.address!=0xdeadbeef) return 1;
    r = readCrashDump(d,0); if (!r.incomplete || !r.threads.empty() || !r.exception.present) return 2;
    put(92,4,4); put(96,9999,4); if (readCrashDump(d).error.empty()) return 3;
    put(92,0,4); put(96,0,4); put(140,16,4); if (readCrashDump(d).error.empty()) return 4;
    if (readCrashDump("MDMP").error.empty()) return 5;
    std::string m(32+24+112+56+12, '\0');
    auto set = [&](std::size_t p, unsigned long long v, unsigned n) { for (unsigned i=0; i<n; ++i) m[p+i] = static_cast<char>(v >> (8*i)); };
    m.replace(0,4,"MDMP"); set(4,0xa793,4); set(8,2,4); set(12,32,4);
    set(32,4,4); set(36,112,4); set(40,56,4); set(44,7,4); set(48,56,4); set(52,168,4);
    set(56,1,4); set(60,0x1000,8); set(68,123,4); set(80,224,4); set(168,12,2);
    set(224,8,4); set(228,'a',2); set(230,0xd83d,2); set(232,0xde00,2); set(234,'b',2);
    r = readCrashDump(m); if (!r.error.empty() || r.architecture != "AArch64" || r.modules.size()!=1 || r.modules[0].base!=0x1000 || r.modules[0].size!=123 || r.modules[0].name != "a\xf0\x9f\x98\x80" "b") return 6;
    r = readCrashDump(m, 4096, 0); if (!r.incomplete || !r.modules.empty()) return 7;
    set(232, 'x', 2); if (readCrashDump(m).error.empty()) return 8;
    set(80, 9999, 4); if (readCrashDump(m).error.empty()) return 9;
    // AMD64 context and a bounded frame-pointer chain in captured stack bytes.
    std::string x(32 + 24 + 52 + 56 + 256 + 64, '\0');
    auto sx = [&](std::size_t p, unsigned long long v, unsigned n) { for (unsigned i=0; i<n; ++i) x[p+i] = static_cast<char>(v >> (8*i)); };
    x.replace(0,4,"MDMP"); sx(4,0xa793,4); sx(8,2,4); sx(12,32,4);
    sx(32,3,4); sx(36,52,4); sx(40,56,4); sx(44,7,4); sx(48,56,4); sx(52,108,4);
    sx(56,1,4); sx(60,7,4); sx(84,0x1000,8); sx(92,64,4); sx(96,420,4); sx(100,256,4); sx(104,164,4);
    sx(108,9,2); sx(164+48,0x100001,4); sx(164+152,0x1000,8); sx(164+160,0x1010,8); sx(164+248,0x1234,8);
    sx(420+16,0x1020,8); sx(420+24,0x5678,8); sx(420+32,0,8); sx(420+40,0x9abc,8);
    r = readCrashDump(x);
    if (!r.error.empty() || r.threads.size()!=1 || !r.threads[0].framesIncomplete || r.threads[0].frames.size()!=3 ||
        r.threads[0].frames[0].address!=0x1234 || r.threads[0].frames[0].source!=lucid::modules::CrashFrameSource::Context ||
        r.threads[0].frames[1].address!=0x5678 || r.threads[0].frames[1].stackPointer!=0x1020 ||
        r.threads[0].frames[1].source!=lucid::modules::CrashFrameSource::FramePointerHeuristic || r.threads[0].frames[2].address!=0x9abc) return 10;
    sx(420+16,0x1010,8); r = readCrashDump(x); if (!r.error.empty() || r.threads[0].frames.size()!=2) return 11;
    sx(164+48,0x100000,4); r = readCrashDump(x); if (!r.error.empty() || !r.threads[0].frames.empty() || !r.threads[0].framesIncomplete) return 12;
    sx(164+48,0x100001,4); sx(164+160,0xfffffffffffffff8ULL,8); r = readCrashDump(x); if (!r.error.empty() || r.threads[0].frames.size()!=1) return 13;
    sx(100,255,4); r = readCrashDump(x); if (!r.error.empty() || !r.threads[0].frames.empty()) return 14;
    // Prefer the exception record's crashing context over the thread snapshot.
    std::string z(920, '\0');
    auto sz = [&](std::size_t p, unsigned long long v, unsigned n) { for (unsigned i=0; i<n; ++i) z[p+i] = static_cast<char>(v >> (8*i)); };
    z.replace(0,4,"MDMP"); sz(4,0xa793,4); sz(8,3,4); sz(12,32,4);
    sz(32,3,4); sz(36,52,4); sz(40,68,4); sz(44,7,4); sz(48,56,4); sz(52,120,4); sz(56,6,4); sz(60,168,4); sz(64,176,4);
    sz(68,1,4); sz(72,7,4); sz(96,0x1000,8); sz(104,64,4); sz(108,856,4); sz(112,256,4); sz(116,344,4);
    sz(120,9,2); sz(176,7,4); sz(336,256,4); sz(340,600,4);
    sz(344+48,0x100001,4); sz(344+152,0x1000,8); sz(344+248,0x1111,8);
    sz(600+48,0x100001,4); sz(600+152,0x1000,8); sz(600+248,0x2222,8);
    r = readCrashDump(z); if (!r.error.empty() || r.threads.size()!=1 || r.threads[0].frames.size()!=1 || r.threads[0].frames[0].address!=0x2222) return 15;
    // CodeView RSDS GUID fields use little-endian encoding; Breakpad appends hexadecimal age.
    std::string cv(240, '\0'); auto sc=[&](std::size_t p, unsigned long long v, unsigned n) { for (unsigned i=0;i<n;++i) cv[p+i]=static_cast<char>(v>>(8*i)); };
    cv.replace(0,4,"MDMP"); sc(4,0xa793,4); sc(8,1,4); sc(12,32,4); sc(32,4,4); sc(36,112,4); sc(40,44,4); sc(44,1,4); sc(48+20,156,4); sc(156,2,4); sc(160,'x',2);
    sc(48+76,24,4); sc(48+80,180,4); cv.replace(180,4,"RSDS"); for (unsigned i=0;i<16;++i) cv[184+i]=static_cast<char>(i); sc(200,0x12,4);
    r=readCrashDump(cv); if (!r.error.empty() || r.modules.size()!=1 || r.modules[0].debugId!="030201000504070608090A0B0C0D0E0F12") return 16;
    sc(48+76,23,4); if (readCrashDump(cv).error.empty()) return 17;
    sc(48+76,24,4); sc(48+80,999,4); if (readCrashDump(cv).error.empty()) return 18;
    // Windows x86 control context and a two-entry EBP chain.
    std::string i386(32+24+52+56+200+32, '\0');
    auto si=[&](std::size_t p,unsigned long long v,unsigned n) {for(unsigned i=0;i<n;++i)i386[p+i]=static_cast<char>(v>>(8*i));};
    i386.replace(0,4,"MDMP"); si(4,0xa793,4); si(8,2,4); si(12,32,4);
    si(32,3,4); si(36,52,4); si(40,56,4); si(44,7,4); si(48,56,4); si(52,108,4);
    si(56,1,4); si(60,11,4); si(84,0x1000,8); si(92,32,4); si(96,364,4); si(100,200,4); si(104,164,4);
    si(108,0,2); si(164,0x10001,4); si(164+180,0x1008,4); si(164+184,0x12345678,4); si(164+196,0x1000,4);
    si(364+8,0x1010,4); si(364+12,0x11112222,4); si(364+16,0,4); si(364+20,0x33334444,4);
    r=readCrashDump(i386);
    if(!r.error.empty() || r.architecture!="x86" || r.threads.size()!=1 || r.threads[0].frames.size()!=3 ||
       r.threads[0].frames[0].address!=0x12345678 || r.threads[0].frames[1].address!=0x11112222 ||
       r.threads[0].frames[1].stackPointer!=0x1010 || r.threads[0].frames[2].address!=0x33334444 || !r.threads[0].framesIncomplete) return 19;
    si(164,0x10000,4); r=readCrashDump(i386); if(!r.error.empty() || !r.threads[0].frames.empty()) return 20;
    si(164,0x10001,4); si(100,199,4); r=readCrashDump(i386); if(!r.error.empty() || !r.threads[0].frames.empty()) return 21;
    si(100,200,4); si(164+180,0x1009,4); r=readCrashDump(i386); if(!r.error.empty() || r.threads[0].frames.size()!=1) return 22;
    si(164+180,0x101c,4); r=readCrashDump(i386); if(!r.error.empty() || r.threads[0].frames.size()!=1) return 23;
    si(164+180,0x1008,4); si(364+8,0x1008,4); r=readCrashDump(i386); if(!r.error.empty() || r.threads[0].frames.size()!=2) return 24;
    sz(120,0,2); sz(344,0x10001,4); sz(344+196,0x1000,4); sz(344+184,0x1111,4);
    sz(600,0x10001,4); sz(600+196,0x1000,4); sz(600+184,0x2222,4);
    r=readCrashDump(z); if(!r.error.empty() || r.architecture!="x86" || r.threads.size()!=1 || r.threads[0].frames.size()!=1 || r.threads[0].frames[0].address!=0x2222) return 25;
    return 0;
}
