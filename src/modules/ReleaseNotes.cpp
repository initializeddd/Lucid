#include "modules/ReleaseNotes.hpp"
#include <algorithm>
#include <array>
#include <cctype>
namespace lucid::modules {
namespace {
std::string singleLine(std::string_view in) {
    std::string out; out.reserve(std::min<std::size_t>(in.size(), 4096));
    std::size_t length = std::min<std::size_t>(in.size(), 4096);
    // Never cut a UTF-8 sequence at the byte limit.
    if (length < in.size()) while (length && (static_cast<unsigned char>(in[length]) & 0xc0U) == 0x80U) --length;
    for (char c : in.substr(0, length)) {
        if (c == '\n' || c == '\r' || c == '\t') out.push_back(' ');
        else if (static_cast<unsigned char>(c) >= 32 && c != 127) out.push_back(c);
    }
    return out;
}
std::string escape(std::string_view in) {
    std::string out; for (char c : singleLine(in)) { if (c == '\\' || c == '*' || c == '_' || c == '[' || c == ']' || c == '<' || c == '>' || c == '`' || c == '#' || c == '~') out.push_back('\\'); out.push_back(c); } return out;
}
std::size_t category(std::string_view in) {
    std::string lower; for (char c : in.substr(0, 100)) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (lower == "feature" || lower == "features" || lower == "feat") return 0;
    if (lower == "fix" || lower == "fixes" || lower == "bug" || lower == "bugfix") return 1;
    if (lower == "change" || lower == "changes" || lower == "improvement") return 2;
    return 3;
}
}
ReleaseNoteDraft buildReleaseNotes(std::string_view version, std::string_view date, const std::vector<ReleaseNoteEntry>& entries, std::size_t maxEntries, std::size_t maxBytes) {
    ReleaseNoteDraft out; std::array<std::vector<std::string>, 4> groups;
    if (version.size() > 4096 || date.size() > 4096) out.incomplete = true;
    const auto heading = std::string("# Release ") + (version.empty() ? "Draft" : escape(version)) + (date.empty() ? "" : " — " + escape(date)) + "\n";
    if (heading.size() > maxBytes) { out.incomplete = true; for (const auto& e : entries) if (e.selected) ++out.omitted; return out; }
    std::size_t used = heading.size(); constexpr std::array<std::string_view, 4> names{"Features", "Fixes", "Changes", "Other"};
    for (const auto& entry : entries) {
        if (!entry.selected) continue;
        if (out.included >= maxEntries || singleLine(entry.title).find_first_not_of(' ') == std::string::npos) { ++out.omitted; if (out.included >= maxEntries) out.incomplete = true; continue; }
        auto group = category(entry.category);
        std::string line = "- " + escape(entry.title);
        if (!entry.description.empty()) line += " — " + escape(entry.description);
        if (!entry.reference.empty()) line += " (" + escape(entry.reference) + ")";
        line += '\n';
        auto extra = line.size() + (groups[group].empty() ? names[group].size() + 5 : 0);
        if (extra > maxBytes - used) { ++out.omitted; out.incomplete = true; continue; }
        used += extra; groups[group].push_back(std::move(line)); ++out.included;
        if (entry.title.size() > 4096 || entry.description.size() > 4096 || entry.reference.size() > 4096) out.incomplete = true;
    }
    out.markdown = heading;
    for (std::size_t i = 0; i < groups.size(); ++i) if (!groups[i].empty()) { out.markdown += "\n## "; out.markdown += names[i]; out.markdown += '\n'; for (const auto& line : groups[i]) out.markdown += line; }
    return out;
}
}
