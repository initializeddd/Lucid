#include "modules/ContainerActions.hpp"
#include "editor/json/Json.hpp"
#include "text/Utf8.hpp"
#include "workspace/Project.hpp"
#include <algorithm>
#include <set>
namespace lucid::modules {
namespace {
constexpr std::size_t kMaxConfig = 128U * 1024U, kMaxCompose = 1024U * 1024U;
bool clean(std::string_view text, std::size_t limit) {
    if (text.empty() || text.size() > limit) return false;
    for (std::size_t i = 0; i < text.size();) {
        const auto start = i; const auto cp = text::nextUtf8(text, i);
        if (cp < 32 || cp == 127 || (cp == U'\ufffd' && i - start == 1 && static_cast<unsigned char>(text[start]) >= 0x80)) return false;
    }
    return true;
}
bool unique(const editor::JsonValue& value, std::size_t depth = 0) {
    if (depth > 16) return false;
    if (value.type == editor::JsonType::Object) {
        std::set<std::string> keys; for (const auto& item : value.children) if (!keys.insert(item.key).second || !unique(item, depth + 1)) return false;
    } else if (value.type == editor::JsonType::Array) for (const auto& item : value.children) if (!unique(item, depth + 1)) return false;
    return true;
}
const editor::JsonValue* field(const editor::JsonValue& value, const char* name, editor::JsonType type) {
    const auto* found = value.find(name); return found && found->type == type ? found : nullptr;
}
bool serviceName(std::string_view text) {
    if (text.empty() || text.size() > 128 || !((text.front() >= 'a' && text.front() <= 'z') || (text.front() >= 'A' && text.front() <= 'Z') || (text.front() >= '0' && text.front() <= '9'))) return false;
    return std::all_of(text.begin(), text.end(), [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'; });
}
std::string pathUtf8(const std::filesystem::path& path) { auto value = path.u8string(); return {value.begin(), value.end()}; }
bool equalAction(const ContainerProjectAction& a, const ContainerProjectAction& b) {
    return a.name == b.name && a.operation == b.operation && a.composeFile == b.composeFile && a.services == b.services;
}
}
bool parseContainerProjectActions(std::string_view json, std::vector<ContainerProjectAction>& actions, std::string& error) {
    actions.clear(); error.clear(); if (json.size() > kMaxConfig) { error = "Action configuration exceeds size limit"; return false; }
    editor::JsonError parseError; auto root = editor::parseJson(json, parseError);
    if (!root || root->type != editor::JsonType::Object || !unique(*root) || root->children.size() != 2) { error = "Invalid action configuration"; return false; }
    const auto* version = field(*root, "version", editor::JsonType::Number); const auto* list = field(*root, "actions", editor::JsonType::Array);
    if (!version || version->numberValue != 1 || !list || list->children.empty() || list->children.size() > 32) { error = "Expected version 1 and 1–32 actions"; return false; }
    std::vector<ContainerProjectAction> result; std::set<std::string> names;
    for (const auto& item : list->children) {
        if (item.type != editor::JsonType::Object || item.children.size() != 4) { error = "Invalid action fields"; return false; }
        const auto* name = field(item, "name", editor::JsonType::String); const auto* op = field(item, "operation", editor::JsonType::String);
        const auto* file = field(item, "composeFile", editor::JsonType::String); const auto* services = field(item, "services", editor::JsonType::Array);
        if (!name || !clean(name->stringValue, 128) || !names.insert(name->stringValue).second || !op ||
            (op->stringValue != "build" && op->stringValue != "up" && op->stringValue != "stop" && op->stringValue != "restart") ||
            !file || !clean(file->stringValue, 4096) || file->stringValue.find('\\') != std::string::npos || !services || services->children.empty() || services->children.size() > 16) { error = "Invalid action definition"; return false; }
        ContainerProjectAction action{name->stringValue, op->stringValue, file->stringValue, {}}; std::set<std::string> seen;
        for (const auto& service : services->children) if (service.type != editor::JsonType::String || !serviceName(service.stringValue) || !seen.insert(service.stringValue).second) { error = "Invalid or duplicate Compose service"; return false; } else action.services.push_back(service.stringValue);
        result.push_back(std::move(action));
    }
    actions = std::move(result); return true;
}
bool prepareContainerProjectAction(const workspace::Project& project, std::string_view configPath, std::size_t index,
    const std::filesystem::path& executable, ContainerActionReview& review, std::string& error, const std::atomic_bool* cancel) {
    review = {}; error.clear(); if (cancel && cancel->load()) { error = "Action review cancelled"; return false; }
    const auto name = executable.filename().string(); std::error_code ec;
    if (!executable.is_absolute() || (name != "docker" && name != "docker.exe" && name != "podman" && name != "podman.exe") || !std::filesystem::is_regular_file(executable, ec) || ec) { error = "Choose an absolute Docker or Podman executable"; return false; }
    std::string config, compose; if (!project.readFileBounded(configPath, config, error, kMaxConfig, cancel)) return false;
    std::vector<ContainerProjectAction> actions; if (!parseContainerProjectActions(config, actions, error)) return false;
    if (index >= actions.size()) { error = "Action no longer exists"; return false; }
    if (!project.readFileBounded(actions[index].composeFile, compose, error, kMaxCompose, cancel)) return false;
    const auto resolved = project.resolve(actions[index].composeFile, error); if (!error.empty()) return false;
    ContainerActionReview result; result.projectRoot = pathUtf8(project.root()); result.configPath = std::string(configPath); result.configSnapshot = std::move(config); result.composeSnapshot = std::move(compose);
    result.executable = executable; result.actionIndex = index; result.action = std::move(actions[index]);
    result.arguments = {"compose", "--project-directory", result.projectRoot, "--file", pathUtf8(resolved), result.action.operation};
    if (result.action.operation == "up") result.arguments.push_back("--detach");
    result.arguments.push_back("--"); result.arguments.insert(result.arguments.end(), result.action.services.begin(), result.action.services.end());
    review = std::move(result); return true;
}
ModuleProcessResult runContainerProjectAction(const workspace::Project& project, const ContainerActionReview& review,
    bool confirmed, const ContainerActionRunner& runner, const std::atomic_bool* cancel) {
    ModuleProcessResult result;
    if (cancel && cancel->load()) { result.cancelled = true; return result; }
    if (!confirmed || !runner) { result.error = "Explicit action confirmation is required"; return result; }
    if (pathUtf8(project.root()) != review.projectRoot) { result.error = "Project changed; review the action again"; return result; }
    ContainerActionReview fresh; std::string error;
    if (!prepareContainerProjectAction(project, review.configPath, review.actionIndex, review.executable, fresh, error, cancel)) { if (cancel && cancel->load()) result.cancelled = true; else result.error = "Action could not be revalidated: " + error; return result; }
    if (fresh.configSnapshot != review.configSnapshot || fresh.composeSnapshot != review.composeSnapshot || !equalAction(fresh.action, review.action) || fresh.arguments != review.arguments) { result.error = "Action or Compose file changed; review it again"; return result; }
    if (cancel && cancel->load()) { result.cancelled = true; return result; }
    return runner(fresh.executable, fresh.arguments, cancel);
}
ModuleProcessResult runBoundedContainerProjectAction(const workspace::Project& project, const ContainerActionReview& review,
    bool confirmed, const std::atomic_bool* cancel) {
    return runContainerProjectAction(project, review, confirmed,
        [](const auto& executable, const auto& args, const auto* token) {
            return runModuleProcess(executable, args, std::chrono::seconds(120), 1024U * 1024U, token);
        }, cancel);
}
}
