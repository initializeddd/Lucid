#pragma once
#include "workspace/collaboration/SharedDocument.hpp"
#include <cstdint>
#include <string>
#include <string_view>
namespace lucid::workspace::collaboration {
// Client-side state. A conflict never replaces local text; the caller must
// explicitly preserve and resolve it before installing another snapshot.
class Replica {
public:
    // Use a fresh replica for a different file or a newly created host session.
    [[nodiscard]] bool install(std::uint64_t revision, std::string text, std::string& error);
    [[nodiscard]] bool setLocal(std::string text, std::string& error);
    [[nodiscard]] bool nextEdit(std::string request, Edit& edit, std::string& error);
    [[nodiscard]] bool receive(std::uint64_t revision, std::string_view actor,
                               std::string_view self, std::string_view request,
                               const Edit& change, std::string& error);
    void reject() noexcept { m_conflict = true; }
    [[nodiscard]] const std::string& localText() const noexcept { return m_local; }
    [[nodiscard]] const std::string& serverText() const noexcept { return m_server; }
    [[nodiscard]] std::uint64_t revision() const noexcept { return m_revision; }
    [[nodiscard]] bool inFlight() const noexcept { return m_inFlight; }
    [[nodiscard]] bool conflicted() const noexcept { return m_conflict; }
private:
    static Edit difference(std::uint64_t revision, std::string_view before, std::string_view after);
    std::string m_server, m_local, m_request, m_sentText;
    std::uint64_t m_revision = 0;
    bool m_installed = false, m_inFlight = false, m_conflict = false;
};
}
