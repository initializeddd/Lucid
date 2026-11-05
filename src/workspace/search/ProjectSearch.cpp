#include "workspace/search/ProjectSearch.hpp"
#include "editor/document/Document.hpp"
#include "workspace/recovery/History.hpp"
#include <algorithm>
namespace lucid::workspace {
namespace {
unsigned char foldAscii(unsigned char c) { return c >= 'A' && c <= 'Z' ? static_cast<unsigned char>(c + ('a' - 'A')) : c; }
bool sameText(std::string_view left, std::string_view right, bool caseSensitive) {
    if (left.size() != right.size()) return false;
    return std::equal(left.begin(), left.end(), right.begin(), [caseSensitive](unsigned char a, unsigned char b) {
        return caseSensitive ? a == b : foldAscii(a) == foldAscii(b);
    });
}
bool wordByte(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c >= 0x80;
}
struct PathRule { std::string_view pattern; bool exclude; bool wildcard; };
std::vector<PathRule> pathRules(std::string_view filter) {
    std::vector<PathRule> rules;
    while (!filter.empty()) {
        const auto comma = filter.find(',');
        auto part = filter.substr(0, comma);
        while (!part.empty() && (part.front() == ' ' || part.front() == '\t')) part.remove_prefix(1);
        while (!part.empty() && (part.back() == ' ' || part.back() == '\t')) part.remove_suffix(1);
        const bool exclude = !part.empty() && part.front() == '!';
        if (exclude) part.remove_prefix(1);
        if (!part.empty()) rules.push_back({part, exclude, part.find_first_of("*?") != std::string_view::npos});
        if (comma == std::string_view::npos) break;
        filter.remove_prefix(comma + 1);
    }
    return rules;
}
bool pathWildcard(std::string_view pattern, std::string_view path) {
    std::size_t p = 0, s = 0, star = std::string_view::npos, retry = 0;
    while (s < path.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == path[s])) { ++p; ++s; }
        else if (p < pattern.size() && pattern[p] == '*') { star = p++; retry = s; }
        else if (star != std::string_view::npos) { p = star + 1; s = ++retry; }
        else return false;
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}
bool pathAllowed(std::string_view path, const std::vector<PathRule>& rules) {
    bool hasIncludes = false, included = false;
    for (const auto& rule : rules) {
        const bool matches = rule.wildcard ? pathWildcard(rule.pattern, path) : path.find(rule.pattern) != std::string_view::npos;
        if (rule.exclude && matches) return false;
        if (!rule.exclude) { hasIncludes = true; included = included || matches; }
    }
    return !hasIncludes || included;
}
// Keep the match in view, and never cut a UTF-8 character at either edge.
std::string searchPreview(const std::string& content, std::size_t lineStart,
                          std::size_t lineEnd, std::size_t match) {
    constexpr std::size_t kContextBefore = 60;
    constexpr std::size_t kPreviewBytes = 180;
    std::size_t start = match - lineStart > kContextBefore ? match - kContextBefore : lineStart;
    while (start < match && (static_cast<unsigned char>(content[start]) & 0xc0U) == 0x80U) ++start;
    std::size_t end = std::min(lineEnd, start + kPreviewBytes);
    while (end > start && end < lineEnd &&
           (static_cast<unsigned char>(content[end]) & 0xc0U) == 0x80U) --end;
    std::string result;
    if (start > lineStart) result += "...";
    result.append(content, start, end - start);
    if (end < lineEnd) result += "...";
    return result;
}
} // namespace

bool validProjectPathFilter(std::string_view filter) {
    while (!filter.empty()) {
        const auto comma = filter.find(',');
        auto part = filter.substr(0, comma);
        while (!part.empty() && (part.front() == ' ' || part.front() == '\t')) part.remove_prefix(1);
        while (!part.empty() && (part.back() == ' ' || part.back() == '\t')) part.remove_suffix(1);
        if (part == "!" || part.find_first_of("\r\n") != std::string_view::npos) return false;
        if (comma == std::string_view::npos) break;
        filter.remove_prefix(comma + 1);
    }
    return true;
}

std::string replacementPreview(const ProjectSearchFile& file, const ProjectSearchMatch& match,
                               std::string_view query, std::string_view replacement, bool caseSensitive) {
    const auto& text = file.original; const auto pos = match.offset;
    if (query.empty() || pos > text.size() || query.size() > text.size() - pos || !sameText(std::string_view(text).substr(pos, query.size()), query, caseSensitive)) return "Preview unavailable";
    const auto previous = pos == 0 ? std::string::npos : text.find_last_of("\r\n", pos - 1);
    const auto lineStart = previous == std::string::npos ? 0 : previous + 1;
    const auto after = pos + query.size();
    const auto newline = text.find_first_of("\r\n", after);
    const auto lineEnd = newline == std::string::npos ? text.size() : newline;
    auto start = pos - std::min<std::size_t>(60, pos - lineStart);
    while (start < pos && (static_cast<unsigned char>(text[start]) & 0xc0U) == 0x80U) ++start;
    auto end = std::min(lineEnd, after + std::min<std::size_t>(60, lineEnd - after));
    while (end > after && end < lineEnd && (static_cast<unsigned char>(text[end]) & 0xc0U) == 0x80U) --end;
    std::string result = start > lineStart ? "..." : "";
    result.append(text, start, pos - start);
    std::size_t shown = std::min<std::size_t>(replacement.size(), 80);
    while (shown > 0 && shown < replacement.size() && (static_cast<unsigned char>(replacement[shown]) & 0xc0U) == 0x80U) --shown;
    for (std::size_t i = 0; i < shown; ++i) {
        if (replacement[i] == '\r' || replacement[i] == '\n') {
            if (replacement[i] == '\r' && i + 1 < shown && replacement[i + 1] == '\n') ++i;
            result += " ↵ ";
        } else result.push_back(replacement[i]);
    }
    if (shown < replacement.size()) result += "...";
    result.append(text, after, end - after);
    if (end < lineEnd) result += "...";
    return result;
}

ProjectSearchResults searchProject(const Project& project, std::string_view query, std::size_t limit, const std::atomic_bool* cancel, bool wholeWord, bool caseSensitive, std::string_view pathFilter) {
    ProjectSearchResults result;
    result.caseSensitive = caseSensitive;
    std::size_t skippedDetailBytes = 0;
    const auto recordSkipped = [&result, &skippedDetailBytes](const std::string& path, std::string_view reason) {
        ++result.skipped;
        if (result.firstSkippedPath.empty()) { result.firstSkippedPath = path; result.firstSkippedReason = reason; }
        constexpr std::size_t maxDetails = 1000, maxBytes = 256U * 1024U;
        if (result.skippedFiles.size() < maxDetails && path.size() + reason.size() <= maxBytes - skippedDetailBytes) {
            skippedDetailBytes += path.size() + reason.size();
            result.skippedFiles.push_back({path, std::string(reason)});
        }
    };
    std::size_t storedBytes = 0;
    if (!validProjectPathFilter(pathFilter)) { result.indexIncomplete = true; return result; }
    if (query.empty() || query.find('\n') != std::string_view::npos || query.find('\r') != std::string_view::npos) return result;
    const auto rules = pathRules(pathFilter);
    const auto normalize = [caseSensitive](unsigned char c) { return caseSensitive ? c : foldAscii(c); };
    std::vector<unsigned char> pattern; pattern.reserve(query.size());
    for (const unsigned char c : query) pattern.push_back(normalize(c));
    std::vector<std::size_t> prefix(pattern.size());
    for (std::size_t i = 1, length = 0; i < pattern.size(); ++i) {
        while (length && pattern[i] != pattern[length]) length = prefix[length - 1];
        if (pattern[i] == pattern[length]) ++length;
        prefix[i] = length;
        if ((i & 4095U) == 0 && cancel && cancel->load()) { result.indexIncomplete = true; return result; }
    }
    for (const auto& path : project.files()) {
        if (cancel && cancel->load()) { result.indexIncomplete = true; return result; }
        if (!pathAllowed(path, rules)) continue;
        std::string content, error;
        if (!project.readFile(path, content, error, cancel)) {
            if (cancel && cancel->load()) { result.indexIncomplete = true; return result; }
            recordSkipped(path, "could not read"); ++result.readErrors;
            if (result.firstReadErrorPath.empty()) result.firstReadErrorPath = path;
            continue;
        }
        bool binary = false;
        for (std::size_t i = 0; i < content.size(); ++i) {
            if ((i & 4095U) == 0 && cancel && cancel->load()) { result.indexIncomplete = true; return result; }
            const auto c = static_cast<unsigned char>(content[i]);
            if (c == 0 || (c < 32 && c != '\t' && c != '\n' && c != '\r')) { binary = true; break; }
        }
        if (binary || !editor::Document::canEditLosslessly(content, error, cancel)) {
            if (cancel && cancel->load()) { result.indexIncomplete = true; return result; }
            recordSkipped(path, binary ? "binary content" : "unsupported text encoding or line endings");
            continue;
        }
        std::size_t pos = 0, scanned = 0, lineStart = 0, line = 0, column = 0;
        std::size_t lineEnd = 0;
        const std::size_t fileIndex = result.files.size();
        bool needLineEnd = true;
        bool added = false;
        std::size_t cursor = 0, matched = 0;
        while (cursor < content.size()) {
            while (cursor < content.size() && matched < pattern.size()) {
                if ((cursor & 4095U) == 0 && cancel && cancel->load()) { result.indexIncomplete = true; return result; }
                const auto byte = normalize(static_cast<unsigned char>(content[cursor++]));
                while (matched && byte != pattern[matched]) matched = prefix[matched - 1];
                if (byte == pattern[matched]) ++matched;
            }
            if (matched != pattern.size()) break;
            pos = cursor - pattern.size();
            if (cancel && cancel->load()) { result.indexIncomplete = true; return result; }
            if (wholeWord && ((pos > 0 && wordByte(static_cast<unsigned char>(content[pos - 1]))) ||
                (pos + query.size() < content.size() && wordByte(static_cast<unsigned char>(content[pos + query.size()]))))) {
                matched = prefix[matched - 1]; continue;
            }
            if (result.matches.size() >= limit) { result.truncated = true; return result; }
            while (scanned < pos) {
                if ((scanned & 4095U) == 0 && cancel && cancel->load()) { result.indexIncomplete = true; return result; }
                const unsigned char byte = static_cast<unsigned char>(content[scanned++]);
                if (byte == '\r' || byte == '\n') {
                    if (byte == '\r' && scanned < pos && content[scanned] == '\n') ++scanned;
                    lineStart = scanned;
                    ++line;
                    column = 0;
                    needLineEnd = true;
                } else if ((byte & 0xc0U) != 0x80U) {
                    ++column;
                }
            }
            if (!added) {
                if (content.size() > 64U * 1024U * 1024U - storedBytes) { result.truncated = true; return result; }
                storedBytes += content.size(); result.files.push_back({path, content}); added = true;
            }
            if (needLineEnd) {
                lineEnd = content.find_first_of("\r\n", pos);
                if (lineEnd == std::string::npos) lineEnd = content.size();
                needLineEnd = false;
            }
            const auto snippet = searchPreview(content, lineStart, lineEnd, pos);
            result.matches.push_back({fileIndex, pos, line, column, snippet});
            matched = 0;
        }
    }
    return result;
}
bool replaceProject(Project& project, const ProjectSearchResults& results, std::string_view query,
                    std::string_view replacement, std::size_t& changed, std::string& error,
                    std::vector<std::string>* changedPaths, const std::filesystem::path& historyRoot) {
    changed = 0; error.clear();
    if (changedPaths) changedPaths->clear();
    if (query.empty() || results.truncated || results.indexIncomplete || results.readErrors) { error = "Search is empty or incomplete; replacement was not applied"; return false; }
    // Preflight all files before modifying any, and recheck each one at replacement time.
    for (const auto& file : results.files) {
        std::string current;
        if (!project.readFile(file.path, current, error) || current != file.original) { error = "File changed since review: " + file.path; return false; }
    }
    std::vector<std::vector<std::size_t>> offsets(results.files.size());
    for (const auto& match : results.matches) {
        if (match.file >= results.files.size()) { error = "Invalid reviewed search result"; return false; }
        offsets[match.file].push_back(match.offset);
    }
    for (std::size_t i = 0; i < offsets.size(); ++i) {
        auto& positions = offsets[i]; std::sort(positions.begin(), positions.end());
        const auto& original = results.files[i].original;
        std::size_t end = 0; bool first = true;
        for (const auto pos : positions) {
            if (pos > original.size() || query.size() > original.size() - pos ||
                !sameText(std::string_view(original).substr(pos, query.size()), query, results.caseSensitive) || (!first && pos < end)) {
                error = "Invalid reviewed search result"; return false;
            }
            end = pos + query.size(); first = false;
        }
    }
    const auto replaced = [query, replacement](const std::string& original, const std::vector<std::size_t>& positions, std::string& updated) {
        updated.clear(); std::size_t start = 0;
        const auto firstEnding = original.find_first_of("\r\n");
        const std::string_view ending = firstEnding == std::string::npos || original[firstEnding] == '\n' ? "\n" :
            firstEnding + 1 < original.size() && original[firstEnding + 1] == '\n' ? "\r\n" : "\r";
        std::string normalizedReplacement;
        if (replacement.find_first_of("\r\n") != std::string_view::npos) {
            for (std::size_t i = 0; i < replacement.size(); ++i) {
                if (replacement[i] == '\r' || replacement[i] == '\n') {
                    if (replacement[i] == '\r' && i + 1 < replacement.size() && replacement[i + 1] == '\n') ++i;
                    if (ending.size() > Project::maxFileSize - normalizedReplacement.size()) return false;
                    normalizedReplacement.append(ending);
                } else {
                    if (normalizedReplacement.size() == Project::maxFileSize) return false;
                    normalizedReplacement.push_back(replacement[i]);
                }
            }
        }
        const auto fileReplacement = replacement.find_first_of("\r\n") == std::string_view::npos ? replacement : std::string_view(normalizedReplacement);
        const auto append = [&updated](std::string_view text) {
            if (text.size() > Project::maxFileSize - updated.size()) return false;
            updated.append(text); return true;
        };
        for (const auto pos : positions) {
            if (!append(std::string_view(original).substr(start, pos - start)) || !append(fileReplacement)) return false;
            start = pos + query.size();
        }
        return append(std::string_view(original).substr(start));
    };
    for (std::size_t i = 0; i < results.files.size(); ++i) {
        const auto& file = results.files[i];
        std::string updated;
        if (!replaced(file.original, offsets[i], updated)) {
            error = "No files changed; replacement would make " + file.path + " exceed the 16 MiB file limit"; return false;
        }
        if (std::any_of(updated.begin(), updated.end(), [](unsigned char c) { return c == 0 || (c < 32 && c != '\t' && c != '\n' && c != '\r'); }) ||
            !editor::Document::canEditLosslessly(updated, error)) {
            error = "No files changed; replacement would make " + file.path + " unsafe to edit" + (error.empty() ? "" : ": " + error);
            return false;
        }
    }
    if (!historyRoot.empty()) {
        for (const auto& file : results.files) {
            recovery::Snapshot backup; backup.project = project.root().generic_string(); backup.file = file.path;
            backup.contents = file.original;
            if (!recovery::recordHistory(historyRoot, backup, error)) { error = "No files changed; could not back up " + file.path + ": " + error; return false; }
        }
    }
    for (std::size_t i = 0; i < results.files.size(); ++i) {
        const auto& file = results.files[i];
        std::string updated;
        if (!replaced(file.original, offsets[i], updated)) { error = "Stopped after " + std::to_string(changed) + " files: replacement exceeds the file limit"; return false; }
        if (updated == file.original) continue;
        if (!project.writeFile(file.path, updated, error, &file.original)) { error = "Stopped after " + std::to_string(changed) + " files: " + error; return false; }
        ++changed;
        if (changedPaths) changedPaths->push_back(file.path);
    }
    return true;
}
}
