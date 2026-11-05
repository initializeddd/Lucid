#include "modules/BinaryInspector.hpp"
#include <algorithm>
#include <limits>

namespace lucid::modules {
namespace {
struct Reader {
    std::string_view data; bool little;
    bool range(std::uint64_t off, std::uint64_t len) const { return off <= data.size() && len <= data.size() - static_cast<std::size_t>(off); }
    bool number(std::uint64_t off, unsigned n, std::uint64_t& value) const {
        if (n > 8 || !range(off, n)) return false;
        value = 0;
        for (unsigned i = 0; i < n; ++i) {
            const auto index = little ? i : n - i - 1;
            value |= std::uint64_t(static_cast<unsigned char>(data[static_cast<std::size_t>(off) + index])) << (8 * i);
        }
        return true;
    }
    std::string string(std::uint64_t base, std::uint64_t size, std::uint64_t off) const {
        if (!range(base, size) || off >= size) return {};
        const auto start = static_cast<std::size_t>(base + off);
        // Bound each lookup: a malicious table can point many entries at the
        // same unterminated string near the start of a large binary.
        const auto available = static_cast<std::size_t>(size - off);
        const auto candidate = data.substr(start, std::min<std::size_t>(available, 4097));
        const auto end = candidate.find('\0');
        if (end == std::string_view::npos || end > 4096) return {};
        return std::string(candidate.substr(0, end));
    }
};
struct Section { std::uint64_t name = 0, type = 0, offset = 0, size = 0, link = 0, entry = 0; };
// Sectionless executables still describe their dynamic dependencies through
// PT_DYNAMIC and a string table mapped by PT_LOAD. No code is executed.
void inspectSectionlessElf(const Reader& r, bool wide, std::size_t limit, BinaryInspection& out) {
    out.incomplete = true; // Section names and symbols cannot be enumerated here.
    std::uint64_t offset=0, size=0, count=0;
    if (!r.number(wide ? 32 : 28, wide ? 8 : 4, offset) || !r.number(wide ? 54 : 42, 2, size) || !r.number(wide ? 56 : 44, 2, count)) { out.error="Truncated ELF program header"; return; }
    if (!count && !offset) return;
    if (count==0xffff) return; // Extended program-header numbering needs section zero.
    if (!offset || !count || size<(wide ? 56u : 32u) || !r.range(offset,0) || count>(r.data.size()-static_cast<std::size_t>(offset))/size) {out.error="Invalid ELF program header table";return;}
    struct Load {std::uint64_t address, offset, size;}; std::vector<Load> loads;
    std::uint64_t dynamicOffset=0,dynamicSize=0; bool dynamicSeen=false;
    for(std::uint64_t i=0;i<count;++i) {
        const auto p=offset+i*size; std::uint64_t type=0, fileOffset=0, address=0, fileSize=0;
        if(!r.number(p,4,type) || !r.number(p+(wide?8:4),wide?8:4,fileOffset) || !r.number(p+(wide?16:8),wide?8:4,address) || !r.number(p+(wide?32:16),wide?8:4,fileSize) || ((type==1 || type==2) && !r.range(fileOffset,fileSize))) {out.error="Invalid ELF program segment";return;}
        if(type==1) loads.push_back({address,fileOffset,fileSize});
        if(type==2) {if(dynamicSeen) {out.error="Ambiguous ELF dynamic segment";return;} dynamicSeen=true;dynamicOffset=fileOffset;dynamicSize=fileSize;}
    }
    if(!dynamicSeen) return;
    const auto entrySize=wide?16u:8u; if(dynamicSize%entrySize) {out.error="Invalid ELF dynamic segment";return;}
    std::uint64_t strAddress=0,strSize=0; bool addressSeen=false,sizeSeen=false,terminated=false; std::vector<std::uint64_t> needed;
    const auto entries=dynamicSize/entrySize; const auto scan=std::min<std::uint64_t>(entries,1000000);
    for(std::uint64_t i=0;i<scan;++i) {
        std::uint64_t tag=0,value=0; const auto p=dynamicOffset+i*entrySize;
        r.number(p,wide?8:4,tag); r.number(p+(wide?8:4),wide?8:4,value);
        if(!tag) {terminated=true;break;}
        if(tag==1) {if(needed.size()<limit) needed.push_back(value);}
        if(tag==5) {if(addressSeen) {out.error="Ambiguous ELF string table";return;} addressSeen=true;strAddress=value;}
        if(tag==10) {if(sizeSeen) {out.error="Ambiguous ELF string table";return;} sizeSeen=true;strSize=value;}
    }
    if(!terminated) {out.error="Unterminated ELF dynamic segment";return;}
    if(needed.empty()) return;
    if(!addressSeen || !sizeSeen || !strSize) {out.error="Missing ELF dynamic string table";return;}
    std::uint64_t strOffset=0; bool mapped=false;
    for(const auto& load:loads) if(strAddress>=load.address && strAddress-load.address<=load.size && strSize<=load.size-(strAddress-load.address)) {
        const auto candidate=load.offset+(strAddress-load.address);
        if(mapped && candidate!=strOffset) {out.error="Ambiguous ELF dynamic string mapping";return;}
        mapped=true;strOffset=candidate;
    }
    if(!mapped) {out.error="Invalid ELF dynamic string mapping";return;}
    for(auto index:needed) {auto name=r.string(strOffset,strSize,index);if(name.empty()) {out.error="Invalid ELF dynamic library name";out.libraries.clear();return;}out.libraries.push_back(std::move(name));}
}
}
namespace {
BinaryInspection inspectOther(std::string_view bytes, std::size_t limit) {
    BinaryInspection out;
    auto fail = [&](const char* msg) { out.error = msg; out.sections.clear(); out.symbols.clear(); out.libraries.clear(); out.peImports.clear(); out.peExports.clear(); return out; };
    auto add = [&](auto& list, auto value) { if (list.size() < limit) list.push_back(std::move(value)); else out.incomplete = true; };
    Reader r{bytes, true}; std::uint64_t v = 0;
    if (bytes.size() >= 2 && bytes.substr(0, 2) == "MZ") {
        std::uint64_t pe = 0, machine = 0, count = 0, symoff = 0, symcount = 0, optsize = 0;
        if (!r.number(60, 4, pe) || !r.range(pe, 24) || bytes.substr(static_cast<std::size_t>(pe), 4) != std::string_view("PE\0\0", 4) ||
            !r.number(pe + 4, 2, machine) || !r.number(pe + 6, 2, count) || !r.number(pe + 12, 4, symoff) || !r.number(pe + 16, 4, symcount) || !r.number(pe + 20, 2, optsize)) return fail("Invalid PE header");
        out.format = "PE";
        switch (machine) { case 0x14c: out.architecture = "x86"; break; case 0x8664: out.architecture = "x86-64"; break; case 0x1c4: out.architecture = "ARM"; break; case 0xaa64: out.architecture = "AArch64"; break; default: out.architecture = "Unknown (" + std::to_string(machine) + ")"; }
        const auto opt = pe + 24, table = opt + optsize;
        if (!r.range(opt, optsize) || count > 1024 || count > (bytes.size() - static_cast<std::size_t>(table)) / 40) return fail("Invalid PE section table");
        struct PeSection { std::uint64_t va, virtualSize, raw, rawSize; }; std::vector<PeSection> sections;
        for (std::uint64_t i = 0; i < count; ++i) {
            auto p = table + 40 * i; std::uint64_t vs = 0, va = 0, rawsize = 0, raw = 0;
            r.number(p + 8, 4, vs); r.number(p + 12, 4, va); r.number(p + 16, 4, rawsize); r.number(p + 20, 4, raw);
            if (rawsize && !r.range(raw, rawsize)) return fail("PE section outside file");
            auto name = std::string(bytes.substr(static_cast<std::size_t>(p), 8)); name.resize(name.find('\0') == std::string::npos ? 8 : name.find('\0'));
            add(out.sections, BinarySection{std::move(name), raw, rawsize}); sections.push_back({va, vs, raw, rawsize});
        }
        auto rva = [&](std::uint64_t address, std::uint64_t length, std::uint64_t& offset) {
            bool found = false;
            for (const auto& s : sections) if (address >= s.va && address - s.va <= s.rawSize && length <= s.rawSize - (address - s.va)) {
                if (found) return false;
                offset = s.raw + address - s.va; found = true;
            }
            return found && r.range(offset, length);
        };
        if (symcount) {
            if (!symoff || symcount > (bytes.size() - std::min<std::uint64_t>(symoff, bytes.size())) / 18 || !r.range(symoff, symcount * 18)) return fail("Invalid PE symbol table");
            auto str = symoff + symcount * 18; std::uint64_t strsize = 0;
            if (!r.number(str, 4, strsize) || strsize < 4 || !r.range(str, strsize)) return fail("Invalid PE string table");
            const auto scanLimit = std::min<std::uint64_t>(symcount, 1000000); if (scanLimit < symcount) out.incomplete = true;
            for (std::uint64_t i = 0; i < scanLimit;) {
                auto p = symoff + i * 18; std::uint64_t zero = 0, off = 0, aux = 0; r.number(p, 4, zero); r.number(p + 4, 4, off); r.number(p + 17, 1, aux);
                if (aux >= symcount - i) return fail("Invalid PE auxiliary symbols");
                std::string name; if (!zero) name = r.string(str, strsize, off); else { name = std::string(bytes.substr(static_cast<std::size_t>(p), 8)); auto end = name.find('\0'); if (end != std::string::npos) name.resize(end); }
                if (!name.empty()) add(out.symbols, std::move(name));
                i += aux + 1;
            }
        }
        std::uint64_t magic = 0, dirs = 0;
        if (optsize >= 2 && r.number(opt, 2, magic) && (magic == 0x10b || magic == 0x20b)) {
            const bool wide = magic == 0x20b;
            const auto dirbase = wide ? 112u : 96u, countoff = wide ? 108u : 92u;
            if (optsize < countoff + 4 || !r.number(opt + countoff, 4, dirs) || dirs > (optsize - dirbase) / 8) return fail("Invalid PE data directories");
            auto mappedString = [&](std::uint64_t address) {
                std::uint64_t offset = 0; if (!rva(address, 1, offset)) return std::string{};
                // Bound the string to the section that mapped this RVA. File
                // ranges can overlap even when virtual ranges do not.
                for (const auto& sec : sections) if (address >= sec.va && address - sec.va < sec.rawSize)
                    return r.string(sec.raw, sec.rawSize, address - sec.va);
                return std::string{};
            };
            std::uint64_t exportRva = 0, exportSize = 0, importRva = 0, importSize = 0;
            if (dirs > 0) { r.number(opt + dirbase, 4, exportRva); r.number(opt + dirbase + 4, 4, exportSize); }
            if (dirs > 1) { r.number(opt + dirbase + 8, 4, importRva); r.number(opt + dirbase + 12, 4, importSize); }
            if (dirs > 13) { std::uint64_t delayRva = 0, delaySize = 0; r.number(opt + dirbase + 104, 4, delayRva); r.number(opt + dirbase + 108, 4, delaySize); if (delayRva || delaySize) out.incomplete = true; }
            if (!!exportRva != !!exportSize || !!importRva != !!importSize) return fail("Invalid PE data directory");
            constexpr std::uint64_t maxEntries = 10000;
            if (exportRva) {
                std::uint64_t off = 0, base = 0, functionCount = 0, nameCount = 0, functions = 0, names = 0, ordinals = 0;
                if (exportSize < 40 || !rva(exportRva, 40, off) || !r.number(off+16,4,base) || !r.number(off+20,4,functionCount) ||
                    !r.number(off+24,4,nameCount) || !r.number(off+28,4,functions) || !r.number(off+32,4,names) || !r.number(off+36,4,ordinals) ||
                    base + functionCount > std::uint64_t(UINT32_MAX) + 1) return fail("Invalid PE export directory");
                std::uint64_t functionsOff = 0, namesOff = 0, ordinalsOff = 0;
                if ((functionCount && !rva(functions, functionCount * 4, functionsOff)) ||
                    (nameCount && (!rva(names, nameCount * 4, namesOff) || !rva(ordinals, nameCount * 2, ordinalsOff)))) return fail("Invalid PE export tables");
                const auto checkedFunctions = std::min(functionCount, maxEntries), checkedNames = std::min(nameCount, maxEntries);
                if (checkedFunctions != functionCount || checkedNames != nameCount) out.incomplete = true;
                std::vector<bool> named(static_cast<std::size_t>(checkedFunctions), false);
                auto appendExport = [&](std::uint64_t index, std::string name, bool byOrdinal) {
                    std::uint64_t address = 0; r.number(functionsOff + index * 4, 4, address);
                    if (!address) return true;
                    BinaryPeSymbol symbol; symbol.name = std::move(name); symbol.ordinal = static_cast<std::uint32_t>(base + index); symbol.rva = static_cast<std::uint32_t>(address); symbol.byOrdinal = byOrdinal;
                    if (address >= exportRva && address - exportRva < exportSize) { symbol.forwarder = mappedString(address); if (symbol.forwarder.empty() || symbol.forwarder.size() >= exportSize - (address - exportRva)) return false; }
                    add(out.peExports, std::move(symbol)); return true;
                };
                for (std::uint64_t i = 0; i < checkedNames; ++i) {
                    std::uint64_t nameRva = 0, index = 0; r.number(namesOff + i*4,4,nameRva); r.number(ordinalsOff + i*2,2,index);
                    if (index >= functionCount) return fail("Invalid PE export ordinal");
                    if (index >= checkedFunctions) { out.incomplete = true; continue; }
                    auto name = mappedString(nameRva); if (name.empty() || !appendExport(index, std::move(name), false)) return fail("Invalid PE export name or forwarder");
                    named[static_cast<std::size_t>(index)] = true;
                }
                for (std::uint64_t i = 0; checkedNames == nameCount && i < checkedFunctions; ++i) if (!named[static_cast<std::size_t>(i)] && !appendExport(i, {}, true)) return fail("Invalid PE export forwarder");
            }
            if (importRva) {
                std::uint64_t off = 0; if (!rva(importRva, importSize, off)) return fail("Invalid PE import table");
                bool terminated = false; std::uint64_t thunkBudget = maxEntries;
                for (std::uint64_t i = 0; i + 20 <= importSize && i / 20 < maxEntries; i += 20) {
                    std::uint64_t nameRva = 0, first = 0, stamp = 0, chain = 0, thunk = 0; r.number(off+i,4,first); r.number(off+i+4,4,stamp); r.number(off+i+8,4,chain); r.number(off+i+12,4,nameRva); r.number(off+i+16,4,thunk);
                    if (!(first | stamp | chain | nameRva | thunk)) { terminated = true; break; }
                    auto library = mappedString(nameRva); if (library.empty()) return fail("Invalid PE import name"); add(out.libraries, library);
                    auto tableRva = first ? first : thunk;
                    if (!tableRva) { out.incomplete = true; continue; }
                    bool thunkTerminated = false;
                    while (thunkBudget) {
                        --thunkBudget; std::uint64_t thunkOff = 0, entry = 0;
                        if (!rva(tableRva, wide ? 8 : 4, thunkOff) || !r.number(thunkOff, wide ? 8 : 4, entry)) return fail("Invalid PE import thunk");
                        tableRva += wide ? 8 : 4;
                        if (!entry) { thunkTerminated = true; break; }
                        const auto ordinalBit = wide ? (std::uint64_t(1) << 63) : (std::uint64_t(1) << 31);
                        BinaryPeSymbol symbol; symbol.library = library;
                        if (entry & ordinalBit) { symbol.byOrdinal = true; symbol.ordinal = static_cast<std::uint32_t>(entry & 0xffff); }
                        else {
                            if (entry > UINT32_MAX - 2) { if (!first) { out.incomplete = true; break; } return fail("Invalid PE import symbol RVA"); }
                            symbol.rva = static_cast<std::uint32_t>(entry); symbol.name = mappedString(entry + 2);
                            if (symbol.name.empty()) { if (!first) { out.incomplete = true; break; } return fail("Invalid PE import symbol name"); }
                        }
                        add(out.peImports, std::move(symbol));
                    }
                    if (!thunkTerminated) out.incomplete = true;
                }
                if (!terminated) { if (importSize / 20 >= maxEntries) out.incomplete = true; else return fail("Unterminated PE import table"); }
            }
        } else out.incomplete = true;
        return out;
    }
    if (bytes.size() < 4) return fail("Unsupported binary format");
    const auto magic = bytes.substr(0, 4); bool wide = false, little = true;
    if (magic == std::string_view("\xcf\xfa\xed\xfe", 4)) wide = true;
    else if (magic == std::string_view("\xce\xfa\xed\xfe", 4)) {}
    else if (magic == std::string_view("\xfe\xed\xfa\xcf", 4)) { wide = true; little = false; }
    else if (magic == std::string_view("\xfe\xed\xfa\xce", 4)) little = false;
    else return fail("Unsupported binary format (supported: ELF, PE, Mach-O)");
    r.little = little; std::uint64_t cpu = 0, ncmds = 0, cmdbytes = 0; auto header = wide ? 32u : 28u;
    if (!r.range(0, header) || !r.number(4, 4, cpu) || !r.number(16, 4, ncmds) || !r.number(20, 4, cmdbytes) || !r.range(header, cmdbytes) || ncmds > cmdbytes / 8) return fail("Invalid Mach-O header");
    out.format = wide ? "Mach-O 64" : "Mach-O 32";
    switch (cpu) { case 7: out.architecture = "x86"; break; case 0x1000007: out.architecture = "x86-64"; break; case 12: out.architecture = "ARM"; break; case 0x100000c: out.architecture = "AArch64"; break; default: out.architecture = "Unknown (" + std::to_string(cpu) + ")"; }
    auto pos = std::uint64_t(header), end = pos + cmdbytes;
    for (std::uint64_t i = 0; i < ncmds; ++i) {
        std::uint64_t cmd = 0, size = 0; if (pos > end || end - pos < 8 || !r.number(pos, 4, cmd) || !r.number(pos + 4, 4, size) || size < 8 || size > end - pos) return fail("Invalid Mach-O load command");
        if (cmd == 1 || cmd == 0x19) {
            bool seg64 = cmd == 0x19; auto fixed = seg64 ? 72u : 56u, stride = seg64 ? 80u : 68u; std::uint64_t nsects = 0;
            if (size < fixed || !r.number(pos + (seg64 ? 64 : 48), 4, nsects) || nsects > (size - fixed) / stride) return fail("Invalid Mach-O segment");
            for (std::uint64_t j = 0; j < nsects; ++j) { auto p = pos + fixed + j * stride; std::uint64_t length = 0, off = 0, flags = 0; r.number(p + (seg64 ? 40 : 36), seg64 ? 8 : 4, length); r.number(p + (seg64 ? 48 : 40), 4, off); r.number(p + (seg64 ? 64 : 56), 4, flags);
                auto type = flags & 0xff; if (type != 1 && type != 0xc && type != 0x12 && !r.range(off, length)) return fail("Mach-O section outside file");
                auto name = std::string(bytes.substr(static_cast<std::size_t>(p), 16)); auto z = name.find('\0'); if (z != std::string::npos) name.resize(z); add(out.sections, BinarySection{std::move(name), off, length}); }
        } else if (cmd == 2) {
            std::uint64_t symoff = 0, count = 0, str = 0, strsize = 0; if (size < 24 || !r.number(pos+8,4,symoff) || !r.number(pos+12,4,count) || !r.number(pos+16,4,str) || !r.number(pos+20,4,strsize) || !r.range(str,strsize) || count > (bytes.size() - std::min<std::uint64_t>(symoff, bytes.size())) / (wide ? 16 : 12) || !r.range(symoff, count * (wide ? 16 : 12))) return fail("Invalid Mach-O symbol table");
            auto scan = std::min<std::uint64_t>(count, 1000000); if (scan < count) out.incomplete = true;
            for (std::uint64_t j = 0; j < scan; ++j) { r.number(symoff + j * (wide ? 16 : 12), 4, v); auto name = r.string(str, strsize, v); if (!name.empty()) add(out.symbols, std::move(name)); }
        } else if (cmd == 0xc || cmd == 0x18 || cmd == 0x80000018 || cmd == 0x8000001f || cmd == 0x80000023 || cmd == 0x20) {
            std::uint64_t nameoff = 0; if (size < 24 || !r.number(pos + 8, 4, nameoff) || nameoff < 24 || nameoff >= size) return fail("Invalid Mach-O library command"); auto name = r.string(pos, size, nameoff); if (name.empty()) return fail("Invalid Mach-O library name"); add(out.libraries, std::move(name));
        }
        pos += size;
    }
    return out;
}
}
BinaryInspection inspectBinarySlice(std::string_view bytes, std::size_t sliceIndex, std::size_t itemLimit) {
    BinaryInspection result;
    auto fail = [&](const char* msg) { result = {}; result.error = msg; return result; };
    if (bytes.size() < 8) return fail("Invalid universal Mach-O header");
    const auto magic = bytes.substr(0, 4); bool little = false, wide = false;
    if (magic == std::string_view("\xca\xfe\xba\xbe", 4)) {}
    else if (magic == std::string_view("\xbe\xba\xfe\xca", 4)) little = true;
    else if (magic == std::string_view("\xca\xfe\xba\xbf", 4)) wide = true;
    else if (magic == std::string_view("\xbf\xba\xfe\xca", 4)) { little = true; wide = true; }
    else return fail("Not a universal Mach-O file");
    Reader r{bytes, little}; std::uint64_t count = 0;
    const std::uint64_t stride = wide ? 32 : 20;
    if (!r.number(4, 4, count) || !count || count > 256 || !r.range(8, count * stride)) return fail("Invalid universal Mach-O table");
    const auto tableEnd = 8 + count * stride;
    for (std::uint64_t i = 0; i < count; ++i) {
        const auto p = 8 + i*stride; std::uint64_t cpu = 0, offset = 0, size = 0, align = 0;
        r.number(p,4,cpu); r.number(p+8,wide ? 8 : 4,offset); r.number(p+(wide ? 16 : 12),wide ? 8 : 4,size); r.number(p+(wide ? 24 : 16),4,align);
        if (align > 31 || offset < tableEnd || (offset & ((std::uint64_t(1)<<align)-1)) || size < 4 || !r.range(offset,size)) return fail("Invalid universal Mach-O slice");
        const auto signature = bytes.substr(static_cast<std::size_t>(offset),4);
        const bool sliceLittle = signature == std::string_view("\xcf\xfa\xed\xfe",4) || signature == std::string_view("\xce\xfa\xed\xfe",4);
        const bool sliceBig = signature == std::string_view("\xfe\xed\xfa\xcf",4) || signature == std::string_view("\xfe\xed\xfa\xce",4);
        std::uint64_t sliceCpu = 0; Reader child{bytes.substr(static_cast<std::size_t>(offset),static_cast<std::size_t>(size)),sliceLittle};
        if ((!sliceLittle && !sliceBig) || !child.number(4,4,sliceCpu) || sliceCpu != cpu) return fail("Invalid universal Mach-O slice header");
        for (const auto& previous : result.slices) if (offset < previous.offset + previous.size && previous.offset < offset + size) return fail("Overlapping universal Mach-O slices");
        std::string arch; switch (cpu) { case 7: arch="x86"; break; case 0x1000007: arch="x86-64"; break; case 12: arch="ARM"; break; case 0x100000c: arch="AArch64"; break; default: arch="Unknown (" + std::to_string(cpu) + ")"; }
        result.slices.push_back({std::move(arch),offset,size});
    }
    if (sliceIndex >= result.slices.size()) return fail("Universal Mach-O slice index out of range");
    const auto selected = result.slices[sliceIndex]; auto slices = std::move(result.slices);
    result = inspectOther(bytes.substr(static_cast<std::size_t>(selected.offset),static_cast<std::size_t>(selected.size)),itemLimit);
    if (!result.error.empty()) return result;
    for (auto& section : result.sections) if (section.offset) section.offset += selected.offset;
    result.format = "Universal " + result.format; result.incomplete = result.incomplete || slices.size() > 1; result.slices = std::move(slices);
    return result;
}
BinaryInspection inspectBinary(std::string_view bytes, std::size_t itemLimit) {
    if (bytes.size() >= 4) {
        const auto m=bytes.substr(0,4);
        if (m==std::string_view("\xca\xfe\xba\xbe",4) || m==std::string_view("\xbe\xba\xfe\xca",4) || m==std::string_view("\xca\xfe\xba\xbf",4) || m==std::string_view("\xbf\xba\xfe\xca",4)) return inspectBinarySlice(bytes,0,itemLimit);
    }
    BinaryInspection result;
    auto fail = [&](const char* message) { result.error = message; result.sections.clear(); result.symbols.clear(); result.libraries.clear(); return result; };
    if (bytes.size() < 4 || bytes.substr(0, 4) != std::string_view("\x7f" "ELF", 4)) return inspectOther(bytes, itemLimit);
    if (bytes.size() < 16) return fail("Truncated ELF header");
    const auto cls = static_cast<unsigned char>(bytes[4]), endian = static_cast<unsigned char>(bytes[5]);
    if ((cls != 1 && cls != 2) || (endian != 1 && endian != 2) || bytes[6] != 1) return fail("Invalid ELF identification");
    Reader r{bytes, endian == 1}; const bool wide = cls == 2;
    std::uint64_t machine = 0, table = 0, entsize = 0, count = 0, names = 0;
    if (!r.number(18, 2, machine) || !r.number(wide ? 40 : 32, wide ? 8 : 4, table) ||
        !r.number(wide ? 58 : 46, 2, entsize) || !r.number(wide ? 60 : 48, 2, count) ||
        !r.number(wide ? 62 : 50, 2, names)) return fail("Truncated ELF header");
    result.format = wide ? "ELF64" : "ELF32";
    switch (machine) { case 3: result.architecture = "x86"; break; case 62: result.architecture = "x86-64"; break; case 40: result.architecture = "ARM"; break; case 183: result.architecture = "AArch64"; break; case 243: result.architecture = "RISC-V"; break; default: result.architecture = "Unknown (" + std::to_string(machine) + ")"; }
    if (table == 0 && count == 0 && names == 0) { inspectSectionlessElf(r,wide,itemLimit,result); return result; }
    const auto minimumSectionSize = wide ? 64u : 40u;
    if (!table || entsize < minimumSectionSize || !r.range(table, entsize)) return fail("Invalid ELF section table");
    // Extended numbering stores the real count and name-table index in section 0.
    // SHT_NULL section 0 is metadata, not a file-backed section.
    if (count == 0 || names == 0xffff) {
        std::uint64_t type = 0;
        if (!r.number(table + 4, 4, type) || type != 0) return fail("Invalid ELF extended section numbering");
        if (count == 0 && (!r.number(table + (wide ? 32 : 20), wide ? 8 : 4, count) || count == 0)) return fail("Invalid ELF extended section count");
        if (names == 0xffff && !r.number(table + (wide ? 40 : 24), 4, names)) return fail("Invalid ELF extended name index");
    }
    if (count > 1000000 || count > (bytes.size() - static_cast<std::size_t>(table)) / entsize || names >= count) return fail("Invalid ELF section table");
    std::vector<Section> sections; sections.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i) {
        const auto p = table + i * entsize; Section s;
        if (!r.number(p, 4, s.name) || !r.number(p + 4, 4, s.type) || !r.number(p + (wide ? 24 : 16), wide ? 8 : 4, s.offset) || !r.number(p + (wide ? 32 : 20), wide ? 8 : 4, s.size) || !r.number(p + (wide ? 40 : 24), 4, s.link) || !r.number(p + (wide ? 56 : 36), wide ? 8 : 4, s.entry)) return fail("Truncated ELF section");
        // SHT_NOBITS occupies no bytes in the file.
        if (s.type != 0 && s.type != 8 && !r.range(s.offset, s.size)) return fail("ELF section outside file");
        sections.push_back(s);
    }
    const auto& strings = sections[static_cast<std::size_t>(names)];
    if (names && strings.type != 3) return fail("Invalid ELF section name table");
    auto add = [&](auto& vec, auto value) { if (vec.size() < itemLimit) vec.push_back(std::move(value)); else result.incomplete = true; };
    for (const auto& s : sections) {
        if (s.type != 0) add(result.sections, BinarySection{names ? r.string(strings.offset, strings.size, s.name) : "", s.offset, s.size});
        if (s.type != 2 && s.type != 11 && s.type != 6) continue;
        const auto minimum = s.type == 6 ? (wide ? 16u : 8u) : (wide ? 24u : 16u);
        if (s.link >= sections.size() || sections[static_cast<std::size_t>(s.link)].type != 3 || s.entry < minimum || s.size % s.entry) return fail("Invalid ELF symbol or dynamic table");
        const auto& str = sections[static_cast<std::size_t>(s.link)];
        const auto entries = s.size / s.entry;
        // Bound work even when most entries are unnamed.
        const auto scan = std::min<std::uint64_t>(entries, 1000000);
        if (scan < entries) result.incomplete = true;
        for (std::uint64_t i = 0; i < scan; ++i) {
            const auto p = s.offset + i * s.entry; std::uint64_t a = 0, b = 0;
            if (s.type == 6) {
                r.number(p, wide ? 8 : 4, a); r.number(p + (wide ? 8 : 4), wide ? 8 : 4, b);
                if (a == 0) break;
                if (a == 1) { auto name = r.string(str.offset, str.size, b); if (!name.empty()) add(result.libraries, std::move(name)); }
            } else {
                r.number(p, 4, a); auto name = r.string(str.offset, str.size, a); if (!name.empty()) add(result.symbols, std::move(name));
            }
        }
    }
    return result;
}
}
