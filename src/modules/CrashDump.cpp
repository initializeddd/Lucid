#include "modules/CrashDump.hpp"
#include <algorithm>
#include <limits>
namespace lucid::modules {
namespace {
struct Reader {
    std::string_view bytes;
    bool range(std::uint64_t off, std::uint64_t size) const { return off <= bytes.size() && size <= bytes.size() - static_cast<std::size_t>(off); }
    bool read(std::uint64_t off, unsigned size, std::uint64_t& n) const {
        if (size > 8 || !range(off, size)) return false;
        n = 0;
        for (unsigned i = 0; i < size; ++i) n |= std::uint64_t(static_cast<unsigned char>(bytes[static_cast<std::size_t>(off) + i])) << (8*i);
        return true;
    }
};
}
CrashDump readCrashDump(std::string_view bytes, std::size_t threadLimit, std::size_t moduleLimit) {
    CrashDump out; auto fail = [&](const char* error) { out.error = error; out.threads.clear(); out.modules.clear(); out.architecture.clear(); out.exception = {}; return out; };
    // Bound memory and candidate-frame work independently of caller input.
    threadLimit = std::min<std::size_t>(threadLimit, 4096);
    moduleLimit = std::min<std::size_t>(moduleLimit, 4096);
    Reader r{bytes}; std::uint64_t version = 0, count = 0, directory = 0;
    if (!r.range(0, 32) || bytes.substr(0, 4) != "MDMP" || !r.read(4, 4, version) || (version & 0xffff) != 0xa793 || !r.read(8, 4, count) || !r.read(12, 4, directory)) return fail("Invalid or unsupported minidump header");
    out.format = "Minidump";
    if (count > 65536 || !r.range(directory, count * 12)) return fail("Invalid minidump directory");
    struct ThreadData { std::uint64_t stackRva, contextRva, contextSize; };
    std::vector<ThreadData> threadData;
    std::uint64_t exceptionContextRva = 0, exceptionContextSize = 0;
    bool seenThreads = false, seenException = false, seenModules = false, seenSystem = false;
    for (std::uint64_t i = 0; i < count; ++i) {
        std::uint64_t type = 0, size = 0, off = 0; auto p = directory + i * 12;
        r.read(p,4,type); r.read(p+4,4,size); r.read(p+8,4,off);
        if (!r.range(off, size)) return fail("Minidump stream outside file");
        if (type == 3) {
            if (seenThreads) return fail("Duplicate minidump thread stream");
            seenThreads = true;
            std::uint64_t n = 0; if (size < 4 || !r.read(off,4,n) || n > (size-4)/48 || n > 1000000) return fail("Invalid minidump thread list");
            for (std::uint64_t j = 0; j < n; ++j) {
                auto t = off + 4 + j*48; std::uint64_t id = 0, start = 0, stackSize = 0, stackRva = 0, contextSize = 0, contextRva = 0;
                r.read(t,4,id); r.read(t+24,8,start); r.read(t+32,4,stackSize); r.read(t+36,4,stackRva); r.read(t+40,4,contextSize); r.read(t+44,4,contextRva);
                if (!r.range(stackRva,stackSize) || !r.range(contextRva,contextSize)) return fail("Invalid minidump thread data");
                if (out.threads.size() < threadLimit) {
                    out.threads.push_back({static_cast<std::uint32_t>(id), start, static_cast<std::uint32_t>(stackSize), {}, false});
                    threadData.push_back({stackRva, contextRva, contextSize});
                } else out.incomplete = true;
            }
        } else if (type == 4) {
            if (seenModules) return fail("Duplicate minidump module stream");
            seenModules = true;
            std::uint64_t n = 0; if (size < 4 || !r.read(off,4,n) || n > (size-4)/108 || n > 1000000) return fail("Invalid minidump module list");
            std::uint64_t totalNameBytes = 0;
            for (std::uint64_t j = 0; j < n; ++j) {
                auto m = off + 4 + j*108; std::uint64_t base = 0, imageSize = 0, nameRva = 0, nameBytes = 0;
                r.read(m,8,base); r.read(m+8,4,imageSize); r.read(m+20,4,nameRva);
                if (!r.read(nameRva,4,nameBytes) || nameBytes % 2 || nameBytes > 8192 || !r.range(nameRva + 4, nameBytes)) return fail("Invalid minidump module name");
                if (nameBytes > 16 * 1024 * 1024 - totalNameBytes) return fail("Minidump module names exceed limit");
                totalNameBytes += nameBytes;
                std::string name;
                for (std::uint64_t k = 0; k < nameBytes; k += 2) {
                    std::uint64_t c = 0; r.read(nameRva + 4 + k, 2, c);
                    if (c >= 0xd800 && c <= 0xdbff) { std::uint64_t low = 0; if (k+2 >= nameBytes || !r.read(nameRva+6+k,2,low) || low < 0xdc00 || low > 0xdfff) return fail("Invalid UTF-16 module name"); c = 0x10000 + ((c-0xd800)<<10) + (low-0xdc00); k += 2; }
                    else if (c >= 0xdc00 && c <= 0xdfff) return fail("Invalid UTF-16 module name");
                    if (c == 0) return fail("Invalid UTF-16 module name");
                    if (c < 0x80) name.push_back(static_cast<char>(c));
                    else if (c < 0x800) { name.push_back(static_cast<char>(0xc0 | (c>>6))); name.push_back(static_cast<char>(0x80 | (c&63))); }
                    else if (c < 0x10000) { name.push_back(static_cast<char>(0xe0 | (c>>12))); name.push_back(static_cast<char>(0x80 | ((c>>6)&63))); name.push_back(static_cast<char>(0x80 | (c&63))); }
                    else { name.push_back(static_cast<char>(0xf0 | (c>>18))); name.push_back(static_cast<char>(0x80 | ((c>>12)&63))); name.push_back(static_cast<char>(0x80 | ((c>>6)&63))); name.push_back(static_cast<char>(0x80 | (c&63))); }
                }
                std::uint64_t cvSize=0, cvRva=0; r.read(m+76,4,cvSize); r.read(m+80,4,cvRva);
                if (!r.range(cvRva,cvSize)) return fail("Invalid minidump CodeView record");
                std::string debugId;
                if (cvSize && cvSize < 4) return fail("Invalid minidump CodeView record");
                if (cvSize >= 4 && bytes.substr(static_cast<std::size_t>(cvRva),4)=="RSDS") {
                    if (cvSize < 24) return fail("Truncated minidump RSDS record");
                    constexpr char hex[]="0123456789ABCDEF"; constexpr unsigned order[]={3,2,1,0,5,4,7,6,8,9,10,11,12,13,14,15};
                    for (auto i : order) { auto b=static_cast<unsigned char>(bytes[static_cast<std::size_t>(cvRva+4+i)]); debugId.push_back(hex[b>>4]); debugId.push_back(hex[b&15]); }
                    std::uint64_t age=0; r.read(cvRva+20,4,age); char buffer[8]; unsigned digits=0; do { buffer[digits++]=hex[age&15]; age >>= 4; } while (age); while (digits) debugId.push_back(buffer[--digits]);
                }
                if (out.modules.size() < moduleLimit) out.modules.push_back({std::move(name), base, static_cast<std::uint32_t>(imageSize), std::move(debugId)}); else out.incomplete = true;
            }
        } else if (type == 7) {
            if (seenSystem) return fail("Duplicate minidump system stream");
            seenSystem = true;
            std::uint64_t arch = 0; if (size < 56 || !r.read(off,2,arch)) return fail("Invalid minidump system stream");
            switch (arch) { case 0: out.architecture = "x86"; break; case 5: out.architecture = "ARM"; break; case 6: out.architecture = "IA-64"; break; case 9: out.architecture = "x86-64"; break; case 12: out.architecture = "AArch64"; break; default: out.architecture = "Unknown (" + std::to_string(arch) + ")"; }
        } else if (type == 6) {
            if (seenException) return fail("Duplicate minidump exception stream");
            seenException = true;
            std::uint64_t id = 0, code = 0, flags = 0, address = 0, parameters = 0, ctxSize = 0, ctxRva = 0;
            if (size < 168 || !r.read(off,4,id) || !r.read(off+8,4,code) || !r.read(off+12,4,flags) || !r.read(off+24,8,address) || !r.read(off+32,4,parameters) || parameters > 15 || !r.read(off+160,4,ctxSize) || !r.read(off+164,4,ctxRva) || !r.range(ctxRva,ctxSize)) return fail("Invalid minidump exception stream");
            out.exception = {true, static_cast<std::uint32_t>(id), static_cast<std::uint32_t>(code), static_cast<std::uint32_t>(flags), address};
            exceptionContextRva = ctxRva; exceptionContextSize = ctxSize;
        }
    }
    // AMD64 CONTEXT uses Windows-defined offsets. Do not interpret a different
    // architecture or a truncated context as an AMD64 register set.
    for (std::size_t i = 0; i < out.threads.size(); ++i) {
        auto& thread = out.threads[i]; const auto& data = threadData[i];
        thread.framesIncomplete = true;
        auto contextRva = data.contextRva, contextSize = data.contextSize;
        if (out.exception.present && out.exception.threadId == thread.id && exceptionContextSize) {
            contextRva = exceptionContextRva; contextSize = exceptionContextSize;
        }
        if (out.architecture != "x86-64" || contextSize < 256) continue;
        std::uint64_t flags = 0, pc = 0, sp = 0, bp = 0;
        if (!r.read(contextRva + 48, 4, flags) || (flags & 0x100001U) != 0x100001U ||
            !r.read(contextRva + 152, 8, sp) || !r.read(contextRva + 160, 8, bp) ||
            !r.read(contextRva + 248, 8, pc) || !pc) continue;
        thread.frames.push_back({pc, sp, CrashFrameSource::Context});
        if (thread.stackBytes > std::numeric_limits<std::uint64_t>::max() - thread.stackStart) continue;
        const auto end = thread.stackStart + thread.stackBytes;
        for (std::size_t frame = 1; frame < 64; ++frame) {
            if ((bp & 7U) || bp < sp || bp < thread.stackStart || bp > end || end - bp < 16) break;
            const auto offset = data.stackRva + (bp - thread.stackStart);
            std::uint64_t next = 0, returnAddress = 0;
            if (!r.read(offset, 8, next) || !r.read(offset + 8, 8, returnAddress) || !returnAddress) break;
            thread.frames.push_back({returnAddress, bp + 16, CrashFrameSource::FramePointerHeuristic});
            if (!next || next <= bp) break;
            bp = next;
        }
    }
    return out;
}
}
