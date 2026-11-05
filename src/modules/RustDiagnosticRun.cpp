#include "modules/RustDiagnosticRun.hpp"
#include <algorithm>
namespace lucid::modules {
namespace {
std::string utf8(const std::filesystem::path& p){auto v=p.generic_u8string();return {reinterpret_cast<const char*>(v.data()),v.size()};}
}
RustDiagnosticPlan prepareRustDiagnosticRun(const workspace::Project& project,const std::filesystem::path& cargo) {
    RustDiagnosticPlan out;std::error_code ec;
    if(!cargo.is_absolute() || !std::filesystem::is_regular_file(cargo,ec) || ec){out.error="Select an existing absolute Cargo executable";return out;}
    auto status=std::filesystem::symlink_status(project.root()/"Cargo.toml",ec);if(ec || status.type()!=std::filesystem::file_type::regular){out.error="Project Cargo.toml is missing or a symbolic link";return out;}
    if(!project.readFileBounded("Cargo.toml",out.manifestSnapshot,out.error,1024U*1024U))return out;
    out.root=project.root();out.cargo=cargo;out.arguments={"check","--locked","--offline","--message-format=json","--manifest-path",utf8(project.root()/"Cargo.toml")};return out;
}
RustDiagnosticRun runRustDiagnostic(const workspace::Project& project,const RustDiagnosticPlan& plan,bool confirmed,const std::atomic_bool* cancel) {
    RustDiagnosticRun out;if(!confirmed){out.error="Explicit Cargo execution confirmation required";return out;}if(cancel && cancel->load()){out.process.cancelled=true;return out;}
    if(!plan.error.empty() || plan.root!=project.root()){out.error="Invalid Cargo review plan";return out;}
    auto fresh=prepareRustDiagnosticRun(project,plan.cargo);if(!fresh.error.empty() || fresh.manifestSnapshot!=plan.manifestSnapshot || fresh.arguments!=plan.arguments){out.error="Cargo manifest or command changed; review again";return out;}
    out.process=runModuleProcess(plan.cargo,plan.arguments,std::chrono::minutes(5),8U*1024U*1024U,cancel);
    out.incomplete=out.process.outputTruncated || out.process.cancelled || out.process.timedOut || !out.process.error.empty();
    // stdout and stderr are merged by the runner. Parse complete JSON object lines
    // independently and flag malformed/truncated lines instead of claiming success.
    std::string_view remaining=out.process.output;
    while(!remaining.empty()) {
        auto end=remaining.find('\n'); if(end==std::string_view::npos){out.incomplete=true;break;}
        auto line=remaining.substr(0,end);remaining.remove_prefix(end+1);if(!line.empty() && line.back()=='\r')line.remove_suffix(1);
        auto first=line.find_first_not_of(" \t");if(first==std::string_view::npos)continue;
        if(line[first]!='{'){++out.ignoredLines;continue;}
        std::vector<RustOwnershipDiagnostic> parsed;std::string error;
        if(!parseRustOwnershipDiagnostics(line,parsed,error)){out.incomplete=true;continue;}
        if(parsed.size()>5000-out.diagnostics.size()){out.incomplete=true;break;}
        out.diagnostics.insert(out.diagnostics.end(),std::make_move_iterator(parsed.begin()),std::make_move_iterator(parsed.end()));
    }
    return out;
}
}
