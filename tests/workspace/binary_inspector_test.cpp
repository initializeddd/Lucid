#include "modules/BinaryInspector.hpp"
#include <algorithm>
#include <string>

int main() {
    using lucid::modules::inspectBinary;
    std::string elf(64 + 4 * 64 + 32 + 24 + 16, '\0');
    auto put = [&](std::size_t p, unsigned long long n, unsigned width) { for (unsigned i = 0; i < width; ++i) elf[p + i] = static_cast<char>(n >> (8 * i)); };
    elf.replace(0, 4, "\177ELF", 4); elf[4] = 2; elf[5] = 1; elf[6] = 1;
    put(18, 62, 2); put(40, 64, 8); put(58, 64, 2); put(60, 4, 2); put(62, 1, 2);
    const std::size_t strings = 64 + 4 * 64, symbols = strings + 32, dynamic = symbols + 24;
    elf.replace(strings, 25, std::string("\0.shstr\0.sym\0foo\0libx.so\0", 25));
    auto section = [&](int i, unsigned name, unsigned type, std::size_t off, std::size_t size, unsigned link, unsigned entry) {
        const auto p = 64 + i * 64; put(p, name, 4); put(p + 4, type, 4); put(p + 24, off, 8); put(p + 32, size, 8); put(p + 40, link, 4); put(p + 56, entry, 8);
    };
    section(1, 1, 3, strings, 32, 0, 0); section(2, 8, 2, symbols, 24, 1, 24); section(3, 0, 6, dynamic, 16, 1, 16);
    put(symbols, 13, 4); put(dynamic, 1, 8); put(dynamic + 8, 17, 8);
    auto result = inspectBinary(elf);
    if (!result.error.empty() || result.architecture != "x86-64" || result.sections.size() != 3 || result.symbols != std::vector<std::string>{"foo"} || result.libraries != std::vector<std::string>{"libx.so"}) return 1;
    // Extended section count and string-table index reside in section zero.
    put(60, 0, 2); put(62, 0xffff, 2); put(64+32, 4, 8); put(64+40, 1, 4);
    result = inspectBinary(elf); if (!result.error.empty() || result.sections.size()!=3 || result.symbols != std::vector<std::string>{"foo"}) return 12;
    put(64+32, 1000001, 8); if (inspectBinary(elf).error.empty()) return 13;
    put(64+32, 4, 8); put(64+40, 4, 4); if (inspectBinary(elf).error.empty()) return 14;
    put(60, 4, 2); put(62, 1, 2); put(64+32, 0, 8); put(64+40, 0, 4);
    result = inspectBinary(elf, 0); if (!result.incomplete || !result.sections.empty()) return 2;
    // A long, unterminated string table must not cause unbounded scans for
    // every symbol. The maximum supported name remains 4096 bytes.
    const auto longTable = elf.size();
    elf.append(std::string(1, '\0') + std::string(4096, 'x') + std::string(2, '\0'));
    section(1, 1, 3, longTable, 4099, 0, 0); put(symbols, 1, 4);
    result = inspectBinary(elf);
    if (!result.error.empty() || result.symbols.size() != 1 || result.symbols[0].size() != 4096) return 9;
    elf[longTable + 4097] = 'x';
    result = inspectBinary(elf);
    if (!result.error.empty() || !result.symbols.empty()) return 10;
    put(64 + 2 * 64 + 24, elf.size() + 1, 8); if (inspectBinary(elf).error.empty()) return 3;
    if (inspectBinary("hello").error.empty()) return 4;
    // ELF32 big-endian extended numbering uses different field widths and offsets.
    std::string be(52 + 2 * 40 + 8, '\0');
    auto bp = [&](std::size_t p, unsigned long long value, unsigned width) { for (unsigned i=0; i<width; ++i) be[p+i]=static_cast<char>(value >> (8*(width-i-1))); };
    be.replace(0,4,"\177ELF",4); be[4]=1; be[5]=2; be[6]=1; bp(18,40,2); bp(32,52,4); bp(46,40,2); bp(48,0,2); bp(50,0xffff,2);
    bp(52+20,2,4); bp(52+24,1,4); bp(92,1,4); bp(96,3,4); bp(108,132,4); bp(112,8,4); be.replace(132,8,std::string("\0.names\0",8));
    result=inspectBinary(be); if (!result.error.empty() || result.architecture!="ARM" || result.sections.size()!=1 || result.sections[0].name!=".names") return 15;
    bp(52+4,1,4); if (inspectBinary(be).error.empty()) return 16;
    // Minimal PE with one section, an import descriptor and a COFF symbol.
    std::string pe(700, '\0');
    auto pp = [&](std::size_t p, unsigned long long n, unsigned w) { for (unsigned i = 0; i < w; ++i) pe[p+i] = static_cast<char>(n >> (i*8)); };
    pe.replace(0, 2, "MZ"); pp(60, 64, 4); pe.replace(64, 4, std::string("PE\0\0", 4)); pp(68, 0x8664, 2); pp(70, 1, 2); pp(76, 650, 4); pp(80, 1, 4); pp(84, 112, 2);
    pp(88, 0x10b, 2); pp(88+92, 2, 4); pp(88+104, 0x1000, 4); pp(88+108, 40, 4);
    const auto ps = 200; pe.replace(ps, 5, ".data"); pp(ps+8, 200, 4); pp(ps+12, 0x1000, 4); pp(ps+16, 200, 4); pp(ps+20, 300, 4);
    pp(312, 0x1040, 4); pe.replace(364, 6, std::string("x.dll\0", 6)); pe.replace(650, 4, "symx"); pp(668, 4, 4);
    result = inspectBinary(pe); if (!result.error.empty() || result.architecture != "x86-64" || result.sections.size() != 1 || result.symbols != std::vector<std::string>{"symx"} || result.libraries != std::vector<std::string>{"x.dll"}) return 5;
    // Overlapping file ranges must not let an RVA read a string past its own
    // shorter section just because another section maps the same bytes.
    pp(70, 2, 2); pp(ps+40+8, 66, 4); pp(ps+40+12, 0x2000, 4); pp(ps+40+16, 66, 4); pp(ps+40+20, 300, 4);
    pp(312, 0x2040, 4);
    if (inspectBinary(pe).error.empty()) return 11;
    pp(70, 1, 2); pp(312, 0x1040, 4);
    pp(ps+20, 9999, 4); if (inspectBinary(pe).error.empty()) return 6;
    // Thin Mach-O with one section, one symbol and one dylib command.
    std::string mach(260, '\0');
    auto mp = [&](std::size_t p, unsigned long long n, unsigned w) { for (unsigned i = 0; i < w; ++i) mach[p+i] = static_cast<char>(n >> (i*8)); };
    mach.replace(0, 4, std::string("\xcf\xfa\xed\xfe", 4)); mp(4, 0x100000c, 4); mp(16, 3, 4); mp(20, 208, 4);
    mp(32, 0x19, 4); mp(36, 152, 4); mp(32+64, 1, 4); mach.replace(104, 6, "__text"); mp(104+40, 1, 8); mp(104+48, 259, 4);
    mp(184, 2, 4); mp(188, 24, 4); mp(192, 240, 4); mp(196, 1, 4); mp(200, 256, 4); mp(204, 4, 4);
    mp(208, 0xc, 4); mp(212, 32, 4); mp(216, 24, 4); mach.replace(232, 8, std::string("lib.dyl\0", 8)); mp(240, 1, 4); mach.replace(256, 4, std::string("\0xx\0", 4));
    result = inspectBinary(mach); if (!result.error.empty() || result.architecture != "AArch64" || result.sections.size() != 1 || result.symbols != std::vector<std::string>{"xx"} || result.libraries != std::vector<std::string>{"lib.dyl"}) return 7;
    mp(36, 9999, 4); if (inspectBinary(mach).error.empty()) return 8;
    // A universal file exposes both slices and uses absolute section offsets.
    mp(36, 152, 4);
    std::string fat(1024, '\0');
    auto fp = [&](std::size_t p, unsigned long long v, unsigned w) { for (unsigned i=0; i<w; ++i) fat[p+i]=static_cast<char>(v >> (8*(w-i-1))); };
    fat.replace(0,4,std::string("\xca\xfe\xba\xbe",4)); fp(4,2,4);
    fp(8,0x100000c,4); fp(16,256,4); fp(20,mach.size(),4); fp(24,0,4);
    fp(28,0x100000c,4); fp(36,600,4); fp(40,mach.size(),4); fp(44,0,4);
    fat.replace(256,mach.size(),mach); fat.replace(600,mach.size(),mach);
    result=inspectBinary(fat); if (!result.error.empty() || result.slices.size()!=2 || result.sections[0].offset!=515 || result.format!="Universal Mach-O 64") return 17;
    result=lucid::modules::inspectBinarySlice(fat,1); if (!result.error.empty() || result.sections[0].offset!=859 || result.slices.size()!=2) return 18;
    if (lucid::modules::inspectBinarySlice(fat,2).error.empty()) return 19;
    fp(36,300,4); if (inspectBinary(fat).error.empty()) return 20;
    fp(36,600,4); fp(40,999999,4); if (inspectBinary(fat).error.empty()) return 21;
    return 0;
}
