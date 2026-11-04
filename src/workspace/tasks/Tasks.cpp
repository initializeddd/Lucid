#include "workspace/tasks/Tasks.hpp"
#include "workspace/Project.hpp"
#include "editor/json/Json.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <system_error>
#if defined(_WIN32)
#include <windows.h>
#endif

namespace lucid::workspace {
namespace {
using editor::JsonType;
using editor::JsonValue;
constexpr std::string_view kFile = ".lucid/tasks.json";
std::string quote(std::string_view text) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string result = "\"";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') { result += '\\'; result += static_cast<char>(c); }
        else if (c < 32) { result += "\\u00"; result += hex[c >> 4]; result += hex[c & 15]; }
        else result += static_cast<char>(c);
    }
    return result + '"';
}
bool stringField(const JsonValue& value, const char* name, std::string& output) {
    const auto* field = value.find(name);
    if (!field || field->type != JsonType::String) return false;
    output = field->stringValue; return true;
}
bool boolField(const JsonValue& value, const char* name, bool& output) {
    const auto* field = value.find(name);
    if (!field || field->type != JsonType::Bool) return false;
    output = field->boolValue; return true;
}
bool uintField(const JsonValue& value, const char* name, std::size_t& output) {
    const auto* field = value.find(name);
    if (!field || field->type != JsonType::Number || !std::isfinite(field->numberValue) ||
        field->numberValue < 0 || field->numberValue > 10000000 || std::floor(field->numberValue) != field->numberValue) return false;
    output = static_cast<std::size_t>(field->numberValue); return true;
}
bool parseBoard(std::string_view text, TaskBoard& board, std::string& error) {
    editor::JsonError jsonError;
    const auto root = editor::parseJson(text, jsonError);
    if (!root || root->type != JsonType::Object) { error = "Invalid task file: " + jsonError.message; return false; }
    const auto* version = root->find("version");
    const auto* tasks = root->find("tasks");
    if (!version || version->type != JsonType::Number || version->numberValue != 1 || !tasks || tasks->type != JsonType::Array) {
        error = "Unsupported or invalid task file"; return false;
    }
    TaskBoard parsed;
    for (const auto& entry : tasks->children) {
        Task task; std::size_t tier = 0;
        if (entry.type != JsonType::Object || !stringField(entry,"id",task.id) || task.id.empty() ||
            !stringField(entry,"title",task.title) || task.title.empty() || !uintField(entry,"tier",tier) || tier < 1 || tier > 3 ||
            !stringField(entry,"createdBy",task.createdBy) || !stringField(entry,"assignedTo",task.assignedTo) ||
            !boolField(entry,"done",task.done) || !stringField(entry,"doneBy",task.doneBy) ||
            !stringField(entry,"solution",task.solution) || !stringField(entry,"debugFunction",task.debugFunction)) {
            error = "Invalid task record"; return false;
        }
        task.tier = static_cast<int>(tier);
        if (const auto* source = entry.find("debugSource")) {
            if (source->type != JsonType::String) { error = "Invalid task debugger source"; return false; }
            task.debugSource = source->stringValue;
        }
        if (const auto* points = entry.find("debugBreakpoints")) {
            if (points->type != JsonType::Array || points->children.size() > 10000) { error = "Invalid task breakpoints"; return false; }
            for (const auto& point : points->children) {
                if (point.type != JsonType::Number || !std::isfinite(point.numberValue) || point.numberValue < 1 || point.numberValue > 10000000 || std::floor(point.numberValue) != point.numberValue) { error = "Invalid task breakpoint"; return false; }
                task.debugBreakpoints.push_back(static_cast<int>(point.numberValue));
            }
        }
        const auto* workers = entry.find("workingBy");
        if (!workers || workers->type != JsonType::Array) { error = "Invalid task workers"; return false; }
        for (const auto& worker : workers->children) {
            if (worker.type != JsonType::String || worker.stringValue.empty()) { error = "Invalid task worker"; return false; }
            task.workingBy.push_back(worker.stringValue);
        }
        const auto* subtasks = entry.find("subtasks"); const auto* history = entry.find("history"); const auto* files = entry.find("files");
        if (!subtasks || subtasks->type != JsonType::Array || !history || history->type != JsonType::Array || !files || files->type != JsonType::Array) {
            error = "Invalid task details"; return false;
        }
        for (const auto& item : subtasks->children) {
            Subtask sub;
            if (item.type != JsonType::Object || !stringField(item,"id",sub.id) || sub.id.empty() ||
                !stringField(item,"title",sub.title) || sub.title.empty() || !stringField(item,"createdBy",sub.createdBy) ||
                !boolField(item,"done",sub.done) || !stringField(item,"doneBy",sub.doneBy)) { error = "Invalid subtask"; return false; }
            task.subtasks.push_back(std::move(sub));
        }
        for (const auto& item : history->children) {
            TaskActivity activity;
            if (item.type != JsonType::Object || !stringField(item,"actor",activity.actor) ||
                !stringField(item,"action",activity.action) || !stringField(item,"subject",activity.subject) ||
                !stringField(item,"time",activity.time)) { error = "Invalid task history"; return false; }
            task.history.push_back(std::move(activity));
        }
        for (const auto& item : files->children) {
            TaskFileContext file; std::size_t first = 0;
            if (item.type != JsonType::Object || !stringField(item,"path",file.path) || file.path.empty() ||
                !uintField(item,"line",file.line) || !uintField(item,"column",file.column) || !uintField(item,"firstLine",first)) {
                error = "Invalid task workspace"; return false;
            }
            file.firstLine = static_cast<int>(first); task.files.push_back(std::move(file));
        }
        for (const auto& existing : parsed.tasks) if (existing.id == task.id) { error = "Duplicate task ID"; return false; }
        parsed.tasks.push_back(std::move(task));
    }
    board = std::move(parsed); return true;
}
std::string serialize(const TaskBoard& board) {
    std::ostringstream out; out << "{\"version\":1,\"tasks\":[";
    for (std::size_t i=0; i<board.tasks.size(); ++i) {
        const auto& t=board.tasks[i]; if (i) out << ',';
        out << "{\"id\":" << quote(t.id) << ",\"title\":" << quote(t.title) << ",\"tier\":" << t.tier
            << ",\"createdBy\":" << quote(t.createdBy) << ",\"assignedTo\":" << quote(t.assignedTo)
            << ",\"workingBy\":[";
        for (std::size_t j=0; j<t.workingBy.size(); ++j) { if (j) out << ','; out << quote(t.workingBy[j]); }
        out << "],\"done\":" << (t.done?"true":"false")
            << ",\"doneBy\":" << quote(t.doneBy) << ",\"solution\":" << quote(t.solution)
            << ",\"debugFunction\":" << quote(t.debugFunction) << ",\"debugSource\":" << quote(t.debugSource) << ",\"debugBreakpoints\":[";
        for (std::size_t j = 0; j < t.debugBreakpoints.size(); ++j) { if (j) out << ','; out << t.debugBreakpoints[j]; }
        out << "],\"subtasks\":[";
        for (std::size_t j=0; j<t.subtasks.size(); ++j) { const auto& s=t.subtasks[j]; if(j) out << ',';
            out << "{\"id\":" << quote(s.id) << ",\"title\":" << quote(s.title) << ",\"createdBy\":" << quote(s.createdBy)
                << ",\"done\":" << (s.done?"true":"false") << ",\"doneBy\":" << quote(s.doneBy) << '}'; }
        out << "],\"history\":[";
        for (std::size_t j=0; j<t.history.size(); ++j) { const auto& a=t.history[j]; if(j) out << ',';
            out << "{\"actor\":" << quote(a.actor) << ",\"action\":" << quote(a.action) << ",\"subject\":" << quote(a.subject) << ",\"time\":" << quote(a.time) << '}'; }
        out << "],\"files\":[";
        for (std::size_t j=0; j<t.files.size(); ++j) { const auto& f=t.files[j]; if(j) out << ',';
            out << "{\"path\":" << quote(f.path) << ",\"line\":" << f.line << ",\"column\":" << f.column << ",\"firstLine\":" << f.firstLine << '}'; }
        out << "]}";
    }
    return out.str()+"]}\n";
}
}
std::string newTaskId() {
    static std::atomic<unsigned long long> counter{0};
    static std::random_device random;
    std::ostringstream out; out << std::hex << std::chrono::steady_clock::now().time_since_epoch().count() << '-' << random() << '-' << counter++;
    return out.str();
}
void recordTaskActivity(Task& task, std::string_view actor, std::string_view action, std::string_view subject) {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    std::ostringstream timestamp;
    timestamp << std::put_time(&utc, "%Y-%m-%d %H:%M:%S UTC");
    task.history.push_back({std::string(actor), std::string(action), std::string(subject), timestamp.str()});
}
Task* findTask(TaskBoard& board, std::string_view id) {
    const auto found = std::find_if(board.tasks.begin(), board.tasks.end(),
        [id](const Task& task) { return task.id == id; });
    return found == board.tasks.end() ? nullptr : &*found;
}
bool createTask(TaskBoard& board, std::string_view title, int tier, std::string_view actor, std::string& id) {
    if (title.empty() || actor.empty() || tier < 1 || tier > 3) return false;
    Task task; task.id = newTaskId(); task.title = title; task.tier = tier; task.createdBy = actor;
    recordTaskActivity(task, actor, "created"); id = task.id; board.tasks.push_back(std::move(task)); return true;
}
bool addSubtask(Task& task, std::string_view title, std::string_view actor) {
    if (title.empty() || actor.empty()) return false;
    Subtask sub; sub.id = newTaskId(); sub.title = title; sub.createdBy = actor;
    recordTaskActivity(task, actor, "created subtask", sub.title); task.subtasks.push_back(std::move(sub)); return true;
}
bool setTaskDone(Task& task, bool done, std::string_view actor) {
    if (actor.empty() || task.done == done) return false;
    task.done = done; task.doneBy = done ? std::string(actor) : std::string();
    recordTaskActivity(task, actor, done ? "completed task" : "reopened task"); return true;
}
bool setSubtaskDone(Task& task, std::string_view id, bool done, std::string_view actor) {
    if (actor.empty()) return false;
    const auto found = std::find_if(task.subtasks.begin(), task.subtasks.end(),
        [id](const Subtask& sub) { return sub.id == id; });
    if (found == task.subtasks.end() || found->done == done) return false;
    found->done = done; found->doneBy = done ? std::string(actor) : std::string();
    recordTaskActivity(task, actor, done ? "completed subtask" : "reopened subtask", found->title); return true;
}
bool setTaskAssignee(Task& task, std::string_view assignee, std::string_view actor) {
    if (actor.empty() || task.assignedTo == assignee) return false;
    task.assignedTo = assignee; recordTaskActivity(task, actor, "assigned task", assignee); return true;
}
bool setTaskSolution(Task& task, std::string_view solution, std::string_view actor) {
    if (actor.empty() || task.solution == solution) return false;
    task.solution = solution; recordTaskActivity(task, actor, "updated solution", solution); return true;
}
bool setTaskDebugFunction(Task& task, std::string_view function, std::string_view actor) {
    if (actor.empty() || task.debugFunction == function) return false;
    task.debugFunction = function; recordTaskActivity(task, actor, "linked debug function", function); return true;
}
bool setTaskWorking(Task& task, bool working, std::string_view actor) {
    if (actor.empty()) return false;
    auto found = std::find(task.workingBy.begin(), task.workingBy.end(), actor);
    if (working && found == task.workingBy.end()) task.workingBy.emplace_back(actor);
    else if (!working && found != task.workingBy.end()) task.workingBy.erase(found);
    else return false;
    recordTaskActivity(task, actor, working ? "started work" : "stopped work"); return true;
}
bool loadTasks(const Project& project, TaskBoard& board, std::string& error) {
    std::string resolveError; const auto path = project.resolve(kFile, resolveError);
    if (!resolveError.empty()) { error=resolveError; return false; }
    std::error_code ec; const bool exists=std::filesystem::exists(path, ec);
    if (ec) { error=ec.message(); return false; }
    if (!exists) { board={}; error.clear(); return true; }
    std::string contents;
    if (!project.readFile(kFile, contents, error)) return false;
    TaskBoard parsed;
    if (!parseBoard(contents, parsed, error)) return false;
    parsed.sourceContents = contents;
    parsed.sourceExists = true;
    board = std::move(parsed);
    error.clear();
    return true;
}
bool saveTasks(const Project& project, TaskBoard& board, std::string& error) {
    const auto path=project.resolve(kFile,error); if(!error.empty()) return false;
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) { error = ec.message(); return false; }
    if (exists != board.sourceExists) { error = "Task file changed; reopen tasks and try again"; return false; }
    if (exists) {
        std::string current;
        if (!project.readFile(kFile, current, error)) return false;
        if (current != board.sourceContents) { error = "Task file changed; reopen tasks and try again"; return false; }
    }
    const auto contents=serialize(board);
    TaskBoard validated;
    if (contents.size() > Project::maxFileSize || !parseBoard(contents, validated, error)) {
        error = "Cannot save invalid or oversized task board" + (error.empty() ? std::string{} : ": " + error); return false;
    }
    auto writer = project;
    if (!writer.writeFile(kFile, contents, error, exists ? &board.sourceContents : nullptr, !exists)) return false;
    board.sourceContents = contents;
    board.sourceExists = true;
    error.clear(); return true;
}
}
