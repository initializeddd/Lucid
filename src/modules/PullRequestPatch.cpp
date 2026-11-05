#include "modules/PullRequestPatch.hpp"
#include <charconv>
#include <limits>

namespace lucid::modules {
namespace {
constexpr std::size_t maxBytes = 2U * 1024U * 1024U, maxLines = 50000, maxHunks = 5000;
bool number(std::string_view text, std::size_t& pos, std::size_t& value) {
    const auto begin = pos;
    while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') ++pos;
    if (begin == pos) return false;
    const auto parsed = std::from_chars(text.data() + begin, text.data() + pos, value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + pos;
}
bool range(std::string_view text, std::size_t& pos, char sign, std::size_t& start, std::size_t& count) {
    if (pos >= text.size() || text[pos++] != sign || !number(text, pos, start)) return false;
    count = 1;
    if (pos < text.size() && text[pos] == ',') { ++pos; if (!number(text, pos, count)) return false; }
    return (start != 0 || count == 0) && count <= std::numeric_limits<std::size_t>::max() - start;
}
}
bool parsePullRequestPatch(std::string_view text, std::vector<PatchHunk>& hunks, std::string& error) {
    hunks.clear(); error.clear();
    if (text.size() > maxBytes) { error = "Patch exceeds size limit"; return false; }
    std::vector<PatchHunk> result; std::size_t oldLine = 0, newLine = 0, oldLeft = 0, newLeft = 0, lines = 0;
    bool active = false, canAnnotate = false;
    for (std::size_t pos = 0; pos < text.size();) {
        const auto end = text.find('\n', pos); auto line = text.substr(pos, end == std::string_view::npos ? text.size() - pos : end - pos);
        pos = end == std::string_view::npos ? text.size() : end + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (++lines > maxLines) { error = "Patch exceeds line limit"; return false; }
        if (line.starts_with("@@ ")) {
            if ((active && (oldLeft || newLeft)) || result.size() >= maxHunks) { error = "Incomplete patch hunk or too many hunks"; return false; }
            std::size_t index = 3;
            if (!range(line, index, '-', oldLine, oldLeft) || index >= line.size() || line[index++] != ' ' ||
                !range(line, index, '+', newLine, newLeft) || line.substr(index, 3) != " @@") { error = "Invalid patch hunk header"; return false; }
            result.push_back({std::string(line), {}}); active = true; canAnnotate = false; continue;
        }
        if (!active || line.empty()) { error = "Invalid patch line"; return false; }
        if (line == "\\ No newline at end of file") {
            if (!canAnnotate) { error = "Unexpected no-newline marker"; return false; }
            result.back().lines.push_back({PatchLineKind::NoNewline, std::string(line), 0, 0}); canAnnotate = false; continue;
        }
        PatchLine entry; entry.text = std::string(line.substr(1));
        if (line.front() == ' ') {
            if (!oldLeft || !newLeft) { error = "Patch hunk contains too many lines"; return false; }
            entry.kind = PatchLineKind::Context; entry.oldLine = oldLine++; entry.newLine = newLine++; --oldLeft; --newLeft;
        } else if (line.front() == '-') {
            if (!oldLeft) { error = "Patch hunk contains too many old lines"; return false; }
            entry.kind = PatchLineKind::Removed; entry.oldLine = oldLine++; --oldLeft;
        } else if (line.front() == '+') {
            if (!newLeft) { error = "Patch hunk contains too many new lines"; return false; }
            entry.kind = PatchLineKind::Added; entry.newLine = newLine++; --newLeft;
        } else { error = "Unsupported patch line"; return false; }
        result.back().lines.push_back(std::move(entry)); canAnnotate = true;
    }
    if (!active || oldLeft || newLeft) { error = "Empty or incomplete patch"; return false; }
    hunks = std::move(result); return true;
}
}
