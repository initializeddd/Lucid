#include "modules/ToolchainProfiles.hpp"
#include "editor/json/Json.hpp"
#include <set>
namespace lucid::modules {
namespace {
using editor::JsonType;
bool textField(const editor::JsonValue& v, const char* key, std::string& out, bool required, std::string& error) {
    const auto* f = v.find(key);
    if (!f && !required) return true;
    if (!f || f->type != JsonType::String || f->stringValue.empty() || f->stringValue.size() > 4096 || f->stringValue.find('\0') != std::string::npos) { error = std::string("Invalid profile field: ") + key; return false; }
    for (unsigned char c : f->stringValue) if (c < 0x20 || c == 0x7f) { error = std::string("Invalid control character in profile field: ") + key; return false; }
    out = f->stringValue; return true;
}
bool pathField(const editor::JsonValue& v, const char* key, std::filesystem::path& out, std::string& error) {
    std::string value; if (!textField(v,key,value,false,error)) return false;
    if (value.empty()) return true;
    try { out = std::filesystem::path(std::u8string(value.begin(),value.end())); }
    catch (const std::exception&) { error = std::string("Invalid profile path: ") + key; return false; }
    if (!out.is_absolute()) { error = std::string("Path must be absolute: ") + key; return false; }
    return true;
}
}
bool parseToolchainProfiles(std::string_view json, std::vector<ToolchainProfile>& profiles, std::string& error) {
    profiles.clear(); error.clear();
    if (json.size() > 1024U*1024U) { error = "Toolchain profiles exceed size limit"; return false; }
    editor::JsonError e; auto root = editor::parseJson(json,e);
    if (!root || root->type != JsonType::Object) { error = "Invalid toolchain profiles JSON"; return false; }
    if (root->children.size() != 1 || root->children[0].key != "profiles") { error = "Unknown or duplicate root field"; return false; }
    const auto* list = root->find("profiles");
    if (!list || list->type != JsonType::Array || list->children.size() > 1000) { error = "Invalid profile list"; return false; }
    std::vector<ToolchainProfile> result; std::set<std::string> names;
    for (const auto& item:list->children) {
        if (item.type != JsonType::Object) { error = "Invalid toolchain profile"; return false; }
        static const std::set<std::string> allowed{"name","target","toolchainFile","cCompiler","cppCompiler","sysroot"};
        std::set<std::string> keys;
        for (const auto& f:item.children) if (!allowed.count(f.key) || !keys.insert(f.key).second) { error = "Unknown or duplicate profile field: " + f.key; return false; }
        ToolchainProfile p;
        if (!textField(item,"name",p.name,true,error) || !textField(item,"target",p.targetTriple,true,error) ||
            !pathField(item,"toolchainFile",p.cmakeToolchainFile,error) || !pathField(item,"cCompiler",p.cCompiler,error) ||
            !pathField(item,"cppCompiler",p.cppCompiler,error) || !pathField(item,"sysroot",p.sysroot,error)) return false;
        if (!names.insert(p.name).second) { error = "Duplicate toolchain profile name"; return false; }
        if (p.targetTriple.size() > 255 || p.targetTriple.front() == '-' || p.targetTriple.back() == '-') { error = "Invalid target triple"; return false; }
        for (unsigned char c:p.targetTriple) if (!((c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='_' || c=='-' || c=='.')) { error = "Invalid target triple"; return false; }
        if (p.cmakeToolchainFile.empty() && p.cCompiler.empty() && p.cppCompiler.empty()) { error = "Profile needs a toolchain file or compiler"; return false; }
        result.push_back(std::move(p));
    }
    profiles=std::move(result); return true;
}
}
