#include "ai/CodeCleanup.hpp"
#include <algorithm>
namespace lucid::ai {
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
