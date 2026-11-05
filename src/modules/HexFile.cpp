#include "modules/HexFile.hpp"
namespace lucid::modules {
namespace {
bool safePath(const workspace::Project& project, std::string_view name, std::string& error) {
    std::filesystem::path relative;
    try { relative = std::filesystem::path(std::u8string(name.begin(), name.end())); }
    catch (const std::exception&) { error = "Invalid project path"; return false; }
    if (name.empty() || name.find('\0') != std::string_view::npos || relative.is_absolute() || relative.has_root_name()) { error = "Invalid project path"; return false; }
    auto path = project.root();
    for (const auto& component : relative) {
        if (component == ".." || component == "." || component.empty()) { error = "Invalid project path"; return false; }
        path /= component; std::error_code ec; auto status = std::filesystem::symlink_status(path, ec);
        if (ec || !std::filesystem::exists(status) || std::filesystem::is_symlink(status)) { error = "File path is missing or contains a symbolic link"; return false; }
    }
    std::string resolveError; (void)project.resolve(name, resolveError);
    if (!resolveError.empty()) { error = resolveError; return false; }
    return true;
}
}
bool HexFile::open(const workspace::Project& project, std::string_view path, std::string& error, const std::atomic_bool* cancel) {
    error.clear(); if (cancel && cancel->load()) { error = "Open cancelled"; return false; }
    if (!safePath(project, path, error)) return false;
    std::string content; if (!project.readFile(path, content, error, cancel)) return false;
    if (!safePath(project, path, error)) return false;
    if (!buffer_.load(content)) { error = "File is too large"; return false; }
    path_ = std::string(path); root_ = project.root(); original_ = std::move(content); opened_ = true; return true;
}
bool HexFile::save(workspace::Project& project, std::string& error, const std::atomic_bool* cancel) {
    error.clear(); if (!opened_ || root_ != project.root()) { error = "No file is open for this project"; return false; }
    if (cancel && cancel->load()) { error = "Save cancelled"; return false; }
    if (!safePath(project, path_, error)) return false;
    if (cancel && cancel->load()) { error = "Save cancelled"; return false; }
    if (!project.writeFile(path_, buffer_.bytes(), error, &original_, false, cancel)) return false;
    original_ = buffer_.bytes(); return true;
}
}
