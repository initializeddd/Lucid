#include "workspace/search/ProjectSearch.hpp"
#include "editor/document/Document.hpp"
#include "workspace/recovery/History.hpp"
#include <algorithm>
namespace lucid::workspace {
ProjectSearchResults searchProject(const Project& project, std::string_view query, std::size_t limit) {
    ProjectSearchResults result;
    std::size_t storedBytes = 0;
    if (query.empty() || query.find('\n') != std::string_view::npos || query.find('\r') != std::string_view::npos) return result;
    for (const auto& path : project.files()) {
        std::string content, error;
        if (!project.readFile(path, content, error) ||
            std::any_of(content.begin(), content.end(), [](unsigned char c) { return c == 0 || (c < 32 && c != '\t' && c != '\n' && c != '\r'); }) ||
            !editor::Document::canEditLosslessly(content, error)) { ++result.skipped; continue; }
        std::size_t pos = 0, lineStart = 0, line = 0, fileIndex = result.files.size();
        bool added = false;
        while ((pos = content.find(query, pos)) != std::string::npos) {
            if (result.matches.size() >= limit) { result.truncated = true; return result; }
            while (lineStart < pos) {
                const auto newline = content.find_first_of("\r\n", lineStart);
                if (newline == std::string::npos || newline >= pos) break;
                lineStart = newline + 1;
                if (content[newline] == '\r' && lineStart < content.size() && content[lineStart] == '\n') ++lineStart;
                ++line;
            }
            if (!added) {
                if (content.size() > 64U * 1024U * 1024U - storedBytes) { result.truncated = true; return result; }
                storedBytes += content.size(); result.files.push_back({path, content}); added = true;
            }
            std::size_t column = 0;
            for (std::size_t i = lineStart; i < pos; ++i) if ((static_cast<unsigned char>(content[i]) & 0xc0U) != 0x80U) ++column;
            const auto lineEnd = content.find_first_of("\r\n", pos);
            const auto snippet = content.substr(lineStart, std::min<std::size_t>((lineEnd == std::string::npos ? content.size() : lineEnd) - lineStart, 180));
            result.matches.push_back({fileIndex, pos, line, column, snippet});
            pos += query.size();
        }
    }
    return result;
}
bool replaceProject(Project& project, const ProjectSearchResults& results, std::string_view query,
                    std::string_view replacement, std::size_t& changed, std::string& error,
                    std::vector<std::string>* changedPaths, const std::filesystem::path& historyRoot) {
    changed = 0; error.clear();
    if (changedPaths) changedPaths->clear();
    if (query.empty() || results.truncated) { error = "Search is empty or incomplete; replacement was not applied"; return false; }
    // Preflight all files before modifying any, and recheck each one at replacement time.
    for (const auto& file : results.files) {
        std::string current;
        if (!project.readFile(file.path, current, error) || current != file.original) { error = "File changed since review: " + file.path; return false; }
    }
    if (!historyRoot.empty() && query != replacement) {
        for (const auto& file : results.files) {
            recovery::Snapshot backup; backup.project = project.root().generic_string(); backup.file = file.path;
            backup.contents = file.original;
            if (!recovery::recordHistory(historyRoot, backup, error)) { error = "No files changed; could not back up " + file.path + ": " + error; return false; }
        }
    }
    for (const auto& file : results.files) {
        std::string updated; std::size_t start = 0, pos = 0;
        while ((pos = file.original.find(query, start)) != std::string::npos) {
            updated.append(file.original, start, pos - start); updated.append(replacement); start = pos + query.size();
        }
        updated.append(file.original, start, std::string::npos);
        if (updated == file.original) continue;
        if (!project.writeFile(file.path, updated, error, &file.original)) { error = "Stopped after " + std::to_string(changed) + " files: " + error; return false; }
        ++changed;
        if (changedPaths) changedPaths->push_back(file.path);
    }
    return true;
}
}
