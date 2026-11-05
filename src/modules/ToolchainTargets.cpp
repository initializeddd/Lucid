#include "modules/ToolchainTargets.hpp"
#include <algorithm>
namespace lucid::modules {
namespace {
std::string jsonString(std::string_view value) {
    constexpr char hex[]="0123456789abcdef"; std::string out="\"";
    for (unsigned char c:value) { if(c=='"' || c=='\\') {out.push_back('\\');out.push_back(static_cast<char>(c));} else if(c<32) {out+="\\u00";out.push_back(hex[c>>4]);out.push_back(hex[c&15]);} else out.push_back(static_cast<char>(c)); }
    out.push_back('"'); return out;
}
std::string pathText(const std::filesystem::path& path) { const auto raw=path.generic_u8string(); return {reinterpret_cast<const char*>(raw.data()),raw.size()}; }
std::string serialize(const std::vector<ToolchainProfile>& profiles) {
    std::string result="{\n  \"profiles\": [";
    for(std::size_t i=0;i<profiles.size();++i) {
        const auto& p=profiles[i]; result += i ? ",\n    {" : "\n    {";
        result += "\"name\": " + jsonString(p.name) + ", \"target\": " + jsonString(p.targetTriple);
        auto path=[&](const char* key,const auto& value) {if(!value.empty()) result += ", \"" + std::string(key) + "\": " + jsonString(pathText(value));};
        path("toolchainFile",p.cmakeToolchainFile);path("cCompiler",p.cCompiler);path("cppCompiler",p.cppCompiler);path("sysroot",p.sysroot);result += "}";
    }
    if(!profiles.empty()) result += '\n';
    result += "  ]\n}\n"; return result;
}
}
TargetEditDraft prepareTargetEdit(std::string_view source, TargetEditKind kind, std::string_view existingName, const ToolchainProfile& replacement) {
    TargetEditDraft out; std::vector<ToolchainProfile> profiles;
    if(!parseToolchainProfiles(source,profiles,out.error)) return out;
    if(kind!=TargetEditKind::Add && kind!=TargetEditKind::Replace && kind!=TargetEditKind::Remove) {out.error="Invalid target edit operation";return out;}
    auto found=std::find_if(profiles.begin(),profiles.end(),[&](const auto& p){return p.name==existingName;});
    if (kind!=TargetEditKind::Remove) {
        try {
            if (replacement.name.size()>4096 || replacement.targetTriple.size()>255 || pathText(replacement.cmakeToolchainFile).size()>4096 || pathText(replacement.cCompiler).size()>4096 || pathText(replacement.cppCompiler).size()>4096 || pathText(replacement.sysroot).size()>4096) {out.error="Target field exceeds size limit";return out;}
        } catch(const std::exception&) {out.error="Could not encode target paths";return out;}
    }
    if(kind==TargetEditKind::Add) {
        if(!existingName.empty()) {out.error="Add must not specify an existing target";return out;}
        if(profiles.size()>=1000) {out.error="Target count limit reached";return out;}
        profiles.push_back(replacement);
    } else {
        if(existingName.empty() || found==profiles.end()) {out.error="Target no longer exists";return out;}
        if(kind==TargetEditKind::Remove) profiles.erase(found); else *found=replacement;
    }
    try {out.updated=serialize(profiles);} catch(const std::exception&) {out.error="Could not encode target paths";return out;}
    if(out.updated.size()>1024U*1024U) {out.updated.clear();out.error="Target document exceeds size limit";return out;}
    std::vector<ToolchainProfile> validated; if(!parseToolchainProfiles(out.updated,validated,out.error)) {out.updated.clear();return out;}
    out.original=std::string(source); return out;
}
}
