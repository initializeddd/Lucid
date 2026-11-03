#pragma once
#include "workspace/Project.hpp"
#include "workspace/collaboration/SharedDocument.hpp"
#include <map>
#include <memory>
#include <string>
#include <string_view>
namespace lucid::workspace::collaboration {
// Authoritative project state. The caller must serialize access from the host's event loop.
class SharedProject {
public:
    explicit SharedProject(Project project) : m_project(std::move(project)) {}
    [[nodiscard]] bool join(std::string id, std::string name, std::string& error);
    void leave(std::string_view id);
    [[nodiscard]] bool open(std::string_view id, std::string_view path, std::string& canonicalPath, std::string& error);
    [[nodiscard]] const SharedDocument* document(std::string_view path) const;
    [[nodiscard]] bool moveCursor(std::string_view id, Cursor cursor, std::string& error);
    [[nodiscard]] bool edit(std::string_view id, const Edit& edit, Edit& applied, std::string& error);
    [[nodiscard]] bool save(std::string_view path, std::string& error);
private:
    struct User { std::string name, file; };
    struct File { std::unique_ptr<SharedDocument> document; std::string baseline; };
    Project m_project;
    std::map<std::string, User> m_users;
    std::map<std::string, File> m_files;
    std::size_t m_liveBytes = 0;
};
}
