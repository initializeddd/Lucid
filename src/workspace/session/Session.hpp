#pragma once
#include "workspace/recovery/Snapshot.hpp"
#include <filesystem>
#include <string>
#include <vector>
namespace lucid::workspace::session {
struct State {
    std::vector<std::string> recentProjects;
    std::string project;
    std::vector<recovery::FileContext> files;
};
[[nodiscard]] bool write(const std::filesystem::path& directory, const State& state, std::string& error);
[[nodiscard]] bool read(const std::filesystem::path& directory, State& state, std::string& error);
}
