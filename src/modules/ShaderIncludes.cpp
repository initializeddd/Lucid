#include "modules/ShaderIncludes.hpp"
#include <filesystem>
#include <functional>
#include <set>
#include <utility>
namespace lucid::modules {
namespace {
std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}
std::string utf8(const std::filesystem::path& p) { const auto s=p.generic_u8string(); return {s.begin(),s.end()}; }
// Strip comments while preserving literal bytes and line boundaries.
std::string strip(std::string_view line, bool& block) {
    std::string out; out.reserve(line.size()); char quote=0; bool escape=false;
    for (std::size_t i=0;i<line.size();++i) {
        const char c=line[i], next=i+1<line.size()?line[i+1]:'\0';
        if (block) { if (c=='*' && next=='/') { block=false; ++i; } continue; }
        if (quote) { out.push_back(c); if (escape) escape=false; else if (c=='\\') escape=true; else if (c==quote) quote=0; continue; }
        if (c=='/' && next=='/') break;
        if (c=='/' && next=='*') { block=true; out.push_back(' '); ++i; continue; }
        if (c=='"' || c=='\'') quote=c;
        out.push_back(c);
    }
    return out;
}
}
ShaderIncludeGraph inspectShaderIncludes(const workspace::Project& project, std::string_view relativeFile, const std::atomic_bool* cancel) {
    ShaderIncludeGraph graph; std::set<std::string> active, visited; std::size_t total=0;
    auto stopped=[&] { if (cancel && cancel->load()) { graph.cancelled=true; graph.incomplete=true; return true; } return false; };
    std::function<void(const std::string&,std::size_t)> visit;
    visit = [&](const std::string& path, std::size_t depth) {
        if (stopped()) return;
        if (depth > 16 || visited.size() >= 128) { graph.incomplete=true; return; }
        if (!visited.insert(path).second) return;
        std::string data,error;
        if (!project.readFileBounded(path,data,error,1024U*1024U,cancel) || data.size()>4U*1024U*1024U-total) { graph.incomplete=true; (void)stopped(); return; }
        total+=data.size(); graph.snapshots.emplace(path,data); active.insert(path);
        bool block=false, continued=false; std::size_t pos=0,lineNo=0;
        while (pos<data.size() && !stopped()) {
            const auto end=data.find('\n',pos); const auto count=(end==std::string::npos?data.size():end)-pos;
            auto stripped=strip(std::string_view(data).substr(pos,count),block); ++lineNo;
            pos=end==std::string::npos?data.size():end+1;
            auto text=trim(stripped);
            const bool wasContinued=continued; continued=!text.empty() && text.back()=='\\';
            if (continued) graph.incomplete=true;
            if (wasContinued || text.empty() || text.front()!='#') continue;
            text=trim(text.substr(1)); constexpr std::string_view keyword="include";
            if (!text.starts_with(keyword) || (text.size()>keyword.size() && text[keyword.size()]!=' ' && text[keyword.size()]!='\t' && text[keyword.size()]!='"' && text[keyword.size()]!='<')) continue;
            text=trim(text.substr(keyword.size()));
            if (graph.edges.size()>=1024) { graph.incomplete=true; break; }
            ShaderIncludeEdge edge; edge.source=path; edge.line=lineNo;
            if (text.size()<2 || text.front()!='"' || text.find('"',1)==std::string_view::npos) { edge.status=ShaderIncludeStatus::Unsupported; graph.incomplete=true; graph.edges.push_back(std::move(edge)); continue; }
            const auto close=text.find('"',1); const auto name=text.substr(1,close-1); const auto trailing=trim(text.substr(close+1));
            if (name.empty() || name.size()>4096 || name.find('\\')!=name.npos || name.find('\0')!=name.npos || !trailing.empty()) { edge.status=ShaderIncludeStatus::Unsupported; graph.incomplete=true; graph.edges.push_back(std::move(edge)); continue; }
            try {
                const auto requested=std::filesystem::path(std::u8string(name.begin(),name.end()));
                if (requested.is_absolute() || requested.has_root_name() || requested.has_root_directory()) { edge.status=ShaderIncludeStatus::Unsupported; graph.incomplete=true; graph.edges.push_back(std::move(edge)); continue; }
                const auto combined=(std::filesystem::path(std::u8string(path.begin(),path.end())).parent_path()/requested).lexically_normal(); edge.target=utf8(combined);
                std::string resolveError; const auto resolved=project.resolve(edge.target,resolveError);
                if (!resolveError.empty()) { edge.status=ShaderIncludeStatus::Unresolved; graph.incomplete=true; graph.edges.push_back(std::move(edge)); continue; }
                if (active.contains(edge.target)) { edge.status=ShaderIncludeStatus::Cycle; graph.incomplete=true; graph.edges.push_back(std::move(edge)); continue; }
                std::error_code ec;
                if (!std::filesystem::is_regular_file(resolved,ec) || ec || (!visited.contains(edge.target) && (depth>=16 || visited.size()>=128))) { edge.status=ShaderIncludeStatus::Unresolved; graph.incomplete=true; graph.edges.push_back(std::move(edge)); continue; }
                edge.status=ShaderIncludeStatus::Resolved; const auto target=edge.target; graph.edges.push_back(std::move(edge)); visit(target,depth+1);
            } catch (const std::exception&) { edge.status=ShaderIncludeStatus::Unsupported; graph.incomplete=true; graph.edges.push_back(std::move(edge)); }
        }
        if (block) graph.incomplete=true;
        active.erase(path);
    };
    try { const auto normalized=std::filesystem::path(std::u8string(relativeFile.begin(),relativeFile.end())).lexically_normal(); visit(utf8(normalized),0); }
    catch (const std::exception&) { graph.incomplete=true; }
    return graph;
}
}
