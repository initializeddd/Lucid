#include "workspace/collaboration/SharedDocument.hpp"
#include <algorithm>
#include <limits>
namespace lucid::workspace::collaboration {
namespace {
constexpr std::size_t maxBytes = 4U * 1024U * 1024U;
bool utf8(std::string_view s) {
    for (std::size_t i = 0; i < s.size();) {
        const auto a = static_cast<unsigned char>(s[i]);
        if (a <= 0x7f) { ++i; continue; }
        std::size_t count = 0; unsigned int code = 0;
        if (a >= 0xc2 && a <= 0xdf) { count = 2; code = a & 0x1f; }
        else if (a >= 0xe0 && a <= 0xef) { count = 3; code = a & 0x0f; }
        else if (a >= 0xf0 && a <= 0xf4) { count = 4; code = a & 0x07; }
        else return false;
        if (count > s.size() - i) return false;
        for (std::size_t j = 1; j < count; ++j) {
            const auto b = static_cast<unsigned char>(s[i+j]);
            if ((b & 0xc0) != 0x80) return false;
            code = (code << 6) | (b & 0x3f);
        }
        if ((count == 3 && code < 0x800) || (count == 4 && code < 0x10000) ||
            (code >= 0xd800 && code <= 0xdfff) || code > 0x10ffff) return false;
        i += count;
    }
    return true;
}
bool boundary(std::string_view text, std::size_t offset) {
    return offset <= text.size() && (offset == text.size() || (static_cast<unsigned char>(text[offset]) & 0xc0) != 0x80);
}
std::size_t transform(std::size_t position, const Edit& edit) {
    const auto end = edit.start + edit.eraseBytes;
    if (position < edit.start) return position;
    if (position <= end) return edit.start + edit.insert.size();
    return position - edit.eraseBytes + edit.insert.size();
}
}
SharedDocument::SharedDocument(std::string text) : m_text(std::move(text)) {
    m_valid = m_text.size() <= maxBytes && utf8(m_text);
}
bool SharedDocument::join(std::string id, std::string name) {
    if (!m_valid || id.empty() || id.size() > 128 || name.empty() || name.size() > 128 || !utf8(id) || !utf8(name) || m_participants.size() >= 64) return false;
    return m_participants.emplace(std::move(id), Participant{std::move(name), {}}).second;
}
void SharedDocument::leave(std::string_view id) { m_participants.erase(std::string(id)); }
bool SharedDocument::moveCursor(std::string_view id, Cursor cursor) {
    auto found = m_participants.find(std::string(id));
    if (found == m_participants.end() || !boundary(m_text, cursor.anchor) || !boundary(m_text, cursor.caret)) return false;
    found->second.cursor = cursor; return true;
}
bool SharedDocument::apply(std::string_view id, const Edit& edit, std::string& error) {
    const auto user = m_participants.find(std::string(id));
    if (!m_valid || user == m_participants.end()) { error = "Participant is not in this session"; return false; }
    if (edit.revision != m_revision) { error = "Document changed; resynchronize before editing"; return false; }
    if (edit.start > m_text.size() || edit.eraseBytes > m_text.size() - edit.start ||
        !boundary(m_text, edit.start) || !boundary(m_text, edit.start + edit.eraseBytes) || !utf8(edit.insert)) {
        error = "Invalid UTF-8 edit range"; return false;
    }
    if (edit.insert.size() > maxBytes - (m_text.size() - edit.eraseBytes) || m_revision == std::numeric_limits<std::uint64_t>::max()) {
        error = "Document size or revision limit reached"; return false;
    }
    if (edit.eraseBytes == 0 && edit.insert.empty()) { error = "Empty edit"; return false; }
    m_text.replace(edit.start, edit.eraseBytes, edit.insert);
    for (auto& [key, participant] : m_participants) { (void)key; participant.cursor.anchor = transform(participant.cursor.anchor, edit); participant.cursor.caret = transform(participant.cursor.caret, edit); }
    ++m_revision;
    m_history.push_back({m_revision, user->first, user->second.name, edit});
    m_historyBytes += edit.insert.size();
    while (m_history.size() > 1024 || m_historyBytes > maxBytes) {
        m_historyBytes -= m_history.front().edit.insert.size();
        m_historyFloor = m_history.front().revision;
        m_history.erase(m_history.begin());
    }
    error.clear(); return true;
}
bool SharedDocument::applyRebased(std::string_view id, const Edit& original, Edit& applied, std::string& error) {
    applied = original;
    if (original.revision > m_revision) { error = "Edit revision is newer than the host"; return false; }
    if (original.revision < m_historyFloor) { error = "Edit is too old to merge safely; preserve and review the local edit"; return false; }
    if (m_participants.find(std::string(id)) == m_participants.end()) { error = "Participant is not in this session"; return false; }
    if (original.start > maxBytes || original.eraseBytes > maxBytes - original.start || original.insert.size() > maxBytes) { error = "Edit is too large"; return false; }
    for (const auto& change : m_history) {
        if (change.revision <= original.revision) continue;
        const auto& previous = change.edit;
        const auto start = applied.start, end = start + applied.eraseBytes;
        const auto priorStart = previous.start, priorEnd = priorStart + previous.eraseBytes;
        if (previous.eraseBytes == 0) {
            if (priorStart <= start) applied.start += previous.insert.size();
            else if (priorStart < end) { error = "Concurrent edits overlap; preserve and review the local edit"; return false; }
        } else if (priorEnd <= start) {
            applied.start = applied.start - previous.eraseBytes + previous.insert.size();
        } else if (priorStart >= end && applied.eraseBytes != 0) {
            // The earlier edit begins after this range.
        } else if (applied.eraseBytes == 0 && start < priorStart) {
            // An insertion strictly before the earlier replacement is independent.
        } else {
            error = "Concurrent edits overlap; preserve and review the local edit"; return false;
        }
        if (applied.start > maxBytes || applied.eraseBytes > maxBytes - applied.start) { error = "Rebased edit is too large"; return false; }
    }
    applied.revision = m_revision;
    return apply(id, applied, error);
}

}
