#include "modules/ReleaseNotes.hpp"
int main() {
    using namespace lucid::modules;
    std::vector<ReleaseNoteEntry> entries{{"Fixed crash", "when loading", "#12", "fix", true}, {"New view", "", "abc", "feat", true}, {"Private", "", "", "feat", false}, {"General", "", "", "unknown", true}, {"", "", "", "fix", true}};
    auto r = buildReleaseNotes("1.2", "2026-01-01", entries);
    if (r.included != 3 || r.omitted != 1 || r.incomplete || r.markdown.find("Private") != std::string::npos || r.markdown.find("## Features") > r.markdown.find("## Fixes") || r.markdown.find("\\#12") == std::string::npos || r.markdown.find("## Other") == std::string::npos) return 1;
    r = buildReleaseNotes("", "", entries, 1); if (!r.incomplete || r.included != 1 || r.omitted != 3) return 2;
    r = buildReleaseNotes("", "", entries, 100, 10); if (!r.incomplete || !r.markdown.empty() || r.omitted != 4) return 3;
    r = buildReleaseNotes("", "", entries, 100, 30); if (!r.incomplete || r.markdown.size() > 30) return 4;
    entries = {{std::string(4095, 'a') + "β", "", "", "fix", true}, {" \t\n", "", "", "fix", true}, {"~~strike~~", "", "", "fix", true}};
    r = buildReleaseNotes(std::string(4095, 'x') + "β", "", entries);
    if (!r.incomplete || r.included != 2 || r.omitted != 1 || r.markdown.find("β") != std::string::npos ||
        r.markdown.find("\\~\\~strike\\~\\~") == std::string::npos) return 5;
    return 0;
}
