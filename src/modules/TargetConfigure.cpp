#include "modules/TargetConfigure.hpp"
#include "modules/ToolchainTargets.hpp"
#include <algorithm>
namespace lucid::modules {
namespace fs=std::filesystem;
namespace {
std::string str(const fs::path& p) {auto u=p.generic_u8string();return {reinterpret_cast<const char*>(u.data()),u.size()};}
bool buildPath(const workspace::Project& project,std::string_view relative,fs::path& result,std::string& error) {
    if(relative.empty() || relative.size()>4096 || relative.find('\0')!=std::string_view::npos) {error="Invalid build directory";return false;}
    fs::path path{std::string(relative)}; if(path.is_absolute() || path.has_root_name()) {error="Build directory must be project-relative";return false;}
    auto current=project.root(); for(const auto& component:path) {if(component.empty() || component=="." || component=="..") {error="Invalid build directory";return false;} current/=component;std::error_code ec; auto st=fs::symlink_status(current,ec);if(ec || fs::is_symlink(st) || !fs::is_directory(st)) {error="Build directory missing or contains a symbolic link";return false;}}
    result=project.resolve(relative,error); return error.empty() && !result.empty();
}
}
TargetConfigurePlan prepareTargetConfigure(const workspace::Project& project,const ToolchainProfile& profile,std::string_view buildRelative,const fs::path& cmakeExecutable) {
    TargetConfigurePlan out; std::error_code ec;
    if(!cmakeExecutable.is_absolute() || !fs::is_regular_file(cmakeExecutable,ec) || ec) {out.error="Select an existing absolute CMake executable";return out;}
    auto draft=prepareTargetEdit("{\"profiles\":[]}",TargetEditKind::Add,"",profile); if(!draft.error.empty()){out.error=draft.error;return out;}
    if(!buildPath(project,buildRelative,out.buildResolved,out.error)) return out;
    if(!project.readFileBounded("CMakeLists.txt",out.sourceSnapshot,out.error,1024U*1024U)) return out;
    out.projectRoot=project.root();out.profile=profile;out.executable=cmakeExecutable;out.buildRelative=std::string(buildRelative);out.profileName=profile.name;out.declaredTarget=profile.targetTriple;
    out.arguments={"-S",str(project.root()),"-B",str(out.buildResolved)};
    auto add=[&](const char* key,const fs::path& value){if(!value.empty()) out.arguments.push_back(std::string("-D")+key+"="+str(value));};
    add("CMAKE_TOOLCHAIN_FILE",profile.cmakeToolchainFile);add("CMAKE_C_COMPILER",profile.cCompiler);add("CMAKE_CXX_COMPILER",profile.cppCompiler);add("CMAKE_SYSROOT",profile.sysroot);
    return out;
}
ModuleProcessResult runTargetConfigure(const workspace::Project& project,const TargetConfigurePlan& plan,bool confirmed,const std::atomic_bool* cancel) {
    ModuleProcessResult result;
    if(!confirmed){result.error="Explicit configure confirmation required";return result;}
    if(cancel && cancel->load()){result.cancelled=true;return result;}
    if(!plan.error.empty() || project.root()!=plan.projectRoot || plan.arguments.size()<4 || plan.arguments[0]!="-S" || plan.arguments[1]!=str(project.root()) || plan.arguments[2]!="-B" || plan.arguments[3]!=str(plan.buildResolved)) {result.error="Invalid target configure plan";return result;}
    auto fresh=prepareTargetConfigure(project,plan.profile,plan.buildRelative,plan.executable);
    if(!fresh.error.empty() || fresh.buildResolved!=plan.buildResolved || fresh.sourceSnapshot!=plan.sourceSnapshot || fresh.arguments!=plan.arguments || fresh.profileName!=plan.profileName || fresh.declaredTarget!=plan.declaredTarget){result.error="Project, build path or configuration changed; review again";return result;}
    if(cancel && cancel->load()){result.cancelled=true;return result;}
    return runModuleProcess(plan.executable,plan.arguments,std::chrono::minutes(5),1024U*1024U,cancel);
}
}
