#include "workspace/tasks/Tasks.hpp"
#include "workspace/Project.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
using namespace lucid::workspace;
int main() {
    const auto directory = std::filesystem::temp_directory_path() /
        ("lucid-task-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    bool created = false; std::string error;
    auto project = Project::openOrCreate(directory, created, error);
    if (!error.empty()) return 1;
    TaskBoard board; if (!loadTasks(project, board, error) || !board.tasks.empty()) return 2;
    std::string id;
    if (!createTask(board, "Fix \"crash\" ✓", 3, "Alice", id)) return 3;
    Task* task = findTask(board, id); if (!task) return 4;
    if (!addSubtask(*task, "Check input", "Bob") || !setTaskAssignee(*task, "Bob", "Alice") ||
        !setTaskWorking(*task, true, "Alice") || !setTaskWorking(*task, true, "Bob")) return 5;
    const auto subId = task->subtasks.front().id;
    if (!setSubtaskDone(*task, subId, true, "Bob") || !setTaskDone(*task, true, "Alice")) return 6;
    task->files.push_back({"src/main.cpp", 3, 4, 2});
    if (!saveTasks(project, board, error)) return 7;
    TaskBoard restored; if (!loadTasks(project, restored, error) || restored.tasks.size() != 1) return 8;
    const auto& saved=restored.tasks.front();
    if (saved.title != "Fix \"crash\" ✓" || saved.tier != 3 || saved.createdBy != "Alice" ||
        saved.assignedTo != "Bob" || saved.workingBy.size() != 2 || saved.doneBy != "Alice" ||
        saved.subtasks.front().doneBy != "Bob" || saved.history.size() != 7 || saved.files.front().column != 4) return 9;
    if (!setTaskDone(restored.tasks.front(), false, "Bob") || !restored.tasks.front().doneBy.empty() ||
        restored.tasks.front().history.back().actor != "Bob") return 10;
    restored.tasks.front().debugSource = "src/main.cpp";
    restored.tasks.front().debugBreakpoints = {3, 7};
    if (!setTaskSolution(restored.tasks.front(), "Fixed validation", "Alice") ||
        !setTaskDebugFunction(restored.tasks.front(), "main", "Bob") ||
        !saveTasks(project, restored, error)) return 12;
    TaskBoard stale; if (!loadTasks(project, stale, error)) return 13;
    TaskBoard current = stale;
    if (!setTaskSolution(current.tasks.front(), "Added regression test", "Alice") ||
        !saveTasks(project, current, error)) return 14;
    if (!setTaskSolution(stale.tasks.front(), "Stale edit", "Bob") ||
        saveTasks(project, stale, error) || error.empty()) return 15;
    TaskBoard final; if (!loadTasks(project, final, error) ||
        final.tasks.front().solution != "Added regression test" ||
        final.tasks.front().debugFunction != "main" || final.tasks.front().debugSource != "src/main.cpp" ||
        final.tasks.front().debugBreakpoints != std::vector<int>{3, 7}) return 16;
    { std::ofstream bad(directory / ".lucid/tasks.json", std::ios::trunc); bad << "invalid"; }
    if (loadTasks(project, restored, error) || error.empty()) return 11;
    std::filesystem::remove_all(directory);
    return 0;
}
