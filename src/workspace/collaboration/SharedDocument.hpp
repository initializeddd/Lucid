#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::workspace::collaboration {
// UTF-8 byte offsets are used on the wire. The host is authoritative; stale
// operations are rejected so a client must resynchronize before retrying.
struct Cursor { std::size_t anchor = 0; std::size_t caret = 0; };
struct Participant { std::string name; Cursor cursor; };
struct Edit { std::uint64_t revision = 0; std::size_t start = 0; std::size_t eraseBytes = 0; std::string insert; };
struct Change { std::uint64_t revision = 0; std::string participantId; std::string participantName; Edit edit; };
class SharedDocument {
public:
    explicit SharedDocument(std::string text = {});
    [[nodiscard]] bool valid() const noexcept { return m_valid; }
    [[nodiscard]] bool join(std::string id, std::string name);
    void leave(std::string_view id);
    [[nodiscard]] bool moveCursor(std::string_view id, Cursor cursor);
    [[nodiscard]] bool apply(std::string_view id, const Edit& edit, std::string& error);
    // Rebase non-overlapping stale edits; reject ambiguous overlaps without changing the document.
    [[nodiscard]] bool applyRebased(std::string_view id, const Edit& edit, Edit& applied, std::string& error);
    [[nodiscard]] const std::string& text() const noexcept { return m_text; }
    [[nodiscard]] std::uint64_t revision() const noexcept { return m_revision; }
    [[nodiscard]] const std::map<std::string, Participant>& participants() const noexcept { return m_participants; }
    [[nodiscard]] const std::vector<Change>& history() const noexcept { return m_history; }
private:
    std::string m_text;
    std::uint64_t m_revision = 0;
    bool m_valid = false;
    std::map<std::string, Participant> m_participants;
    std::vector<Change> m_history;
    std::size_t m_historyBytes = 0;
    std::uint64_t m_historyFloor = 0;
};
}
