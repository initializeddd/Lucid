#include "modules/BoardProfiles.hpp"
#include <charconv>
#include <algorithm>
namespace lucid::modules {
namespace {
std::string_view trim(std::string_view s) { while (!s.empty() && (s.front()==' ' || s.front()=='\t' || s.front()=='\r')) s.remove_prefix(1); while (!s.empty() && (s.back()==' ' || s.back()=='\t' || s.back()=='\r')) s.remove_suffix(1); return s; }
bool safe(std::string_view s) {
    if (s.empty() || s.size() > 1024 || s.front()=='/' || s.front()=='\\') return false;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == '/') { auto part = s.substr(start, i-start); if (part.empty() || part=="." || part=="..") return false; start=i+1; }
        else { char c=s[i]; if (!((c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='_' || c=='-' || c=='.')) return false; }
    }
    return true;
}
}
BoardProfile parseBoardProfile(std::string_view text, std::size_t maxBytes, std::size_t maxLines) {
    BoardProfile out; std::size_t pos=0, lines=0;
    while (pos<text.size()) {
        if (pos>=maxBytes || lines>=maxLines) { out.incomplete=true; break; }
        auto window=text.substr(pos, std::min(maxBytes-pos,text.size()-pos)); auto nl=window.find('\n');
        if (nl==std::string_view::npos && window.size()<text.size()-pos) { out.incomplete=true; break; }
        auto line=trim(window.substr(0,nl)); pos += (nl==std::string_view::npos ? window.size() : nl+1); ++lines;
        if (line.empty() || line.front()=='#') continue;
        auto split=line.find_first_of(" \t"); auto cmd=line.substr(0,split); auto arg=split==std::string_view::npos ? std::string_view{} : trim(line.substr(split));
        if (cmd=="source") {
            if (!safe(arg) || !arg.ends_with(".cfg")) { out.error="Unsafe or unsupported source path on line " + std::to_string(lines); return out; }
            std::string* destination=nullptr;
            if (arg.starts_with("interface/")) destination=&out.interfaceConfig;
            else if (arg.starts_with("target/")) destination=&out.targetConfig;
            else { out.warnings.push_back("Unrecognized source on line " + std::to_string(lines)); continue; }
            if (!destination->empty()) { out.error="Duplicate source on line " + std::to_string(lines); return out; } *destination=arg;
        } else if (cmd=="adapter_khz" || cmd=="adapter-speed") {
            unsigned n=0; auto [p,ec]=std::from_chars(arg.data(),arg.data()+arg.size(),n);
            if (out.adapterKhz || arg.empty() || ec!=std::errc{} || p!=arg.data()+arg.size() || !n || n>100000) { out.error="Invalid adapter speed on line " + std::to_string(lines); return out; } out.adapterKhz=n;
        } else if (cmd=="firmware") {
            if (!out.firmware.empty() || !safe(arg)) { out.error="Unsafe or duplicate firmware path on line " + std::to_string(lines); return out; } out.firmware=arg;
        } else { out.warnings.push_back("Unsupported directive on line " + std::to_string(lines)); }
    }
    return out;
}
}
