#include "modules/PythonEnvironments.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>

namespace lucid::modules {
namespace fs = std::filesystem;
namespace {
constexpr std::size_t maxEntries = 512, maxResults = 512, maxRoots = 256;
bool executable(const fs::path& path) {
    std::error_code ec;
    auto status = fs::status(path, ec);
    if (ec || !fs::is_regular_file(status)) return false;
#ifdef _WIN32
    return true;
#else
    using fs::perms;
    return (status.permissions() & (perms::owner_exec | perms::group_exec | perms::others_exec)) != perms::none;
#endif
}
std::string version(const fs::path& root) {
    std::error_code ec;
    auto config = root / "pyvenv.cfg";
    if (!fs::is_regular_file(config, ec) || ec || fs::file_size(config, ec) > 65536 || ec) return {};
    std::ifstream file(config, std::ios::binary);
    std::string bytes(65537, '\0');
    file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    const auto count = file.gcount();
    if (file.bad() || count < 0 || count > 65536) return {};
    bytes.resize(static_cast<std::size_t>(count));
    std::istringstream input(bytes);
    std::string line;
    while (std::getline(input, line)) {
        const auto equal = line.find('=');
        if (equal == std::string::npos) continue;
        auto key = line.substr(0, equal), value = line.substr(equal + 1);
        auto trim = [](std::string& s) {
            auto first = s.find_first_not_of(" \t\r");
            if (first == std::string::npos) { s.clear(); return; }
            s = s.substr(first, s.find_last_not_of(" \t\r") - first + 1);
        };
        trim(key); trim(value);
        if (key == "version" || key == "version_info") return value.substr(0, 128);
    }
    return {};
}
bool pythonName(const std::string& name) {
#ifdef _WIN32
    return name == "python.exe" || name == "python3.exe";
#else
    if (name == "python" || name == "python3") return true;
    if (name.rfind("python3.", 0) != 0 || name.size() == 8) return false;
    return std::all_of(name.begin() + 8, name.end(), [](unsigned char c) { return c >= '0' && c <= '9'; });
#endif
}
fs::path normalizedAbsolute(const fs::path& path) {
    std::error_code ec; auto p = fs::absolute(path, ec);
    return (ec ? path : p).lexically_normal();
}
template<class F> void entries(const fs::path& dir, F visit) {
    std::error_code ec;
    fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
    for (std::size_t n = 0; !ec && it != end && n < maxEntries; ++n, it.increment(ec)) visit(*it);
}
}
PythonEnvironmentSearch defaultPythonEnvironmentSearch(const fs::path& projectRoot) {
    PythonEnvironmentSearch search; search.projectRoot = projectRoot;
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE"); constexpr char separator = ';';
#else
    const char* home = std::getenv("HOME"); constexpr char separator = ':';
#endif
    if (home && *home) {
        search.environmentRoots.emplace_back(fs::path(home) / ".virtualenvs");
        search.environmentRoots.emplace_back(fs::path(home) / ".conda" / "envs");
    }
    if (const char* path = std::getenv("PATH")) {
        std::string value(path); std::size_t start = 0;
        while (start <= value.size()) {
            auto end = value.find(separator, start);
            auto part = value.substr(start, end == std::string::npos ? end : end - start);
            if (!part.empty()) search.interpreterDirectories.emplace_back(part);
            if (end == std::string::npos) break;
            start = end + 1;
        }
    }
    return search;
}
std::vector<PythonEnvironment> discoverPythonEnvironments(const PythonEnvironmentSearch& search) {
    std::vector<PythonEnvironment> result; std::set<fs::path> seen;
    auto add = [&](const fs::path& interpreter, const fs::path& root, bool local) {
        if (result.size() >= maxResults || !executable(interpreter)) return;
        auto path = normalizedAbsolute(interpreter);
        if (seen.insert(path).second) result.push_back({path, root.empty() ? fs::path{} : normalizedAbsolute(root), root.empty() ? std::string{} : version(root), local});
    };
    auto env = [&](const fs::path& root, bool local) {
        std::error_code ec;
        if (local && (fs::is_symlink(fs::symlink_status(root, ec)) || ec)) return;
        // Require an environment marker rather than treating arbitrary folders as virtualenvs.
        if (!fs::is_regular_file(root / "pyvenv.cfg", ec) && !fs::is_directory(root / "conda-meta", ec)) return;
#ifdef _WIN32
        add(root / "Scripts" / "python.exe", root, local);
        add(root / "python.exe", root, local);
#else
        add(root / "bin" / "python", root, local);
        if (result.size() < maxResults && !executable(root / "bin" / "python")) add(root / "bin" / "python3", root, local);
#endif
    };
    if (!search.projectRoot.empty()) {
        for (const auto* name : {".venv", "venv", "env", ".env"}) env(search.projectRoot / name, true);
        entries(search.projectRoot / ".venvs", [&](const fs::directory_entry& entry) {
            std::error_code ec; if (entry.is_directory(ec) && !entry.is_symlink(ec)) env(entry.path(), true);
        });
    }
    for (std::size_t i=0; i<search.environmentRoots.size() && i<maxRoots && result.size()<maxResults; ++i)
        entries(search.environmentRoots[i], [&](const fs::directory_entry& entry) {
            std::error_code ec; if (entry.is_directory(ec) && !entry.is_symlink(ec)) env(entry.path(), false);
        });
    for (std::size_t i=0; i<search.interpreterDirectories.size() && i<maxRoots && result.size()<maxResults; ++i)
        entries(search.interpreterDirectories[i], [&](const fs::directory_entry& entry) {
            try { const auto name=entry.path().filename().u8string(); if (pythonName(std::string(name.begin(),name.end()))) add(entry.path(), {}, false); }
            catch (const std::exception&) { /* Ignore names that cannot be represented as UTF-8. */ }
        });
    return result;
}
}

namespace lucid::modules {
bool validatePythonInterpreter(const fs::path& interpreter, PythonProcessRunner runner,
                               std::string& detectedVersion, std::string& error) {
    detectedVersion.clear(); error.clear();
    if (!runner || !interpreter.is_absolute() || !executable(interpreter)) {
        error = "Select an absolute path to an executable Python interpreter."; return false;
    }
    std::string output;
    if (!runner(interpreter, {"-I", "-c", "import sys; print('LUCID_PYTHON_VERSION=' + '.'.join(map(str, sys.version_info[:3])))"}, output, error)) {
        if (error.empty()) error = "Could not validate the interpreter.";
        return false;
    }
    constexpr auto prefix = "LUCID_PYTHON_VERSION=";
    if (output.rfind(prefix, 0) != 0 || output.size() > 128) { error = "Interpreter returned an invalid version."; return false; }
    auto value = output.substr(std::char_traits<char>::length(prefix));
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) value.pop_back();
    int dots = 0; bool digit = false;
    for (const char c : value) {
        if (c >= '0' && c <= '9') digit = true;
        else if (c == '.' && digit) { ++dots; digit = false; }
        else { error = "Interpreter returned an invalid version."; return false; }
    }
    if (!digit || dots != 2) { error = "Interpreter returned an invalid version."; return false; }
    const auto major = value.substr(0, value.find('.'));
    if (major.size() > 3 || (major.size() == 1 && major[0] < '3') || major[0] == '0') {
        error = "Python 3 or newer is required."; return false;
    }
    detectedVersion = value; return true;
}
bool createPythonEnvironment(const fs::path& projectRoot, const fs::path& interpreter,
                             const fs::path& destination, PythonProcessRunner runner, std::string& error) {
    error.clear();
    if (!runner || !projectRoot.is_absolute() || !destination.is_absolute() || !interpreter.is_absolute() || !executable(interpreter)) {
        error = "Select an absolute interpreter and project destination."; return false;
    }
    std::error_code ec;
    const auto root = fs::canonical(projectRoot, ec);
    if (ec || !fs::is_directory(root, ec) || ec) { error = "Project directory is unavailable."; return false; }
    const auto target = destination.lexically_normal();
    if (target.filename().empty() || target.filename() == "." || target.filename() == "..") { error = "Invalid environment destination."; return false; }
    const auto parent = fs::canonical(target.parent_path(), ec);
    if (ec) { error = "Destination parent does not exist."; return false; }
    auto relative = parent.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") { error = "Destination must be inside the project."; return false; }
    const auto finalPath = parent / target.filename();
    auto status = fs::symlink_status(finalPath, ec);
    if (ec && ec != std::errc::no_such_file_or_directory) { error = "Cannot inspect destination."; return false; }
    if (!ec && fs::exists(status)) { error = "Destination already exists."; return false; }
    // A dangling symlink also counts as an existing destination.
    if (!ec && fs::is_symlink(status)) { error = "Destination already exists."; return false; }
    // Reserve the destination so an existing environment is never intentionally reused.
    ec.clear();
    if (!fs::create_directory(finalPath, ec)) { error = "Could not reserve destination."; return false; }
    // The runner receives UTF-8 arguments on every platform.
    auto utf8 = finalPath.u8string();
    std::string targetArgument(utf8.begin(), utf8.end());
    std::string output;
    if (!runner(interpreter, {"-I", "-m", "venv", "--without-pip", targetArgument}, output, error)) {
        if (error.empty()) error = "Could not create the environment.";
        return false;
    }
    // Exit status alone does not prove an environment was created.
    ec.clear();
    if (!fs::is_regular_file(finalPath / "pyvenv.cfg", ec) || ec) { error = "Creation did not produce a virtual environment."; return false; }
#ifdef _WIN32
    const auto createdInterpreter = finalPath / "Scripts" / "python.exe";
#else
    const auto createdInterpreter = finalPath / "bin" / "python";
#endif
    if (!executable(createdInterpreter)) { error = "Created environment has no executable interpreter."; return false; }
    return true;
}
}
