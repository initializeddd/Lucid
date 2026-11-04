#include "debug/Session.hpp"
#include "editor/json/Json.hpp"
#include <cmath>
#include <limits>
namespace lucid::debug {
namespace {
using editor::JsonType; using editor::JsonValue;
const JsonValue* member(const JsonValue* v, std::string_view name) { return v && v->type == JsonType::Object ? v->find(name) : nullptr; }
std::string text(const JsonValue* v) { return v && v->type == JsonType::String ? v->stringValue : std::string{}; }
int integer(const JsonValue* v) {
    return v && v->type == JsonType::Number && std::isfinite(v->numberValue) && v->numberValue >= 0 &&
        v->numberValue <= std::numeric_limits<int>::max() && std::floor(v->numberValue) == v->numberValue ? static_cast<int>(v->numberValue) : 0;
}
}
void Session::request(std::string command, std::string arguments) {
    const int seq = m_seq++; m_requests.emplace(seq, Pending{command, m_generation, {}});
    m_outbound += frame("{\"seq\":" + std::to_string(seq) + ",\"type\":\"request\",\"command\":" + jsonString(command) + ",\"arguments\":" + arguments + "}");
}
void Session::start(std::string program, std::string cwd, std::string source, std::vector<int> breakpoints) {
    std::map<std::string, std::vector<int>> all;
    all.emplace(source, std::move(breakpoints));
    start(std::move(program), std::move(cwd), std::move(all));
    m_source = std::move(source);
}
void Session::start(std::string program, std::string cwd, std::map<std::string, std::vector<int>> breakpoints, std::vector<std::string> arguments, std::map<std::string, std::string> environment) {
    m_decoder.reset(); m_seq = 1; m_thread = 0; m_generation = 0; m_initialized = false; m_requests.clear(); m_outbound.clear(); m_stack.clear(); m_variables.clear(); m_output.clear(); m_breakpointWarning.clear(); m_breakpointWarnings.clear(); m_breakpointRequests.clear(); m_previousValues.clear(); m_previousFrameName.clear(); m_previousFramePath.clear(); m_selectedFrame = 0; m_expanded = false;
    m_threadListRequest = 0; m_threads.clear(); m_program = std::move(program); m_cwd = std::move(cwd); m_source = breakpoints.empty() ? std::string{} : breakpoints.begin()->first; m_breakpoints = std::move(breakpoints); m_arguments = std::move(arguments); m_environment = std::move(environment);
    m_state = State::Initializing; m_message = "Starting debugger"; m_messageIsError = false;
    request("initialize", "{\"clientID\":\"lucid\",\"adapterID\":\"lldb-dap\",\"linesStartAt1\":true,\"columnsStartAt1\":true,\"pathFormat\":\"path\",\"supportsRunInTerminalRequest\":false}");
}
std::string Session::takeOutbound() { std::string result; result.swap(m_outbound); return result; }
bool Session::receive(std::string_view bytes, std::string& error) {
    std::vector<std::string> messages;
    if (!m_decoder.append(bytes, messages, error)) { transportClosed(error, false); return false; }
    for (const auto& message : messages) {
        handle(message);
        if (m_state == State::Failed) { error = m_message; transportClosed(error, false); return false; }
    }
    return true;
}
void Session::handle(std::string_view raw) {
    editor::JsonError error; const auto value = editor::parseJson(raw, error);
    if (!value || value->type != JsonType::Object) { m_state = State::Failed; m_message = "Invalid message from debug adapter"; return; }
    const auto type = text(value->find("type"));
    if (type == "request") {
        const int id = integer(value->find("seq")); const auto command = text(value->find("command"));
        m_outbound += frame("{\"seq\":" + std::to_string(m_seq++) + ",\"type\":\"response\",\"request_seq\":" + std::to_string(id) + ",\"command\":" + jsonString(command) + ",\"success\":false,\"message\":\"Unsupported adapter request\"}"); return;
    }
    if (type == "event") {
        const auto event = text(value->find("event")); const auto* body = value->find("body");
        if ((m_state == State::Ended || m_state == State::Failed) && event != "output") return;
        if (event == "initialized") {
            if (m_initialized) return;
            m_initialized = true; std::string ignored;
            const auto allBreakpoints = m_breakpoints;
            for (const auto& [source, lines] : allBreakpoints) (void)setBreakpoints(source, lines, ignored);
            request("configurationDone", "{}");
        } else if (event == "stopped") {
            ++m_generation; m_threadListRequest = 0; m_state = State::Stopped; m_thread = integer(member(body, "threadId")); m_message = "Stopped: " + text(member(body, "reason")); m_messageIsError = false; m_stack.clear(); m_threads.clear(); m_variables.clear();
            if (m_thread) request("stackTrace", "{\"threadId\":" + std::to_string(m_thread) + ",\"startFrame\":0,\"levels\":30}");
            else { request("threads", "{}"); m_threadListRequest = m_seq - 1; }
        } else if (event == "continued") { ++m_generation; m_threadListRequest = 0; m_state = State::Running; m_message = "Running"; m_messageIsError = false; m_stack.clear(); m_threads.clear(); m_variables.clear(); }
        else if (event == "terminated" || event == "exited") { ++m_generation; m_threadListRequest = 0; m_initialized = false; m_state = State::Ended; m_message = "Debug session ended"; m_messageIsError = false; m_stack.clear(); m_threads.clear(); m_variables.clear(); m_thread = 0; }
        else if (event == "output") { m_output += text(member(body, "output")); if (m_output.size() > 65536) m_output.erase(0, m_output.size() - 65536); }
        return;
    }
    if (type != "response") return;
    const int seq = integer(value->find("request_seq")); const auto pending = m_requests.find(seq);
    if (pending == m_requests.end()) return;
    const auto pendingRequest = pending->second; m_requests.erase(pending);
    const std::string& command = pendingRequest.command;
    if (m_state == State::Ended || m_state == State::Failed) return;
    if (command == "setBreakpoints" && !pendingRequest.source.empty() && m_breakpointRequests[pendingRequest.source] != seq) return;
    if ((command == "threads" || command == "stackTrace" || command == "scopes" || command == "variables") && pendingRequest.generation != m_generation) return;
    if (command == "threads") {
        if (seq != m_threadListRequest || m_state != State::Stopped) return;
        m_threadListRequest = 0;
    }
    const auto* success = value->find("success"); const auto* body = value->find("body");
    if (!success || success->type != JsonType::Bool || !success->boolValue) {
        if (command == "setBreakpoints") {
            m_breakpointWarnings[pendingRequest.source] = command + ": " + text(value->find("message"));
            m_breakpointWarning = m_breakpointWarnings.begin()->second;
            return;
        }
        m_message = command + ": " + text(value->find("message"));
        m_messageIsError = true;
        if (command == "initialize" || command == "launch" || command == "configurationDone") m_state = State::Failed;
        else if ((command == "continue" || command == "next" || command == "stepIn" || command == "stepOut") && pendingRequest.generation == m_generation) {
            m_state = State::Stopped;
            if (m_thread) request("stackTrace", "{\"threadId\":" + std::to_string(m_thread) + ",\"startFrame\":0,\"levels\":30}");
        }
        return;
    }
    if (command == "initialize") {
        m_state = State::Launching; m_message = "Launching program"; m_messageIsError = false;
        std::string launch = "{\"name\":\"Lucid\",\"type\":\"lldb-dap\",\"request\":\"launch\",\"program\":" + jsonString(m_program) + ",\"cwd\":" + jsonString(m_cwd) + ",\"stopOnEntry\":false";
        launch += ",\"args\":[";
        for (std::size_t i = 0; i < m_arguments.size(); ++i) { if (i) launch += ','; launch += jsonString(m_arguments[i]); }
        launch += ']';
        launch += ",\"env\":{";
        bool first = true;
        for (const auto& [key, value] : m_environment) { if (!first) launch += ','; first = false; launch += jsonString(key) + ':' + jsonString(value); }
        launch += '}';
#if defined(__linux__)
        // Linux containers commonly prohibit disabling ASLR via personality(2).
        launch += ",\"initCommands\":[\"settings set target.disable-aslr false\"]";
#endif
        request("launch", launch + "}");
    } else if (command == "launch" && m_state == State::Launching) { m_state = State::Running; m_message = "Running"; m_messageIsError = false; }
    else if (command == "setBreakpoints") {
        std::string warning; const auto* points = member(body, "breakpoints");
        if (points && points->type == JsonType::Array) for (const auto& point : points->children) {
            const auto* verified = point.find("verified");
            if (!verified || verified->type != JsonType::Bool || !verified->boolValue) {
                warning = "A breakpoint could not be verified";
                const auto detail = text(point.find("message")); if (!detail.empty()) warning += ": " + detail;
                break;
            }
        }
        if (warning.empty()) m_breakpointWarnings.erase(pendingRequest.source);
        else m_breakpointWarnings[pendingRequest.source] = warning;
        m_breakpointWarning = m_breakpointWarnings.empty() ? std::string{} : m_breakpointWarnings.begin()->second;
    }
    else if (command == "threads") {
        const auto* threads = member(body, "threads"); m_threads.clear();
        if (threads && threads->type == JsonType::Array) for (const auto& item : threads->children) {
            const int id = integer(item.find("id"));
            if (id > 0) m_threads.push_back({id, text(item.find("name"))});
            if (m_threads.size() == 256) break;
        }
        if (!m_thread && !m_threads.empty() && m_state == State::Stopped) {
            m_thread = m_threads.front().id;
            request("stackTrace", "{\"threadId\":" + std::to_string(m_thread) + ",\"startFrame\":0,\"levels\":30}");
        }
        m_message = m_threads.empty() ? "No threads reported" : "Threads loaded"; m_messageIsError = false;
    } else if (command == "stackTrace" && m_state == State::Stopped) {
        const auto* stack = member(body, "stackFrames"); m_stack.clear();
        if (stack && stack->type == JsonType::Array) for (const auto& item : stack->children) {
            const auto* source = item.find("source"); m_stack.push_back({integer(item.find("id")), integer(item.find("line")), integer(item.find("column")), text(item.find("name")), text(member(source, "path"))});
        }
        if (!m_stack.empty()) { m_selectedFrame = 0; m_expanded = false; request("scopes", "{\"frameId\":" + std::to_string(m_stack.front().id) + "}"); }
    } else if (command == "scopes" && m_state == State::Stopped) {
        const auto* scopes = member(body, "scopes");
        if (scopes && scopes->type == JsonType::Array && !scopes->children.empty()) {
            const int ref = integer(scopes->children.front().find("variablesReference"));
            if (ref) request("variables", "{\"variablesReference\":" + std::to_string(ref) + ",\"start\":0,\"count\":100}");
        }
    } else if (command == "variables" && m_state == State::Stopped) {
        const auto* vars = member(body, "variables"); m_variables.clear();
        if (vars && vars->type == JsonType::Array) for (const auto& item : vars->children) {
            Variable variable{text(item.find("name")), text(item.find("value")), text(item.find("type")), integer(item.find("variablesReference")), false};
            if (!m_expanded && m_selectedFrame < m_stack.size() && m_stack[m_selectedFrame].name == m_previousFrameName && m_stack[m_selectedFrame].path == m_previousFramePath) {
                const auto old = m_previousValues.find(variable.name);
                variable.changed = old != m_previousValues.end() && old->second != variable.value;
            }
            m_variables.push_back(std::move(variable));
        }
    }
}
bool Session::resume(std::string_view command, std::string& error) {
    if (m_state != State::Stopped || !m_thread) { error = "Debugger is not stopped"; return false; }
    if (command != "continue" && command != "next" && command != "stepIn" && command != "stepOut") { error = "Unknown debug action"; return false; }
    m_previousValues.clear();
    if (!m_expanded && m_selectedFrame < m_stack.size() && command != "continue") {
        m_previousFrameName = m_stack[m_selectedFrame].name; m_previousFramePath = m_stack[m_selectedFrame].path;
        for (const auto& variable : m_variables) m_previousValues[variable.name] = variable.value;
    } else { m_previousFrameName.clear(); m_previousFramePath.clear(); }
    ++m_generation; m_threadListRequest = 0; request(std::string(command), "{\"threadId\":" + std::to_string(m_thread) + "}"); m_state = State::Running; m_message = "Running"; m_messageIsError = false; m_stack.clear(); m_threads.clear(); m_variables.clear(); error.clear(); return true;
}
bool Session::requestThreads(std::string& error) {
    if (m_state != State::Stopped) { error = "Debugger is not stopped"; return false; }
    m_threads.clear(); m_message = "Loading threads"; m_messageIsError = false;
    request("threads", "{}"); m_threadListRequest = m_seq - 1; error.clear(); return true;
}
bool Session::selectThread(std::size_t index, std::string& error) {
    if (m_state != State::Stopped || index >= m_threads.size()) { error = "Thread is unavailable"; return false; }
    ++m_generation; m_threadListRequest = 0; m_thread = m_threads[index].id; m_stack.clear(); m_variables.clear(); m_selectedFrame = 0; m_expanded = false;
    m_previousValues.clear(); m_previousFrameName.clear(); m_previousFramePath.clear();
    request("stackTrace", "{\"threadId\":" + std::to_string(m_thread) + ",\"startFrame\":0,\"levels\":30}"); error.clear(); return true;
}
bool Session::selectFrame(std::size_t index, std::string& error) {
    if (m_state != State::Stopped || index >= m_stack.size()) { error = "Stack frame is unavailable"; return false; }
    ++m_generation; m_threadListRequest = 0; m_selectedFrame = index; m_expanded = false; m_variables.clear();
    request("scopes", "{\"frameId\":" + std::to_string(m_stack[index].id) + "}"); error.clear(); return true;
}
bool Session::expandVariable(std::size_t index, std::string& error) {
    if (m_state != State::Stopped || index >= m_variables.size() || m_variables[index].reference <= 0) { error = "Variable cannot be expanded"; return false; }
    const int ref = m_variables[index].reference; ++m_generation; m_threadListRequest = 0; m_expanded = true; m_variables.clear();
    request("variables", "{\"variablesReference\":" + std::to_string(ref) + ",\"start\":0,\"count\":100}"); error.clear(); return true;
}
bool Session::setBreakpoints(std::vector<int> lines, std::string& error) {
    return setBreakpoints(m_source, std::move(lines), error);
}
bool Session::setBreakpoints(std::string source, std::vector<int> lines, std::string& error) {
    if (source.empty()) { error = "Invalid breakpoint source"; return false; }
    for (int line : lines) if (line < 1) { error = "Invalid breakpoint line"; return false; }
    m_breakpoints[source] = lines;
    if (m_initialized && m_state != State::Ended && m_state != State::Failed) {
        std::string args = "{\"source\":{\"path\":" + jsonString(source) + "},\"breakpoints\":[";
        for (std::size_t i = 0; i < lines.size(); ++i) { if (i) args += ','; args += "{\"line\":" + std::to_string(lines[i]) + "}"; }
        args += "]}"; request("setBreakpoints", args);
        m_requests[m_seq - 1].source = source; m_breakpointRequests[source] = m_seq - 1;
    }
    error.clear(); return true;
}
void Session::disconnect() { if (m_state != State::Idle && m_state != State::Ended && m_state != State::Failed) request("disconnect", "{\"terminateDebuggee\":true}"); }
void Session::transportClosed(std::string message, bool expected) {
    ++m_generation; m_threadListRequest = 0; m_initialized = false; m_thread = 0; m_stack.clear(); m_threads.clear(); m_variables.clear();
    m_requests.clear(); m_outbound.clear();
    m_state = expected ? State::Ended : State::Failed;
    m_messageIsError = !expected;
    m_message = std::move(message);
}
}
