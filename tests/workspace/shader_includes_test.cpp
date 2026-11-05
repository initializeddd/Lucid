#include "modules/ShaderIncludes.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
using namespace lucid::modules;
int main() {
    namespace fs = std::filesystem;
    const auto root=fs::temp_directory_path()/("lucid-shader-includes-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root/"sub");
    { std::ofstream file(root/"main.vert"); file << "// #include \"ignored\"\n#include \"sub/a.glsl\"\n#include <system>\n#include MACRO\n#include \"missing.glsl\"\n#include \"../outside\"\n"; }
    { std::ofstream file(root/"sub"/"a.glsl"); file << "/* #include \"no\" */\n#include \"../main.vert\"\n#include \"common.glsl\"\n"; }
    { std::ofstream file(root/"sub"/"common.glsl"); file << "float x;\n"; }
    bool created=false; std::string error; auto project=lucid::workspace::Project::openOrCreate(root,created,error,false,false);
    auto graph=inspectShaderIncludes(project,"main.vert");
    if (!graph.incomplete || graph.cancelled || graph.edges.size()!=7 || graph.snapshots.size()!=3) return 1;
    if (graph.edges[0].status!=ShaderIncludeStatus::Resolved || graph.edges[0].line!=2 || graph.edges[1].status!=ShaderIncludeStatus::Cycle || graph.edges[2].status!=ShaderIncludeStatus::Resolved || graph.edges[3].status!=ShaderIncludeStatus::Unsupported || graph.edges[4].status!=ShaderIncludeStatus::Unsupported || graph.edges[5].status!=ShaderIncludeStatus::Unresolved || graph.edges[6].status!=ShaderIncludeStatus::Unresolved) return 2;
    if (graph.snapshots.at("sub/common.glsl")!="float x;\n") return 3;
    std::atomic_bool cancel{true}; graph=inspectShaderIncludes(project,"main.vert",&cancel); if (!graph.cancelled || !graph.incomplete || !graph.snapshots.empty()) return 4;
    graph=inspectShaderIncludes(project,"../outside"); if (!graph.incomplete || !graph.snapshots.empty()) return 5;
    { std::ofstream file(root/"sub"/"common.glsl"); file << "#include \"main.vert\"\\\n"; }
    graph=inspectShaderIncludes(project,"sub/common.glsl"); if (!graph.incomplete || graph.edges.size()!=1 || graph.edges[0].status!=ShaderIncludeStatus::Unsupported) return 6;
    fs::remove_all(root); return 0;
}
