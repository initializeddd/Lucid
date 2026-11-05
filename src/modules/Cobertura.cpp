#include "modules/Coverage.hpp"
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <map>
#include <set>

namespace lucid::modules {
namespace {
bool whitespace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
bool nameChar(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == ':' || c == '.'; }
bool validXmlCharacter(std::uint32_t c) {
    return c == 9 || c == 10 || c == 13 || (c >= 0x20 && c <= 0xd7ff) ||
           (c >= 0xe000 && c <= 0xfffd) || (c >= 0x10000 && c <= 0x10ffff);
}
void appendUtf8(std::string& out, std::uint32_t c) {
    if (c < 0x80) out.push_back(static_cast<char>(c));
    else if (c < 0x800) { out.push_back(static_cast<char>(0xc0 | (c >> 6))); out.push_back(static_cast<char>(0x80 | (c & 63))); }
    else if (c < 0x10000) { out.push_back(static_cast<char>(0xe0 | (c >> 12))); out.push_back(static_cast<char>(0x80 | ((c >> 6) & 63))); out.push_back(static_cast<char>(0x80 | (c & 63))); }
    else { out.push_back(static_cast<char>(0xf0 | (c >> 18))); out.push_back(static_cast<char>(0x80 | ((c >> 12) & 63))); out.push_back(static_cast<char>(0x80 | ((c >> 6) & 63))); out.push_back(static_cast<char>(0x80 | (c & 63))); }
}
bool decode(std::string_view text, std::string& result) {
    result.clear();
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '&') { const auto c = static_cast<unsigned char>(text[i]); if ((c < 0x20 && c != 9 && c != 10 && c != 13) || text[i] == '<') return false; result += text[i]; continue; }
        const auto end = text.find(';', i + 1); if (end == std::string_view::npos) return false;
        const auto entity = text.substr(i, end - i + 1);
        if (entity == "&amp;") result += '&'; else if (entity == "&lt;") result += '<'; else if (entity == "&gt;") result += '>'; else if (entity == "&quot;") result += '"'; else if (entity == "&apos;") result += '\'';
        else if (entity.size() >= 4 && entity[1] == '#') {
            auto digits = entity.substr(2, entity.size() - 3); int base = 10;
            if (!digits.empty() && (digits.front() == 'x' || digits.front() == 'X')) { base = 16; digits.remove_prefix(1); }
            if (digits.empty() || digits.size() > 8) return false;
            std::uint32_t value = 0; const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value, base);
            if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size() || !validXmlCharacter(value)) return false;
            appendUtf8(result, value);
        } else return false;
        i = end;
    }
    return true;
}
std::string trim(std::string s) {
    while (!s.empty() && whitespace(s.back())) s.pop_back();
    auto i = s.begin(); while (i != s.end() && whitespace(*i)) ++i; s.erase(s.begin(), i); return s;
}
}
CoverageReport parseCobertura(const workspace::Project& project, std::string_view report, const std::atomic_bool* cancel, std::size_t limit) {
    CoverageReport out; std::map<std::pair<std::string,std::size_t>, bool> lines;
    if (report.size() > workspace::Project::maxFileSize) { out.incomplete = true; return out; }
    if (std::any_of(report.begin(), report.end(), [](unsigned char c) { return c < 0x20 && c != 9 && c != 10 && c != 13; })) { out.incomplete = true; return out; }
    limit = std::min<std::size_t>(limit, 100000);
    std::error_code ec; const auto root = std::filesystem::weakly_canonical(project.root(), ec);
    if (ec || root.empty()) { out.incomplete = true; return out; }
    std::vector<std::string> stack, sources; std::string sourceText, currentFile; bool sawRoot = false, endedRoot = false, classValid = false;
    auto resolveFile = [&](std::string_view name) {
        std::set<std::string> matches;
        auto check = [&](std::filesystem::path path) {
            if (path.is_absolute()) { auto canonical = std::filesystem::weakly_canonical(path, ec); if (ec) { ec.clear(); return; } path = canonical.lexically_relative(root); }
            std::string relative, error;
            try { const auto utf8 = path.lexically_normal().generic_u8string(); relative.assign(utf8.begin(), utf8.end()); }
            catch (const std::exception&) { return; }
            if (!project.resolve(relative, error).empty() && project.exists(relative) && !project.isDirectory(relative)) matches.insert(relative);
        };
        try {
            const std::filesystem::path filename{std::u8string(name.begin(), name.end())}; check(filename);
            if (!filename.is_absolute()) for (const auto& source : sources) check(std::filesystem::path(std::u8string(source.begin(), source.end())) / filename);
        } catch (const std::exception&) { ++out.skippedFiles; out.incomplete = true; return std::string{}; }
        if (matches.size() != 1) { ++out.skippedFiles; out.incomplete = true; return std::string{}; }
        return *matches.begin();
    };
    std::size_t pos = 0, count = 0;
    while (pos < report.size()) {
        if ((cancel && cancel->load()) || ++count > 200000) { out.incomplete = true; break; }
        if (report[pos] != '<') {
            const auto end = report.find('<', pos); const auto piece = report.substr(pos, end == std::string_view::npos ? report.size() - pos : end - pos);
            if (!stack.empty() && stack.back() == "source") { std::string text; if (!decode(piece, text) || sourceText.size() + text.size() > 4096) { out.incomplete = true; break; } sourceText += text; }
            else if (std::any_of(piece.begin(), piece.end(), [](char c) { return !whitespace(c); })) { out.incomplete = true; break; }
            pos = end == std::string_view::npos ? report.size() : end; continue;
        }
        if (report.substr(pos,4) == "<!--") { const auto end = report.find("-->", pos+4); if (end == std::string_view::npos) { out.incomplete = true; break; } pos=end+3; continue; }
        if (report.substr(pos,2) == "<?") { const auto end = report.find("?>", pos+2); if (end == std::string_view::npos) { out.incomplete = true; break; } pos=end+2; continue; }
        if (report.substr(pos,9) == "<!DOCTYPE") { const auto end = report.find('>', pos+9); if (sawRoot || end == std::string_view::npos || report.substr(pos,end-pos).find('[') != std::string_view::npos) { out.incomplete = true; break; } pos=end+1; continue; }
        if (report.substr(pos,2) == "<!") { out.incomplete = true; break; }
        std::size_t p=pos+1; bool closing = p<report.size() && report[p]=='/'; if (closing) ++p;
        const auto start=p; while (p<report.size() && nameChar(report[p])) ++p;
        if (p==start || p-start>256) { out.incomplete=true; break; } std::string name(report.substr(start,p-start));
        std::map<std::string,std::string> attrs; bool self=false, done=false;
        while (p<report.size()) {
            while (p<report.size() && whitespace(report[p])) ++p;
            if (p<report.size() && report[p]=='>') { ++p; done=true; break; }
            if (!closing && p+1<report.size() && report[p]=='/' && report[p+1]=='>') { p+=2; self=true; done=true; break; }
            if (closing) break;
            auto a=p; while (p<report.size() && nameChar(report[p])) ++p; if (a==p) break;
            if (p-a>256 || attrs.size()>=256) break;
            std::string key(report.substr(a,p-a)); while (p<report.size() && whitespace(report[p])) ++p;
            if (p>=report.size() || report[p++]!='=') break;
            while (p<report.size() && whitespace(report[p])) ++p;
            if (p>=report.size() || (report[p]!='\'' && report[p]!='"')) break;
            const char quote=report[p++]; a=p;
            while (p<report.size() && report[p]!=quote) ++p;
            if (p==report.size()) break;
            if (p-a>65536) break;
            std::string value; if (!decode(report.substr(a,p-a),value) || !attrs.emplace(std::move(key),std::move(value)).second) break; ++p;
        }
        if (!done) { out.incomplete=true; break; } pos=p;
        if (closing) {
            if (stack.empty() || stack.back()!=name) { out.incomplete=true; break; }
            if (name=="source") { if (sources.size()>=128) {out.incomplete=true; break;} auto text=trim(sourceText); if (!text.empty()) sources.push_back(std::move(text)); sourceText.clear(); }
            if (name=="class") { classValid=false; currentFile.clear(); }
            stack.pop_back(); if (stack.empty()) endedRoot=true; continue;
        }
        if (stack.empty()) { if (sawRoot || name!="coverage") {out.incomplete=true; break;} sawRoot=true; }
        if (endedRoot || stack.size()>=64) {out.incomplete=true; break;}
        if (name=="source" && !stack.empty() && stack.back()=="sources") sourceText.clear();
        if (name=="class") {
            if (std::find(stack.begin(), stack.end(), "class") != stack.end()) { out.incomplete = true; break; }
            const auto it=attrs.find("filename"); currentFile = it == attrs.end() || it->second.empty() || it->second.size()>4096 ? std::string{} : resolveFile(it->second);
            if (it==attrs.end() || it->second.empty() || it->second.size()>4096) { ++out.skippedFiles; out.incomplete=true; } classValid=!currentFile.empty();
        }
        if (name=="line" && classValid && stack.size()>=2 && stack.back()=="lines" && stack[stack.size()-2]=="class") {
            const auto n=attrs.find("number"), h=attrs.find("hits"); std::size_t number=0; unsigned long long hits=0;
            if (n==attrs.end() || h==attrs.end()) out.incomplete=true;
            else { const auto a=std::from_chars(n->second.data(),n->second.data()+n->second.size(),number), b=std::from_chars(h->second.data(),h->second.data()+h->second.size(),hits);
                if (a.ec!=std::errc{} || a.ptr!=n->second.data()+n->second.size() || !number || b.ec!=std::errc{} || b.ptr!=h->second.data()+h->second.size()) out.incomplete=true;
                else { auto key=std::make_pair(currentFile,number-1); auto found=lines.find(key); if (found!=lines.end()) found->second |= hits!=0; else if (lines.size()>=limit) {out.incomplete=true; break;} else lines.emplace(std::move(key),hits!=0); }
            }
        }
        if (!self) stack.push_back(std::move(name)); else if (stack.empty()) endedRoot=true;
    }
    if (!sawRoot || !endedRoot || !stack.empty()) out.incomplete=true;
    for (const auto& [key, covered] : lines) out.lines.push_back({key.first,key.second,covered});
    return out;
}
}
