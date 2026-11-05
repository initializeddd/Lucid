#include "modules/CMakeBuild.hpp"
#include <algorithm>
namespace lucid::modules {
namespace fs=std::filesystem;
namespace {
std::string utf8(const fs::path& p) {auto v=p.generic_u8string();return {reinterpret_cast<const char*>(v.data()),v.size()};}
bool safeName(std::string_view text,bool emptyAllowed) {return (emptyAllowed || !text.empty()) && text.size()<=1024 && (text.empty() || text.front()!='-') && std::none_of(text.begin(),text.end(),[](unsigned char c){return c<32 || c==127;});}
bool buildPath(const workspace::Project& p,std::string_view input,fs::path& output,std::string& error) {
    if(input.empty() || input.size()>4096 || input.find('\0')!=std::string_view::npos) {error="Invalid build directory";return false;}
    fs::path relative{std::u8string(input.begin(),input.end())};if(relative.is_absolute() || relative.has_root_name()){error="Build directory must be project-relative";return false;}
    auto path=p.root();for(const auto& part:relative){if(part.empty()||part=="."||part==".."){error="Invalid build directory";return false;}path/=part;std::error_code ec;auto status=fs::symlink_status(path,ec);if(ec || fs::is_symlink(status) || !fs::is_directory(status)){error="Build directory missing or contains a symbolic link";return false;}}
    output=p.resolve(input,error);return error.empty() && !output.empty();
}
}
CMakeBuildPlan prepareCMakeBuild(const workspace::Project& project,std::string_view relative,std::string_view target,std::string_view config,const fs::path& executable) {
    CMakeBuildPlan out;std::error_code ec;
    if(!safeName(target,false) || !safeName(config,true)) {out.error="Invalid target or configuration";return out;}
    if(!executable.is_absolute() || !fs::is_regular_file(executable,ec) || ec){out.error="Select an existing absolute CMake executable";return out;}
    if(!buildPath(project,relative,out.buildDirectory,out.error))return out;
    CMakeModel model;if(!readCMakeModel(out.buildDirectory,model,out.error))return out;
    auto root=fs::weakly_canonical(project.root(),ec);if(ec){out.error="Project unavailable";return out;}auto source=fs::weakly_canonical(fs::path(std::u8string(model.sourceDirectory.begin(),model.sourceDirectory.end())),ec);
    if(ec || source!=root){out.error="CMake model belongs to another project";return out;}
    const CMakeTarget* selected=nullptr;for(const auto& t:model.targets) if(t.name==target && t.configuration==config){if(selected){out.error="CMake target is ambiguous";return out;}selected=&t;}
    if(!selected){out.error="CMake target is no longer present";return out;}
    if(!project.readFileBounded("CMakeLists.txt",out.sourceSnapshot,out.error,1024U*1024U))return out;
    out.projectRoot=project.root();out.executable=executable;out.buildRelative=std::string(relative);out.target=std::string(target);out.configuration=std::string(config);out.targetType=selected->type;
    out.arguments={"--build",utf8(out.buildDirectory),"--target",out.target};if(!out.configuration.empty()){out.arguments.push_back("--config");out.arguments.push_back(out.configuration);}return out;
}
ModuleProcessResult runCMakeBuild(const workspace::Project& project,const CMakeBuildPlan& plan,bool confirmed,const std::atomic_bool* cancel) {
    ModuleProcessResult out;if(!confirmed){out.error="Explicit build confirmation required";return out;}if(cancel && cancel->load()){out.cancelled=true;return out;}
    if(!plan.error.empty() || project.root()!=plan.projectRoot){out.error="Invalid build plan";return out;}
    auto fresh=prepareCMakeBuild(project,plan.buildRelative,plan.target,plan.configuration,plan.executable);
    if(!fresh.error.empty() || fresh.buildDirectory!=plan.buildDirectory || fresh.targetType!=plan.targetType || fresh.sourceSnapshot!=plan.sourceSnapshot || fresh.arguments!=plan.arguments){out.error="Build target or project changed; review again";return out;}
    if(cancel && cancel->load()){out.cancelled=true;return out;}return runModuleProcess(plan.executable,plan.arguments,std::chrono::minutes(5),1024U*1024U,cancel);
}
}
