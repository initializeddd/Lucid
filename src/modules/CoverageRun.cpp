#include "modules/CoverageRun.hpp"
#include "editor/json/Json.hpp"
#include <algorithm>
#include <set>
namespace lucid::modules {
CoverageRunPlan prepareCoverageRun(const workspace::Project& project, std::string_view configPath) {
    CoverageRunPlan out; out.root=project.root(); out.configPath=std::string(configPath);
    if(!project.readFileBounded(configPath,out.configSnapshot,out.error,64U*1024U)) return out;
    editor::JsonError parseError; auto root=editor::parseJson(out.configSnapshot,parseError);
    if(!root || root->type!=editor::JsonType::Object) {out.error="Invalid coverage generator configuration";return out;}
    std::set<std::string> fields;
    for(const auto& item:root->children) if((item.key!="executable" && item.key!="arguments" && item.key!="report" && item.key!="format") || !fields.insert(item.key).second) {out.error="Unknown or duplicate coverage configuration field";return out;}
    const auto* exe=root->find("executable"), *args=root->find("arguments"), *report=root->find("report"), *format=root->find("format");
    if(!exe || !args || !report || !format || exe->type!=editor::JsonType::String || args->type!=editor::JsonType::Array || report->type!=editor::JsonType::String || format->type!=editor::JsonType::String || args->children.size()>64) {out.error="Expected executable, arguments, report and format";return out;}
    const auto clean=[](std::string_view s,std::size_t max){return s.size()<=max && std::none_of(s.begin(),s.end(),[](unsigned char c){return c<32 || c==127;});};
    if(exe->stringValue.empty() || !clean(exe->stringValue,4096) || report->stringValue.empty() || !clean(report->stringValue,4096) || (format->stringValue!="lcov" && format->stringValue!="cobertura")) {out.error="Invalid coverage generator values";return out;}
    out.executable=std::filesystem::path(std::u8string(exe->stringValue.begin(),exe->stringValue.end()));
    std::error_code ec;
    if(!out.executable.is_absolute() || !std::filesystem::is_regular_file(out.executable,ec) || ec) {out.error="Coverage executable must be an existing absolute file";return out;}
    const std::filesystem::path rel{std::u8string(report->stringValue.begin(),report->stringValue.end())};
    if(rel.is_absolute() || rel.has_root_name() || rel.empty()) {out.error="Report must be project-relative";return out;}
    for(const auto& part:rel) if(part.empty() || part=="." || part=="..") {out.error="Invalid coverage report path";return out;}
    for(const auto& arg:args->children) {if(arg.type!=editor::JsonType::String || !clean(arg.stringValue,4096)) {out.error="Invalid coverage argument";return out;}out.arguments.push_back(arg.stringValue);}
    out.reportPath=report->stringValue;out.format=format->stringValue;return out;
}
ModuleProcessResult runCoverageGenerator(const workspace::Project& project,const CoverageRunPlan& plan,bool confirmed,const std::atomic_bool* cancel) {
    ModuleProcessResult result;
    if(!confirmed) {result.error="Explicit coverage run confirmation required";return result;}
    if(cancel && cancel->load()) {result.cancelled=true;return result;}
    if(!plan.error.empty() || plan.root!=project.root()) {result.error="Invalid coverage run plan";return result;}
    const auto fresh=prepareCoverageRun(project,plan.configPath);
    if(!fresh.error.empty() || fresh.configSnapshot!=plan.configSnapshot || fresh.executable!=plan.executable || fresh.arguments!=plan.arguments || fresh.reportPath!=plan.reportPath || fresh.format!=plan.format) {result.error="Coverage configuration changed; review again";return result;}
    return runModuleProcess(plan.executable,plan.arguments,std::chrono::minutes(5),1024U*1024U,cancel,project.root());
}
}
