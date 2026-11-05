#pragma once
#include "debug/Protocol.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::debug {
enum class State { Idle, Initializing, Launching, Running, Stopped, Ended, Failed };
struct StackFrame { int id = 0, line = 0, column = 0; std::string name, path; };
struct Variable { std::string name, value, type; int reference = 0; bool changed = false; };
struct Thread { int id = 0; std::string name; };
class Session {
public:
    void start(std::string program, std::string cwd, std::string source, std::vector<int> breakpoints);
    void start(std::string program, std::string cwd, std::map<std::string, std::vector<int>> breakpoints,
               std::vector<std::string> arguments = {}, std::map<std::string, std::string> environment = {});
    [[nodiscard]] bool receive(std::string_view bytes, std::string& error);
    [[nodiscard]] std::string takeOutbound();
    [[nodiscard]] bool resume(std::string_view command, std::string& error);
    [[nodiscard]] bool pause(std::string& error);
    [[nodiscard]] bool pausePending() const noexcept { return m_pauseRequested; }
    void pauseTimedOut();
    void disconnect();
    void transportClosed(std::string message, bool expected);
    [[nodiscard]] bool setBreakpoints(std::vector<int> lines, std::string& error);
    [[nodiscard]] bool setBreakpoints(std::string source, std::vector<int> lines, std::string& error);
    [[nodiscard]] bool selectFrame(std::size_t index, std::string& error);
    [[nodiscard]] bool expandVariable(std::size_t index, std::string& error);
    [[nodiscard]] bool requestThreads(std::string& error);
    [[nodiscard]] bool selectThread(std::size_t index, std::string& error);
    [[nodiscard]] const std::vector<Thread>& threads() const noexcept { return m_threads; }
    [[nodiscard]] int selectedThread() const noexcept { return m_thread; }
    [[nodiscard]] bool threadsPending() const noexcept { return m_threadListRequest != 0; }
    [[nodiscard]] std::size_t selectedFrame() const noexcept { return m_selectedFrame; }
    [[nodiscard]] bool expandedVariables() const noexcept { return m_expanded; }
    [[nodiscard]] State state() const noexcept { return m_state; }
    [[nodiscard]] const std::string& message() const noexcept { return m_message; }
    [[nodiscard]] bool messageIsError() const noexcept { return m_messageIsError; }
    [[nodiscard]] const std::vector<StackFrame>& stack() const noexcept { return m_stack; }
    [[nodiscard]] const std::vector<Variable>& variables() const noexcept { return m_variables; }
    [[nodiscard]] const std::string& output() const noexcept { return m_output; }
    [[nodiscard]] const std::string& breakpointWarning() const noexcept { return m_breakpointWarning; }
private:
    void request(std::string command, std::string arguments);
    void handle(std::string_view json);
    State m_state = State::Idle;
    FrameDecoder m_decoder;
    int m_seq = 1, m_thread = 0;
    int m_threadListRequest = 0;
    std::uint64_t m_generation = 0;
    bool m_initialized = false, m_messageIsError = false;
    bool m_pauseRequested = false;
    std::string m_program, m_cwd, m_source, m_outbound, m_message, m_output, m_breakpointWarning;
    std::vector<std::string> m_arguments;
    std::map<std::string, std::string> m_environment;
    std::map<std::string, std::vector<int>> m_breakpoints;
    std::map<std::string, std::string> m_breakpointWarnings;
    std::map<std::string, int> m_breakpointRequests;
    std::vector<StackFrame> m_stack;
    std::vector<Thread> m_threads;
    std::vector<Variable> m_variables;
    std::map<std::string, std::string> m_previousValues;
    std::string m_previousFrameName, m_previousFramePath;
    std::size_t m_selectedFrame = 0;
    bool m_expanded = false;
    struct Pending { std::string command; std::uint64_t generation; std::string source; };
    std::map<int, Pending> m_requests;
};
}
