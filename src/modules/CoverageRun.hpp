#pragma once
#include "modules/ModuleProcess.hpp"
#include "workspace/Project.hpp"
#include <atomic>
#include <filesystem>
#include <string>
#include <vector>
namespace lucid::modules {
enum class ReportRunKind { Coverage, Benchmark };
struct CoverageRunPlan {
    std::filesystem::path root, executable;
    std::string configPath, configSnapshot, reportPath, format, error;
    ReportRunKind kind = ReportRunKind::Coverage;
    std::vector<std::string> arguments;
};
// Reads configuration only. No process is launched until explicit confirmation.
[[nodiscard]] CoverageRunPlan prepareCoverageRun(const workspace::Project& project, std::string_view configPath, ReportRunKind kind = ReportRunKind::Coverage);
// Config is re-read and validated after confirmation. The executable and its
// arguments can run arbitrary code; cancellation cannot undo external changes.
[[nodiscard]] ModuleProcessResult runCoverageGenerator(const workspace::Project& project, const CoverageRunPlan& plan,
                                                        bool confirmed, const std::atomic_bool* cancel=nullptr);
}
