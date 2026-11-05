#include "modules/CMakeModel.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("lucid-cmake-model-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto source = root / "source", build = root / "build";
    fs::create_directories(source / "include"); fs::create_directories(build / ".cmake/api/v1/query");
    { std::ofstream f(build / ".cmake/api/v1/query/codemodel-v2"); }
    { std::ofstream f(build / ".cmake/api/v1/query/cache-v2"); }
    { std::ofstream f(source / "main.cpp"); f << "int main() { return 0; }\n"; }
    { std::ofstream f(source / "CMakeLists.txt"); f << "cmake_minimum_required(VERSION 3.16)\nproject(Demo LANGUAGES CXX)\noption(LUCID_FEATURE \"Example feature\" ON)\nadd_executable(demo main.cpp)\ntarget_include_directories(demo PRIVATE include)\ntarget_compile_definitions(demo PRIVATE LUCID_TEST=1)\n"; }
#if defined(_WIN32)
    const std::string cmd = "cmake -S \"" + source.string() + "\" -B \"" + build.string() + "\" >NUL 2>&1";
#else
    const std::string cmd = "cmake -S '" + source.string() + "' -B '" + build.string() + "' >/dev/null 2>&1";
#endif
    if (std::system(cmd.c_str()) != 0) return 1;
    lucid::modules::CMakeModel model; std::string error;
    if (!lucid::modules::readCMakeModel(build, model, error)) return 2;
    const auto option = std::find_if(model.options.begin(), model.options.end(), [](const auto& item) { return item.name == "LUCID_FEATURE"; });
    if (!model.optionsAvailable || option == model.options.end() || option->value != "ON" || option->description != "Example feature") return 7;
    const auto target = std::find_if(model.targets.begin(), model.targets.end(), [](const auto& t) { return t.name == "demo"; });
    if (target == model.targets.end() || target->type != "EXECUTABLE" || target->sources.empty() || target->groups.empty()) return 3;
    const auto main = std::find_if(target->sources.begin(), target->sources.end(), [](const auto& s) { return s.path == "main.cpp"; });
    if (main == target->sources.end() || main->compileGroup < 0) return 4;
    const auto& group = target->groups[static_cast<std::size_t>(main->compileGroup)];
    if (group.language != "CXX" || std::find(group.definitions.begin(), group.definitions.end(), "LUCID_TEST=1") == group.definitions.end() ||
        std::none_of(group.includes.begin(), group.includes.end(), [](const auto& path) { return path.ends_with("/include"); })) return 5;
    const auto bad = root / "bad"; fs::create_directories(bad / ".cmake/api/v1/reply");
    { std::ofstream f(bad / ".cmake/api/v1/reply/index-1.json"); f << R"({"objects":[{"kind":"codemodel","version":{"major":2},"jsonFile":"../escape.json"}]})"; }
    if (lucid::modules::readCMakeModel(bad, model, error) || error.find("Unsafe") == std::string::npos || !model.targets.empty()) return 6;
    fs::remove_all(root); return 0;
}
