#include "modules/ShaderPreviewInterface.hpp"
#include "modules/SpirvInspection.hpp"
#include <cstdint>
#include <map>
#include <set>
namespace lucid::modules {
bool validatePreviewFragmentInterface(std::string_view bytes, std::string& error) {
    SpirvInspection metadata; if (!inspectSpirv(bytes,metadata,error)) return false;
    auto fail=[&] { error="Shader interface is not supported by the fullscreen preview"; return false; };
    if (metadata.entryPoints.size()!=1 || metadata.entryPoints[0].executionModel!=4 || metadata.entryPoints[0].name!="main" || metadata.idBound>65536) return fail();
    const bool swap=static_cast<unsigned char>(bytes[0])==7;
    auto word=[&](std::size_t i) { const auto* p=reinterpret_cast<const unsigned char*>(bytes.data()+i*4); return swap?(std::uint32_t(p[0])<<24)|(std::uint32_t(p[1])<<16)|(std::uint32_t(p[2])<<8)|p[3]:std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24); };
    struct Type { std::uint32_t kind=0, a=0, b=0; }; struct Variable { std::uint32_t type=0, storage=0; };
    std::map<std::uint32_t,Type> types; std::map<std::uint32_t,Variable> variables; std::map<std::uint32_t,std::uint32_t> locations; std::set<std::uint32_t> interfaceIds;
    bool memoryModel=false, origin=false, capability=false; std::size_t pos=5, words=bytes.size()/4;
    while (pos<words) {
        const auto header=word(pos), count=header>>16, op=header&65535U; if (!count || count>words-pos) return fail();
        auto validId=[&](std::uint32_t id) { return id && id<metadata.idBound; };
        if (op==17) { if (count!=2 || word(pos+1)!=1 || capability) return fail(); capability=true; }
        else if (op==10) return fail(); // No SPIR-V extensions.
        else if (op==14) { if (count!=3 || word(pos+1)!=0 || word(pos+2)!=1 || memoryModel) return fail(); memoryModel=true; }
        else if (op==15) {
            bool end=false; std::size_t i=pos+3; for (;i<pos+count && !end;++i) { auto value=word(i); for (unsigned j=0;j<4;++j) if (((value>>(j*8))&255)==0) { end=true; break; } }
            if (!end) return fail();
            for (;i<pos+count;++i) if (!validId(word(i)) || !interfaceIds.insert(word(i)).second) return fail();
        } else if (op==16) { if (count!=3 || word(pos+1)!=metadata.entryPoints[0].id || word(pos+2)!=7 || origin) return fail(); origin=true; }
        else if (op==331) return fail();
        else if (op==22) { if (count!=3 || !validId(word(pos+1)) || !types.emplace(word(pos+1),Type{22,word(pos+2),0}).second) return fail(); }
        else if (op==23) { if (count!=4 || !validId(word(pos+1)) || !validId(word(pos+2)) || !types.emplace(word(pos+1),Type{23,word(pos+2),word(pos+3)}).second) return fail(); }
        else if (op==32) { if (count!=4 || !validId(word(pos+1)) || !validId(word(pos+3)) || !types.emplace(word(pos+1),Type{32,word(pos+2),word(pos+3)}).second) return fail(); }
        else if (op==59) { if (count<4 || count>5 || !validId(word(pos+1)) || !validId(word(pos+2))) return fail(); const auto storage=word(pos+3); if (storage!=1 && storage!=3 && storage!=6 && storage!=7) return fail(); if (!variables.emplace(word(pos+2),Variable{word(pos+1),storage}).second) return fail(); }
        else if (op==71) { if (count<3 || !validId(word(pos+1))) return fail(); const auto decoration=word(pos+2); if (decoration==11 || decoration==33 || decoration==34) return fail(); if (decoration==30 && (count!=4 || !locations.emplace(word(pos+1),word(pos+3)).second)) return fail(); }
        else if (op==72 || op==73 || op==74 || op==75) return fail(); // Reject member/group decorations; do not guess layout.
        pos+=count;
    }
    if (!capability || !memoryModel || !origin) return fail();
    unsigned inputs=0,outputs=0;
    for (const auto& [id,var]:variables) if (var.storage==1 || var.storage==3) {
        if (!interfaceIds.contains(id) || !locations.contains(id) || locations[id]!=0) return fail();
        const auto pointer=types.find(var.type); if (pointer==types.end() || pointer->second.kind!=32 || pointer->second.a!=var.storage) return fail();
        const auto vec=types.find(pointer->second.b); if (vec==types.end() || vec->second.kind!=23 || vec->second.b!=(var.storage==1?2U:4U)) return fail();
        const auto scalar=types.find(vec->second.a); if (scalar==types.end() || scalar->second.kind!=22 || scalar->second.a!=32) return fail();
        if (var.storage==1) ++inputs; else ++outputs;
    }
    for (auto id:interfaceIds) if (!variables.contains(id)) return fail();
    if (inputs>1 || outputs!=1) return fail();
    error.clear(); return true;
}
}
