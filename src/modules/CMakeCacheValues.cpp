#include "modules/CMakeCacheValues.hpp"
#include <algorithm>
#include <filesystem>
#include <unordered_map>
namespace lucid::modules {
namespace {
bool validText(std::string_view s, std::size_t max) {return s.size()<=max && std::none_of(s.begin(),s.end(),[](unsigned char c){return c<32 || c==127;});}
bool safePath(const workspace::Project& p,std::string_view rel,std::string& error) {
    if(rel.empty() || rel.find('\0')!=std::string_view::npos) {error="Invalid cache path";return false;}
    const std::filesystem::path path{std::u8string(rel.begin(),rel.end())}; if(path.is_absolute() || path.has_root_name() || path.filename()!="CMakeCache.txt") {error="Invalid cache path";return false;}
    auto current=p.root(); for(const auto& part:path) {if(part.empty() || part=="." || part=="..") {error="Invalid cache path";return false;} current/=part; std::error_code ec; auto st=std::filesystem::symlink_status(current,ec); if(ec || std::filesystem::is_symlink(st) || !std::filesystem::exists(st)) {error="Cache path is missing or contains a symbolic link";return false;}}
    const auto resolved=p.resolve(rel,error);return error.empty() && !resolved.empty();
}
struct Entry {std::string name,type,value;std::size_t offset=0;};
bool read(const workspace::Project& project,std::string_view path,std::string& contents,std::vector<Entry>& entries,std::string& error) {
    entries.clear(); error.clear(); if(!safePath(project,path,error) || !project.readFileBounded(path,contents,error,4U*1024U*1024U)) return false;
    for(std::size_t pos=0;pos<contents.size();) {
        auto end=contents.find('\n',pos); if(end==std::string::npos) end=contents.size();auto lineEnd=end;if(lineEnd>pos && contents[lineEnd-1]=='\r') --lineEnd;
        std::string_view line(contents.data()+pos,lineEnd-pos);
        if(!line.empty() && !line.starts_with("//") && !line.starts_with('#')) {auto colon=line.find(':');auto equals=line.find('=');if(colon!=std::string_view::npos && equals!=std::string_view::npos && colon<equals) {
            if(entries.size()>=4096){error="CMake cache entry limit exceeded";return false;}
            entries.push_back({std::string(line.substr(0,colon)),std::string(line.substr(colon+1,equals-colon-1)),std::string(line.substr(equals+1)),pos+equals+1});
        } else {error="Malformed CMake cache entry";return false;}}
        pos=end==contents.size()?end:end+1;
    }
    return true;
}
bool choices(const std::vector<Entry>& entries,std::string_view name,std::vector<std::string>& result,std::string& error) {
    result.clear(); const auto key=std::string(name)+"-STRINGS";const Entry* found=nullptr;
    for(const auto& e:entries) if(e.name==key) {if(found || e.type!="INTERNAL") {error="Invalid or duplicate STRINGS property";return false;}found=&e;}
    if(!found) return true;
    if(found->value.size()>16384 || found->value.find('\\')!=std::string::npos) {error="Unsupported STRINGS property";return false;}
    std::string_view list=found->value;
    while(true) {auto sep=list.find(';');auto item=list.substr(0,sep);if(item.empty() || !validText(item,1024) || result.size()>=1000) {error="Invalid STRINGS property";return false;}result.emplace_back(item);if(sep==std::string_view::npos) break;list.remove_prefix(sep+1);}
    return true;
}
}
bool listCMakeCacheValues(const workspace::Project& project,std::string_view path,std::vector<CMakeCacheValue>& values,std::string& error) {
    values.clear();std::string contents;std::vector<Entry> entries;if(!read(project,path,contents,entries,error)) return false;
    std::unordered_map<std::string,unsigned> counts;for(const auto& e:entries) ++counts[e.name];
    for(const auto& e:entries) if(e.type=="STRING" || e.type=="PATH" || e.type=="FILEPATH") {
        if(counts[e.name]!=1 || e.name.empty() || !validText(e.name,1024) || !validText(e.value,16384)) {error="Invalid or duplicate editable cache entry";values.clear();return false;}
        CMakeCacheValue value{e.name,e.type,e.value,{}};if(!choices(entries,e.name,value.choices,error)) {values.clear();return false;} values.push_back(std::move(value));
    }
    return true;
}
bool prepareCMakeCacheValueUpdate(const workspace::Project& project,std::string_view path,std::string_view name,std::string_view type,std::string_view expected,std::string_view replacement,CMakeCacheUpdate& update,std::string& error) {
    update={};if(name.empty() || !validText(name,1024) || name.find_first_of(":=")!=std::string_view::npos || !validText(expected,16384) || !validText(replacement,16384) || (type!="STRING" && type!="PATH" && type!="FILEPATH")) {error="Invalid cache edit";return false;}
    std::string contents;std::vector<Entry> entries;if(!read(project,path,contents,entries,error)) return false;const Entry* found=nullptr;
    for(const auto& e:entries) if(e.name==name) {if(found) {error="Duplicate cache option";return false;}found=&e;}
    if(!found || found->type!=type || found->value!=expected) {error="Cache option changed; reload";return false;}
    std::vector<std::string> allowed;if(!choices(entries,name,allowed,error)) return false;
    if(!allowed.empty() && std::find(allowed.begin(),allowed.end(),replacement)==allowed.end()) {error="Value is not an allowed STRINGS choice";return false;}
    update.path=std::string(path);update.original=std::move(contents);update.replacement=update.original;update.replacement.replace(found->offset,found->value.size(),replacement);return true;
}
bool saveCMakeCacheValueUpdate(workspace::Project& project,const CMakeCacheUpdate& update,std::string& error) {if(!safePath(project,update.path,error)) return false;return project.writeFile(update.path,update.replacement,error,&update.original);}
}
