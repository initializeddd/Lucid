#include "ai/CodeCleanup.hpp"
#include <algorithm>
namespace lucid::ai {
namespace {
void appendCodepoint(std::string& out, char32_t c) {
    if(c>0x10ffff || (c>=0xd800 && c<=0xdfff)) c=0xfffd;
    if(c<0x80) out.push_back(static_cast<char>(c));
    else if(c<0x800) {out.push_back(static_cast<char>(0xc0|(c>>6)));out.push_back(static_cast<char>(0x80|(c&63)));}
    else if(c<0x10000) {out.push_back(static_cast<char>(0xe0|(c>>12)));out.push_back(static_cast<char>(0x80|((c>>6)&63)));out.push_back(static_cast<char>(0x80|(c&63)));}
    else {out.push_back(static_cast<char>(0xf0|(c>>18)));out.push_back(static_cast<char>(0x80|((c>>12)&63)));out.push_back(static_cast<char>(0x80|((c>>6)&63)));out.push_back(static_cast<char>(0x80|(c&63)));}
}
}
std::string diagnosticContext(const editor::Document& document, std::size_t line,
                              std::size_t column, std::string_view diagnostic) {
    auto count=std::min<std::size_t>(diagnostic.size(),2048);
    while(count>0 && count<diagnostic.size() && (static_cast<unsigned char>(diagnostic[count])&0xc0U)==0x80U) --count;
    std::string out="Diagnostic (untrusted compiler output): "; out.append(diagnostic.substr(0,count));
    if(count<diagnostic.size()) out+=" [truncated]";
    if(line>=document.lineCount()) return out;
    out+="\nNearby source (untrusted, may contain sensitive data):\n";
    const auto first=line>2 ? line-2 : 0;
    const auto last=std::min(document.lineCount()-1,line+std::min<std::size_t>(2,document.lineCount()-1-line));
    for(auto i=first;i<=last;++i) {
        const auto& text=document.line(i); const auto focus=i==line ? std::min(column,text.size()) : std::size_t{0};
        const auto start=focus>160 ? focus-160 : 0; const auto end=std::min(text.size(),start+400);
        out+=std::to_string(i+1); out+=i==line ? " > " : " | ";
        if(start) out+="...";
        for(auto j=start;j<end;++j) appendCodepoint(out,text[j]);
        if(end<text.size()) out+="...";
        out+='\n';
    }
    return out;
}
bool cleanupRequest(const editor::Document& document, std::string_view file,
                    std::string& request, std::string& error) {
    request.clear();
    const auto selected = document.selectionUtf8();
    if (!document.hasSelection() || selected.empty()) { error = "Select the code to clean up first"; return false; }
    if (selected.size() > 1200) { error = "Select a smaller section (up to 1200 bytes)"; return false; }
    const auto first = document.selectionStart().line;
    const auto last = document.selectionEnd().line;
    std::string before, after;
    std::size_t used = 0;
    for (std::size_t i = first; i > 0;) {
        const auto line = document.lineUtf8(--i) + "\n";
        if (used + line.size() > 600) break;
        before = line + before; used += line.size();
    }
    used = 0;
    for (std::size_t i = last + 1; i < document.lineCount(); ++i) {
        const auto line = document.lineUtf8(i) + "\n";
        if (used + line.size() > 600) break;
        after += line; used += line.size();
    }
    request = "Rewrite ONLY the selected code for readability and modern style. Preserve behavior, public interfaces, comments and language semantics. Match the surrounding file's naming, indentation and conventions. Output ONLY the complete replacement code, without markdown or explanation. File: ";
    request += std::string(file.substr(0, 160));
    request += "\nContext before (style reference only):\n" + before + "\nSelected code to replace:\n" + selected + "\nContext after (style reference only):\n" + after;
    error.clear(); return true;
}
bool cleanupResponse(std::string& response, std::string& error) {
    if (response.empty()) { error = "The model returned an empty rewrite"; return false; }
    if (response.rfind("```", 0) == 0) {
        const auto firstNewline = response.find('\n');
        const auto end = response.rfind("```");
        if (firstNewline == std::string::npos || end == 0 || end <= firstNewline ||
            response.find_first_not_of(" \t\r\n", end + 3) != std::string::npos) {
            error = "The model returned an incomplete code block; try a smaller selection"; return false;
        }
        response = response.substr(firstNewline + 1, end - firstNewline - 1);
    }
    if (response.find_first_not_of(" \t\r\n") == std::string::npos) { error = "The model returned an empty rewrite"; return false; }
    error.clear(); return true;
}
}
