#include "modules/MarkdownLinks.hpp"
#include <md4c.h>
#include <algorithm>
#include <filesystem>
#include <cstdint>
#include <map>
#include <set>

namespace lucid::modules {
namespace {
struct Anchors { std::set<std::string> slugs; bool certain=true; };
struct Context { const workspace::Project& project; std::filesystem::path parent; const std::atomic_bool* cancel; std::size_t limit; std::string_view source; MarkdownLinkResults result; std::vector<std::size_t> activeLinks; std::map<std::string,Anchors> anchors; std::size_t anchorBytes=0; };
int hex(char c) { if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return c - 'a' + 10; if (c >= 'A' && c <= 'F') return c - 'A' + 10; return -1; }
Anchors parseAnchors(std::string_view markdown, const std::atomic_bool* cancel) {
    struct State { Anchors result; std::map<std::string,std::size_t> nextSuffix; std::string title; bool inHeading=false; std::size_t headings=0; const std::atomic_bool* cancel=nullptr; } state;
    state.cancel=cancel;
    MD_PARSER parser{}; parser.flags=MD_DIALECT_GITHUB;
    parser.enter_block=+[](MD_BLOCKTYPE type, void*, void* data) { auto& s=*static_cast<State*>(data); if(s.cancel && s.cancel->load()) return 1; if(type==MD_BLOCK_HTML) s.result.certain=false; if(type==MD_BLOCK_H) { s.inHeading=true; s.title.clear(); if(++s.headings>10000) return 1; } return 0; };
    parser.leave_block=+[](MD_BLOCKTYPE type, void*, void* data) { auto& s=*static_cast<State*>(data); if(type==MD_BLOCK_H) { s.inHeading=false; std::string base; for(unsigned char c:s.title) { if(c>='A' && c<='Z') base+=static_cast<char>(c-'A'+'a'); else if((c>='a' && c<='z') || (c>='0' && c<='9') || c=='_' || c=='-') base+=static_cast<char>(c); else if(c==' ') base+='-'; else s.result.certain=false; } if(base.empty()) s.result.certain=false; else { auto slug=base; auto& suffix=s.nextSuffix[base]; while(s.result.slugs.count(slug)) slug=base+"-"+std::to_string(++suffix); s.result.slugs.insert(std::move(slug)); } } return 0; };
    parser.enter_span=+[](MD_SPANTYPE, void*, void* data) { auto& s=*static_cast<State*>(data); if(s.inHeading) s.result.certain=false; return 0; }; parser.leave_span=+[](MD_SPANTYPE, void*, void*) { return 0; };
    parser.text=+[](MD_TEXTTYPE type,const MD_CHAR* text,MD_SIZE size,void* data) { auto& s=*static_cast<State*>(data); if(s.cancel && s.cancel->load()) return 1; if(type==MD_TEXT_HTML) s.result.certain=false; if(s.inHeading) { if(type!=MD_TEXT_NORMAL || size>4096-s.title.size()) s.result.certain=false; else s.title.append(text,size); } return 0; };
    if(md_parse(markdown.data(),static_cast<MD_SIZE>(markdown.size()),&parser,&state)!=0) state.result.certain=false;
    return std::move(state.result);
}
void checkFragment(Context& c, MarkdownLink& result, std::string_view fragment, std::string_view relative, bool current) {
    if(fragment.empty() || fragment.size()>256 || !std::all_of(fragment.begin(),fragment.end(),[](unsigned char ch){return (ch>='a' && ch<='z') || (ch>='0' && ch<='9') || ch=='_' || ch=='-';})) { result.detail="Fragment syntax not checked"; return; }
    const std::string key=current ? std::string{} : std::string(relative);
    auto found=c.anchors.find(key);
    if(found==c.anchors.end()) {
        if(c.anchors.size()>=32) { result.detail="Fragment scan limit reached"; return; }
        std::string content; std::string_view text=c.source;
        if(!current) { std::string error; if(!c.project.readFileBounded(relative,content,error,1024U*1024U,c.cancel)) {result.detail="Could not read Markdown target"; return;} text=content; }
        if(text.size()>1024U*1024U || text.size()>8U*1024U*1024U-c.anchorBytes) {result.detail="Fragment scan limit reached"; return;}
        c.anchorBytes+=text.size(); found=c.anchors.emplace(key,parseAnchors(text,c.cancel)).first;
    }
    if(!found->second.certain) {result.detail="Heading syntax or custom anchors require manual checking";return;}
    if(found->second.slugs.count(std::string(fragment))) {result.status=MarkdownLinkStatus::Found;result.detail="Local Markdown heading found";}
    else {result.status=MarkdownLinkStatus::Missing;result.detail="Local Markdown heading not found";}
}
MarkdownLink check(Context& c, const MD_ATTRIBUTE& attribute) {
    MarkdownLink result; if (attribute.size) result.destination.assign(attribute.text, attribute.size);
    auto raw = std::string_view(result.destination);
    if (raw.empty()) { result.detail = "Empty target not checked"; return result; }
    if (raw.front() == '#') { checkFragment(c,result,raw.substr(1),{},true); return result; }
    const auto scheme = raw.find(':'); const auto firstSlash = raw.find_first_of("/?#");
    if (raw.starts_with("//") || (scheme != std::string_view::npos && (firstSlash == std::string_view::npos || scheme < firstSlash))) { result.detail = "External or non-file link not checked"; return result; }
    if (raw.starts_with('/') || raw.find('\\') != std::string_view::npos || raw.find('&') != std::string_view::npos) { result.detail = "Absolute or encoded link not checked"; return result; }
    const auto suffix = raw.find_first_of("?#"); const auto hash=raw.find('#'); const bool hasFragment = hash != std::string_view::npos;
    const auto fragment=hasFragment ? raw.substr(hash+1) : std::string_view{}; const bool hasQuery=raw.find('?')!=std::string_view::npos;
    raw = raw.substr(0, suffix); std::string decoded;
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '%') {
            if (i + 2 >= raw.size() || hex(raw[i + 1]) < 0 || hex(raw[i + 2]) < 0) { result.detail = "Invalid URL encoding"; return result; }
            const char ch = static_cast<char>((hex(raw[i + 1]) << 4) | hex(raw[i + 2])); i += 2;
            if (ch == '\0' || ch == '\\' || ch == '/') { result.detail = "Encoded separator not checked"; return result; }
            decoded.push_back(ch);
        } else decoded.push_back(raw[i]);
    }
    const auto relative = (c.parent / std::filesystem::path(std::u8string(decoded.begin(), decoded.end()))).lexically_normal();
    std::string error; const auto target = relative == "." ? c.project.root() : c.project.resolve([&] { const auto u = relative.generic_u8string(); return std::string(u.begin(), u.end()); }(), error);
    if (target.empty()) { result.detail = "Target outside project or unavailable"; return result; }
    std::error_code ec; const auto status = std::filesystem::status(target, ec);
    if (ec && ec != std::errc::no_such_file_or_directory) { result.detail = "Could not inspect target"; return result; }
    if (!std::filesystem::exists(status)) { result.status = MarkdownLinkStatus::Missing; result.detail = "File or directory not found"; return result; }
    if (hasFragment) { auto ext=relative.extension().string(); std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char ch){return static_cast<char>(ch>='A'&&ch<='Z'?ch-'A'+'a':ch);}); if(hasQuery || (ext!=".md" && ext!=".markdown")) {result.detail="File exists; fragment not checked"; return result;} const auto u=relative.generic_u8string(); checkFragment(c,result,fragment,std::string(u.begin(),u.end()),false); return result; }
    result.status = MarkdownLinkStatus::Found; result.detail = "Local target exists"; return result;
}
int enterSpan(MD_SPANTYPE type, void* detail, void* data) {
    auto& context = *static_cast<Context*>(data);
    if (context.cancel && context.cancel->load()) { context.result.incomplete = true; return 1; }
    if (type != MD_SPAN_A && type != MD_SPAN_IMG) return 0;
    if (context.result.links.size() >= context.limit) { context.result.incomplete = true; return 1; }
    const auto& attribute = type == MD_SPAN_A ? static_cast<MD_SPAN_A_DETAIL*>(detail)->href : static_cast<MD_SPAN_IMG_DETAIL*>(detail)->src;
    try { context.result.links.push_back(check(context, attribute)); }
    catch (const std::exception&) { MarkdownLink link; if (attribute.size) link.destination.assign(attribute.text, attribute.size); link.detail = "Could not interpret link path"; context.result.links.push_back(std::move(link)); }
    context.activeLinks.push_back(context.result.links.size() - 1); return 0;
}
}
MarkdownLinkResults checkMarkdownLinks(const workspace::Project& project, std::string_view documentPath, std::string_view markdown, const std::atomic_bool* cancel, std::size_t limit) {
    std::filesystem::path parent;
    try { parent = std::filesystem::path(std::u8string(documentPath.begin(), documentPath.end())).parent_path(); }
    catch (const std::exception&) { MarkdownLinkResults result; result.incomplete = true; return result; }
    Context context{project, std::move(parent), cancel, limit, markdown, {}, {}, {}, 0};
    if (markdown.size() > 16U * 1024U * 1024U || (cancel && cancel->load())) { context.result.incomplete = true; return context.result; }
    MD_PARSER parser{}; parser.flags = MD_DIALECT_GITHUB;
    parser.enter_block = +[](MD_BLOCKTYPE, void*, void* data) {
        auto& c = *static_cast<Context*>(data);
        if (c.cancel && c.cancel->load()) { c.result.incomplete = true; return 1; }
        return 0;
    }; parser.leave_block = parser.enter_block;
    parser.enter_span = enterSpan; parser.leave_span = +[](MD_SPANTYPE type, void*, void* data) {
        auto& c = *static_cast<Context*>(data);
        if ((type == MD_SPAN_A || type == MD_SPAN_IMG) && !c.activeLinks.empty()) c.activeLinks.pop_back();
        return 0;
    };
    parser.text = +[](MD_TEXTTYPE, const MD_CHAR* text, MD_SIZE size, void* data) {
        auto& c = *static_cast<Context*>(data);
        if (c.cancel && c.cancel->load()) { c.result.incomplete = true; return 1; }
        const auto start = reinterpret_cast<std::uintptr_t>(c.source.data());
        const auto ptr = reinterpret_cast<std::uintptr_t>(text);
        if (size && ptr >= start && ptr - start < c.source.size() && size <= c.source.size() - (ptr - start)) {
            for (auto index : c.activeLinks) if (!c.result.links[index].sourceOffset) c.result.links[index].sourceOffset = ptr - start;
        }
        return 0;
    };
    if (md_parse(markdown.empty() ? "" : markdown.data(), static_cast<MD_SIZE>(markdown.size()), &parser, &context) != 0) context.result.incomplete = true;
    return std::move(context.result);
}
}
