#include "modules/WebPreviewConfig.hpp"
#include <algorithm>
#include <set>
#include <system_error>
namespace lucid::modules {
namespace fs = std::filesystem;
namespace {
int hex(char c) { if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return c - 'a' + 10; if (c >= 'A' && c <= 'F') return c - 'A' + 10; return -1; }
bool decode(std::string_view input, std::string& out) {
    out.clear();
    for (std::size_t i = 0; i < input.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        if (c == '%') {
            if (i + 2 >= input.size() || hex(input[i + 1]) < 0 || hex(input[i + 2]) < 0) return false;
            c = static_cast<unsigned char>(hex(input[i + 1]) * 16 + hex(input[i + 2])); i += 2;
        }
        if (c < 0x20 || c == 0x7f || c == '\\') return false;
        out += static_cast<char>(c);
    }
    return true;
}
bool safeHtmlPath(const fs::path& root, const fs::path& path, std::string& error) {
    if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory()) { error = "HTML path must be project-relative."; return false; }
    for (const auto& part : path) if (part == "." || part == ".." || part.empty()) { error = "HTML path contains traversal."; return false; }
    const auto raw = path.generic_u8string();
    for (auto c : raw) if (c == ':' || c == '\\' || c == '?' || c == '#' || c == '%' || c < 0x20 || c == 0x7f) { error = "Invalid HTML path."; return false; }
    auto ext = path.extension().string(); std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; });
    if (ext != ".html" && ext != ".htm") { error = "Entry must be an HTML file."; return false; }
    std::error_code ec;
    const auto absolute = (root / path).lexically_normal(); const auto canonical = fs::canonical(absolute, ec);
    if (ec || absolute != canonical || !fs::is_regular_file(canonical, ec) || ec) { error = "HTML file must exist inside the project without symlink components."; return false; }
    auto relative = canonical.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") { error = "HTML path is outside the project."; return false; }
    return true;
}
}
bool validateWebPreviewConfig(const fs::path& projectRoot, const WebPreviewConfig& config, std::string& error) {
    error.clear(); std::error_code ec; auto root = fs::canonical(projectRoot, ec);
    if (ec || !fs::is_directory(root, ec) || ec) { error = "Project directory is unavailable."; return false; }
    if (config.width < 100 || config.width > 4096 || config.height < 100 || config.height > 4096 || config.width * config.height > 8U * 1024U * 1024U) { error = "Invalid preview viewport size."; return false; }
    if (!safeHtmlPath(root, config.entry, error)) return false;
    if (config.states.size() > 32) { error = "Too many preview states."; return false; }
    std::set<std::string> names;
    for (const auto& state : config.states) {
        if (state.name.empty() || state.name.size() > 128 || state.url.empty() || state.url.size() > 4096 || !names.insert(state.name).second) { error = "Invalid or duplicate preview state."; return false; }
        for (unsigned char c : state.name) if (c < 0x20 || c == 0x7f) { error = "Invalid preview state name."; return false; }
        auto separator = state.url.find_first_of("?#");
        auto rawPath = std::string_view(state.url).substr(0, separator);
        std::string decodedPath, decodedUrl;
        if (!decode(state.url, decodedUrl) || !decode(rawPath, decodedPath)) { error = "Invalid preview state URL encoding."; return false; }
        // Encoded delimiters, percent escapes and separators in paths can be interpreted
        // differently by servers or browsers; reject them instead of normalizing twice.
        if (rawPath.find('%') != std::string_view::npos || decodedPath.find(':') != std::string::npos || decodedPath.starts_with('/') || decodedPath.starts_with("//")) { error = "Preview state URL must use a local relative HTML path."; return false; }
        if (!decodedPath.empty() && !safeHtmlPath(root, fs::path(std::u8string(decodedPath.begin(), decodedPath.end())), error)) return false;
    }
    return true;
}
}
