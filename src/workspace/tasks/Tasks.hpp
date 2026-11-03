#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::workspace {
class Project;
struct TaskActivity {
    std::string actor;
    std::string action;
    std::string subject;
    std::string time;
};
struct Subtask {
    std::string id;
    std::string title;
    std::string createdBy;
    bool done = false;
    std::string doneBy;
};
struct TaskFileContext {
    std::string path;
    std::size_t line = 0;
    std::size_t column = 0;
    int firstLine = 0;
};
struct Task {
    std::string id;
    std::string title;
    int tier = 1;
    std::string createdBy;
    std::string assignedTo;
    std::vector<std::string> workingBy;
    bool done = false;
    std::string doneBy;
    std::string solution;
    std::string debugFunction;
    std::string debugSource;
    std::vector<int> debugBreakpoints;
    std::vector<Subtask> subtasks;
    std::vector<TaskActivity> history;
    std::vector<TaskFileContext> files;
};
struct TaskBoard {
    std::vector<Task> tasks;
    std::string sourceContents;
    bool sourceExists = false;
};
[[nodiscard]] bool loadTasks(const Project& project, TaskBoard& board, std::string& error);
[[nodiscard]] bool saveTasks(const Project& project, TaskBoard& board, std::string& error);
[[nodiscard]] std::string newTaskId();
[[nodiscard]] Task* findTask(TaskBoard& board, std::string_view id);
[[nodiscard]] bool createTask(TaskBoard& board, std::string_view title, int tier,
                              std::string_view actor, std::string& id);
[[nodiscard]] bool addSubtask(Task& task, std::string_view title, std::string_view actor);
[[nodiscard]] bool setTaskDone(Task& task, bool done, std::string_view actor);
[[nodiscard]] bool setSubtaskDone(Task& task, std::string_view id, bool done, std::string_view actor);
[[nodiscard]] bool setTaskAssignee(Task& task, std::string_view assignee, std::string_view actor);
[[nodiscard]] bool setTaskSolution(Task& task, std::string_view solution, std::string_view actor);
[[nodiscard]] bool setTaskDebugFunction(Task& task, std::string_view function, std::string_view actor);
[[nodiscard]] bool setTaskWorking(Task& task, bool working, std::string_view actor);
void recordTaskActivity(Task& task, std::string_view actor, std::string_view action,
                        std::string_view subject = {});
}
