#pragma once
#include "workspace/recovery/Snapshot.hpp"
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::workspace::session {
inline constexpr std::size_t kMaxSessionFiles = 1024;
struct State {
    std::vector<std::string> recentProjects;
    std::string project;
    std::vector<recovery::FileContext> files;
};
// Keep unavailable contexts alongside currently open files, without replacing
// the newer cursor position of a file that has since been reopened.
[[nodiscard]] bool mergeUnrestored(State& state, std::string_view project,
                                   const std::vector<recovery::FileContext>& files);
[[nodiscard]] bool write(const std::filesystem::path& directory, const State& state, std::string& error);
[[nodiscard]] bool read(const std::filesystem::path& directory, State& state, std::string& error);
}
