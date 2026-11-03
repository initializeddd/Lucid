#include "workspace/collaboration/Replica.hpp"
#include <algorithm>
#include <limits>
namespace lucid::workspace::collaboration {
namespace {
bool boundary(std::string_view s, std::size_t position) {
    return position == s.size() || (position < s.size() && (static_cast<unsigned char>(s[position]) & 0xc0U) != 0x80U);
}
}
Edit Replica::difference(std::uint64_t revision, std::string_view before, std::string_view after) {
    std::size_t prefix = 0;
    while (prefix < before.size() && prefix < after.size() && before[prefix] == after[prefix]) ++prefix;
    while (prefix > 0 && (!boundary(before, prefix) || !boundary(after, prefix))) --prefix;
    std::size_t suffix = 0;
    while (suffix < before.size() - prefix && suffix < after.size() - prefix && before[before.size() - suffix - 1] == after[after.size() - suffix - 1]) ++suffix;
    while (suffix > 0 && (!boundary(before, before.size() - suffix) || !boundary(after, after.size() - suffix))) --suffix;
    return {revision, prefix, before.size() - prefix - suffix, std::string(after.substr(prefix, after.size() - prefix - suffix))};
}
bool Replica::install(std::uint64_t revision, std::string text, std::string& error) {
    SharedDocument validation(text);
    if (!validation.valid()) { error = "Invalid shared document"; return false; }
    if (m_installed && (m_inFlight || m_local != m_server || m_conflict)) { m_conflict = true; error = "Local edits require review before resynchronizing"; return false; }
    m_server = text; m_local = std::move(text); m_revision = revision; m_installed = true;
    m_inFlight = false; m_conflict = false; m_request.clear(); error.clear(); return true;
}
bool Replica::setLocal(std::string text, std::string& error) {
    if (!m_installed) { error = "No shared file is open"; return false; }
    SharedDocument validation(text); if (!validation.valid()) { error = "Invalid shared document"; return false; }
    m_local = std::move(text); error.clear(); return true;
}
bool Replica::nextEdit(std::string request, Edit& edit, std::string& error) {
    if (!m_installed || m_conflict || m_inFlight) { error = "Shared edits are waiting for synchronization or review"; return false; }
    if (request.empty() || request.size() > 128) { error = "Invalid edit request ID"; return false; }
    if (m_local == m_server) { error = "No local changes"; return false; }
    edit = difference(m_revision, m_server, m_local); m_request = std::move(request); m_inFlight = true; error.clear(); return true;
}
bool Replica::receive(std::uint64_t revision, std::string_view actor, std::string_view self,
                      std::string_view request, const Edit& change, std::string& error) {
    if (!m_installed || m_conflict || actor.empty() || self.empty() || m_revision == std::numeric_limits<std::uint64_t>::max() || revision != m_revision + 1) { m_conflict = true; error = "Shared revision is out of sequence; preserve local edits"; return false; }
    SharedDocument server(m_server);
    if (!server.join("remote", "Remote") || !server.apply("remote", {0, change.start, change.eraseBytes, change.insert}, error)) { m_conflict = true; return false; }
    const std::string nextServer = server.text();
    if (actor == self) {
        if (!m_inFlight || request != m_request) { m_conflict = true; error = "Unexpected edit acknowledgement; preserve local edits"; return false; }
        m_server = nextServer; m_revision = revision; m_inFlight = false; m_request.clear(); error.clear(); return true;
    }
    if (m_local == m_server) { m_local = nextServer; m_server = nextServer; m_revision = revision; error.clear(); return true; }
    SharedDocument rebased(m_server); Edit transformed;
    if (!rebased.join("local", "Local") || !rebased.join("remote", "Remote") ||
        !rebased.apply("local", difference(0, m_server, m_local), error) ||
        !rebased.applyRebased("remote", {0, change.start, change.eraseBytes, change.insert}, transformed, error)) {
        m_conflict = true; error = "Concurrent edits overlap; local text was kept for review"; return false;
    }
    m_local = rebased.text(); m_server = nextServer; m_revision = revision; error.clear(); return true;
}
}
