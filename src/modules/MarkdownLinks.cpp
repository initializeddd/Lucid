#include "modules/MarkdownLinks.hpp"
#include <md4c.h>
#include <algorithm>
#include <filesystem>
#include <cstdint>

namespace lucid::modules {
namespace {
struct Context { const workspace::Project& project; std::filesystem::path parent; const std::atomic_bool* cancel; std::size_t limit; std::string_view source; MarkdownLinkResults result; std::vector<std::size_t> activeLinks; };
int hex(char c) { if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return c - 'a' + 10; if (c >= 'A' && c <= 'F') return c - 'A' + 10; return -1; }
MarkdownLink check(Context& c, const MD_ATTRIBUTE& attribute) {
    MarkdownLink result; if (attribute.size) result.destination.assign(attribute.text, attribute.size);
    auto raw = std::string_view(result.destination);
    if (raw.empty() || raw.front() == '#') { result.detail = "Fragment or empty target not checked"; return result; }
    const auto scheme = raw.find(':'); const auto firstSlash = raw.find_first_of("/?#");
    if (raw.starts_with("//") || (scheme != std::string_view::npos && (firstSlash == std::string_view::npos || scheme < firstSlash))) { result.detail = "External or non-file link not checked"; return result; }
    if (raw.starts_with('/') || raw.find('\\') != std::string_view::npos || raw.find('&') != std::string_view::npos) { result.detail = "Absolute or encoded link not checked"; return result; }
    const auto suffix = raw.find_first_of("?#"); const bool hasFragment = raw.find('#') != std::string_view::npos;
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
    if (hasFragment) { result.detail = "File exists; fragment not checked"; return result; }
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
    Context context{project, std::move(parent), cancel, limit, markdown, {}, {}};
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
