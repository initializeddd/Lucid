#include "modules/SpirvInspection.hpp"
#include <map>
#include <set>
#include <utility>
namespace lucid::modules {
namespace {
constexpr std::size_t maxBytes=16U*1024U*1024U, maxInstructions=1000000, maxEntries=256;
std::uint32_t word(std::string_view s, std::size_t i, bool swap) {
    const auto* p=reinterpret_cast<const unsigned char*>(s.data()+i*4);
    if (swap) return (std::uint32_t(p[0])<<24)|(std::uint32_t(p[1])<<16)|(std::uint32_t(p[2])<<8)|p[3];
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
}
bool inspectSpirv(std::string_view bytes, SpirvInspection& output, std::string& error, const std::atomic_bool* cancel) {
    output={}; error.clear();
    const auto fail=[&](const char* message) { error=message; return false; };
    if (cancel && cancel->load()) return fail("SPIR-V inspection cancelled");
    if (bytes.size()<20 || bytes.size()>maxBytes || bytes.size()%4) return fail("Invalid SPIR-V size");
    const auto magic=word(bytes,0,false); const bool swap=magic==0x03022307;
    if (magic!=0x07230203 && !swap) return fail("Invalid SPIR-V magic");
    SpirvInspection result; result.version=word(bytes,1,swap); result.generator=word(bytes,2,swap); result.idBound=word(bytes,3,swap);
    if ((result.version & 0xff0000ffU)!=0 || (result.version>>16)!=1 || ((result.version>>8)&255)>6 || !result.idBound || result.idBound>4194304 || word(bytes,4,swap)!=0) return fail("Unsupported or invalid SPIR-V header");
    const auto words=bytes.size()/4; std::size_t pos=5,instructions=0;
    std::map<std::uint32_t,SpirvWorkgroupSize> sizes; std::set<std::uint32_t> ids;
    while (pos<words) {
        if (cancel && cancel->load()) return fail("SPIR-V inspection cancelled");
        if (++instructions>maxInstructions) return fail("SPIR-V instruction limit exceeded");
        const auto header=word(bytes,pos,swap), length=header>>16, opcode=header&0xffffU;
        if (!length || length>words-pos) return fail("Malformed SPIR-V instruction");
        if (opcode==15) { // OpEntryPoint: model, id, null-terminated name, interface IDs.
            if (length<4 || result.entryPoints.size()>=maxEntries) return fail("Invalid or excessive SPIR-V entry points");
            SpirvEntryPoint entry; entry.executionModel=word(bytes,pos+1,swap); entry.id=word(bytes,pos+2,swap);
            if (!entry.id || entry.id>=result.idBound || !ids.insert(entry.id).second) return fail("Invalid or duplicate SPIR-V entry point ID");
            bool ended=false; std::size_t interfaceStart=pos+length;
            for (std::size_t i=pos+3;i<pos+length && !ended;++i) {
                const auto w=word(bytes,i,swap);
                for (unsigned j=0;j<4;++j) { const auto c=static_cast<unsigned char>((w>>(j*8))&255); if (!c) { if ((w>>(j*8))!=0) return fail("Invalid SPIR-V string padding"); ended=true; interfaceStart=i+1; break; } if (c<32 || c==127 || entry.name.size()>=4096) return fail("Invalid SPIR-V entry point name"); entry.name.push_back(static_cast<char>(c)); }
            }
            if (!ended || entry.name.empty()) return fail("Invalid SPIR-V entry point name");
            for (std::size_t i=interfaceStart;i<pos+length;++i) { const auto id=word(bytes,i,swap); if (!id || id>=result.idBound) return fail("Invalid SPIR-V interface ID"); }
            if (entry.executionModel>5) result.incomplete=true;
            result.entryPoints.push_back(std::move(entry));
        } else if (opcode==16) { // OpExecutionMode; LocalSize is mode 17.
            if (length<3) return fail("Malformed SPIR-V execution mode");
            const auto id=word(bytes,pos+1,swap), mode=word(bytes,pos+2,swap);
            if (!id || id>=result.idBound) return fail("Invalid SPIR-V execution mode ID");
            if (mode==17) { if (length!=6 || sizes.size()>=1024) return fail("Malformed or excessive SPIR-V local sizes"); SpirvWorkgroupSize size{word(bytes,pos+3,swap),word(bytes,pos+4,swap),word(bytes,pos+5,swap)}; if (!size.x || !size.y || !size.z || !sizes.emplace(id,size).second) return fail("Invalid SPIR-V local size"); }
            else result.incomplete=true;
        } else if (opcode==331) { // OpExecutionModeId can use specialization constants.
            if (length<3 || !word(bytes,pos+1,swap) || word(bytes,pos+1,swap)>=result.idBound) return fail("Malformed SPIR-V execution mode ID");
            result.incomplete=true;
        }
        pos+=length;
    }
    for (const auto& [id, size] : sizes) { (void)size; if (!ids.contains(id)) result.incomplete=true; }
    for (auto& entry:result.entryPoints) if (const auto it=sizes.find(entry.id); it!=sizes.end()) { entry.localSize=it->second; if (entry.executionModel!=5) result.incomplete=true; }
    if (result.entryPoints.empty()) result.incomplete=true;
    output=std::move(result); return true;
}
}
