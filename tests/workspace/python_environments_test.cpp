#include "modules/PythonEnvironments.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace {
int calls = 0;
bool emptyRunner(const std::filesystem::path&, const std::vector<std::string>&, std::string&, std::string&) { return true; }
bool badVersionRunner(const std::filesystem::path&, const std::vector<std::string>&, std::string& output, std::string&) { output = "LUCID_PYTHON_VERSION=2.7.18\n"; return true; }
bool runner(const std::filesystem::path&, const std::vector<std::string>& args, std::string& output, std::string&) {
    ++calls;
    if (args.size() >= 2 && args[1] == "-c") output = "LUCID_PYTHON_VERSION=3.12.1\n";
    else if (args.size() == 5 && args[1] == "-m") {
        auto target = std::filesystem::path(std::u8string(args.back().begin(), args.back().end()));
#ifdef _WIN32
        auto exe = target / "Scripts/python.exe";
#else
        auto exe = target / "bin/python";
#endif
        std::filesystem::create_directories(exe.parent_path());
        std::ofstream(exe) << "fake interpreter";
        std::filesystem::permissions(exe, std::filesystem::perms::owner_all, std::filesystem::perm_options::add);
        std::ofstream(target / "pyvenv.cfg") << "version=3.12.1";
    }
    return true;
}
}

int main() {
    namespace fs = std::filesystem;
    using namespace lucid::modules;
    const auto root = fs::temp_directory_path() / ("lucid-python-env-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto project = root / "project", envs = root / "envs", bin = root / "bin";
#ifdef _WIN32
    const auto python = fs::path("Scripts/python.exe"), standalone = fs::path("python.exe");
#else
    const auto python = fs::path("bin/python"), standalone = fs::path("python3.12");
#endif
    auto make = [](const fs::path& path, const char* contents) {
        fs::create_directories(path.parent_path()); std::ofstream(path) << contents;
        fs::permissions(path, fs::perms::owner_all, fs::perm_options::add);
    };
    make(project / ".venv" / "pyvenv.cfg", "version = 3.12.1\n");
    make(project / ".venv" / python, "never execute");
    make(envs / "other" / "pyvenv.cfg", "version_info = 3.11.4\n");
    make(envs / "other" / python, "never execute");
    make(envs / "unmarked" / python, "not an environment");
    make(bin / standalone, "never execute");
    make(bin / "python3.bad", "not a python name");
    PythonEnvironmentSearch search{project, {envs, envs}, {bin, bin}};
    auto found = discoverPythonEnvironments(search);
    bool ok = found.size() == 3 && found[0].projectLocal && found[0].version == "3.12.1" &&
        found[1].version == "3.11.4" && !found[1].projectLocal && found[2].environmentRoot.empty() &&
        found[2].interpreter.filename() == standalone;
    std::string detected, error;
    auto selected = fs::absolute(bin / standalone);
    ok = ok && validatePythonInterpreter(selected, runner, detected, error) && detected == "3.12.1" && calls == 1;
    ok = ok && !validatePythonInterpreter(selected, badVersionRunner, detected, error) && detected.empty();
    ok = ok && !createPythonEnvironment(project, selected, project / "empty", emptyRunner, error) && fs::is_directory(project / "empty");
    ok = ok && !createPythonEnvironment(project, selected, root / "outside", runner, error) && calls == 1;
    ok = ok && !createPythonEnvironment(project, selected, project / ".venv", runner, error) && calls == 1;
    ok = ok && createPythonEnvironment(project, selected, project / "created", runner, error) && calls == 2 && fs::is_directory(project / "created");
    ok = ok && !createPythonEnvironment(project, selected, project / "created", runner, error) && calls == 2;
    std::error_code ec; fs::create_directories(root / "project-link-test");
    fs::create_directory_symlink(envs / "other", root / "project-link-test" / ".venv", ec);
    if (!ec) { PythonEnvironmentSearch linked{root / "project-link-test", {}, {}}; ok = ok && discoverPythonEnvironments(linked).empty(); }
    fs::remove_all(root);
    return ok ? 0 : 1;
}
