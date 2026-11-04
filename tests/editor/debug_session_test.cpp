#include "debug/Session.hpp"
#include <string>
using namespace lucid::debug;
int main() {
    Session threads; std::string threadError;
    threads.start("/tmp/program", "/tmp", "/tmp/file.cpp", {}); (void)threads.takeOutbound();
    if (!threads.receive(frame(R"({"type":"event","event":"stopped","body":{"reason":"breakpoint","threadId":42}})"), threadError)) return 72;
    (void)threads.takeOutbound();
    if (!threads.requestThreads(threadError) || threads.takeOutbound().find("\"threads\"") == std::string::npos ||
        !threads.receive(frame(R"({"type":"response","request_seq":3,"success":true,"body":{"threads":[{"id":42,"name":"main"},{"id":77,"name":"worker"}]}})"), threadError) ||
        threads.threads().size() != 2 || threads.selectedThread() != 42) return 73;
    if (!threads.selectThread(1, threadError) || threads.selectedThread() != 77 || threads.takeOutbound().find("\"threadId\":77") == std::string::npos) return 74;
    if (!threads.receive(frame(R"({"type":"response","request_seq":2,"success":true,"body":{"stackFrames":[{"id":4,"name":"stale"}]}})"), threadError) || !threads.stack().empty()) return 75;
    Session threadStatus; threadStatus.start("/tmp/program", "/tmp", "/tmp/file.cpp", {}); (void)threadStatus.takeOutbound();
    if (!threadStatus.receive(frame(R"({"type":"event","event":"stopped","body":{"threadId":1}})"), threadError)) return 76;
    (void)threadStatus.takeOutbound();
    if (!threadStatus.requestThreads(threadError) || !threadStatus.requestThreads(threadError) || !threadStatus.threadsPending()) return 77;
    (void)threadStatus.takeOutbound();
    if (!threadStatus.receive(frame(R"({"type":"response","request_seq":3,"success":true,"body":{"threads":[{"id":4}]}})"), threadError) ||
        !threadStatus.threadsPending() || !threadStatus.threads().empty()) return 78;
    if (!threadStatus.receive(frame(R"({"type":"response","request_seq":4,"success":true,"body":{"threads":[]}})"), threadError) ||
        threadStatus.threadsPending() || threadStatus.message() != "No threads reported") return 79;
    if (!threadStatus.requestThreads(threadError)) return 80;
    (void)threadStatus.takeOutbound();
    if (!threadStatus.receive(frame(R"({"type":"response","request_seq":5,"success":false,"message":"unavailable"})"), threadError) ||
        threadStatus.threadsPending() || !threadStatus.messageIsError() || threadStatus.message().find("unavailable") == std::string::npos) return 81;
    Session withArguments; std::string argumentError;
    withArguments.start("/tmp/program", "/tmp", std::map<std::string, std::vector<int>>{}, {"hello world", "quote\"value", ""}, {{"LUCID_TEST", "hello world"}});
    (void)withArguments.takeOutbound();
    if (!withArguments.receive(frame(R"({"type":"response","request_seq":1,"success":true})"), argumentError)) return 70;
    const auto argumentLaunch = withArguments.takeOutbound();
    if (argumentLaunch.find(R"("args":["hello world","quote\"value",""])") == std::string::npos ||
        argumentLaunch.find(R"("env":{"LUCID_TEST":"hello world"})") == std::string::npos) return 71;
    Session s; std::string error;
    s.start("/tmp/a.out", "/tmp", "/tmp/a.cpp", {3});
    if (s.takeOutbound().find("\"initialize\"") == std::string::npos) return 1;
    if (!s.receive(frame(R"({"seq":1,"type":"response","request_seq":1,"command":"initialize","success":true})"), error)) return 2;
    if (s.takeOutbound().find("\"launch\"") == std::string::npos) return 3;
    if (!s.receive(frame(R"({"seq":2,"type":"event","event":"initialized"})"), error)) return 4;
    auto requests = s.takeOutbound();
    if (requests.find("\"setBreakpoints\"") == std::string::npos || requests.find("\"configurationDone\"") == std::string::npos) return 5;
    if (!s.receive(frame(R"({"seq":20,"type":"response","request_seq":3,"command":"setBreakpoints","success":true,"body":{"breakpoints":[{"verified":false,"message":"No symbols"}]}})"), error) || s.breakpointWarning().find("No symbols") == std::string::npos) return 22;
    if (!s.receive(frame(R"({"seq":3,"type":"event","event":"stopped","body":{"reason":"breakpoint","threadId":42}})"), error) || s.state() != State::Stopped) return 6;
    if (s.takeOutbound().find("\"stackTrace\"") == std::string::npos) return 7;
    if (!s.receive(frame(R"({"seq":4,"type":"response","request_seq":5,"command":"stackTrace","success":true,"body":{"stackFrames":[{"id":8,"name":"main","line":3,"column":1,"source":{"path":"/tmp/a.cpp"}}]}})"), error)) return 8;
    if (s.stack().size() != 1 || s.stack()[0].line != 3 || s.takeOutbound().find("\"scopes\"") == std::string::npos) return 9;
    if (!s.receive(frame(R"({"seq":5,"type":"response","request_seq":6,"command":"scopes","success":true,"body":{"scopes":[{"variablesReference":9}]}})"), error)) return 10;
    if (s.takeOutbound().find("\"variables\"") == std::string::npos) return 11;
    if (!s.receive(frame(R"({"seq":6,"type":"response","request_seq":7,"command":"variables","success":true,"body":{"variables":[{"name":"n","value":"7","type":"int","variablesReference":0}]}})"), error)) return 12;
    if (s.variables().size() != 1 || s.variables()[0].name != "n" || !s.resume("next", error) || s.takeOutbound().find("\"next\"") == std::string::npos) return 13;
    if (s.resume("continue", error)) return 14;
    if (!s.receive(frame(R"({"seq":7,"type":"event","event":"stopped","body":{"reason":"step","threadId":42}})"), error)) return 15;
    (void)s.takeOutbound();
    if (!s.receive(frame(R"({"seq":8,"type":"response","request_seq":9,"command":"stackTrace","success":true,"body":{"stackFrames":[{"id":8,"name":"main","line":4,"column":1,"source":{"path":"/tmp/a.cpp"}}]}})"), error)) return 16;
    (void)s.takeOutbound();
    if (!s.receive(frame(R"({"seq":9,"type":"response","request_seq":10,"command":"scopes","success":true,"body":{"scopes":[{"variablesReference":9}]}})"), error)) return 17;
    (void)s.takeOutbound();
    if (!s.receive(frame(R"({"seq":10,"type":"response","request_seq":11,"command":"variables","success":true,"body":{"variables":[{"name":"n","value":"8","type":"int","variablesReference":44}]}})"), error) || !s.variables()[0].changed) return 18;
    if (!s.expandVariable(0, error) || !s.expandedVariables() || s.takeOutbound().find("44") == std::string::npos) return 19;
    if (!s.receive(frame(R"({"seq":11,"type":"response","request_seq":12,"command":"variables","success":true,"body":{"variables":[{"name":"child","value":"x","variablesReference":0}]}})"), error) || s.variables()[0].name != "child") return 20;
    if (!s.selectFrame(0, error) || s.expandedVariables() || s.takeOutbound().find("scopes") == std::string::npos) return 21;
    Session multiple;
    multiple.start("/tmp/program", "/tmp", std::map<std::string, std::vector<int>>{{"/tmp/one.cpp", {2}}, {"/tmp/two.cpp", {7}}});
    (void)multiple.takeOutbound();
    if (!multiple.receive(frame(R"({"type":"response","request_seq":1,"success":true})"), error)) return 23;
    (void)multiple.takeOutbound();
    if (!multiple.receive(frame(R"({"type":"event","event":"initialized"})"), error)) return 24;
    const auto multiRequests = multiple.takeOutbound();
    if (multiRequests.find("/tmp/one.cpp") == std::string::npos || multiRequests.find("/tmp/two.cpp") == std::string::npos) return 25;
    if (!multiple.setBreakpoints("/tmp/two.cpp", {9}, error) || multiple.takeOutbound().find("\"line\":9") == std::string::npos) return 26;
    if (!multiple.receive(frame(R"({"type":"response","request_seq":3,"success":true,"body":{"breakpoints":[{"verified":false,"message":"Missing symbols"}]}})"), error)) return 27;
    if (!multiple.receive(frame(R"({"type":"response","request_seq":6,"success":true,"body":{"breakpoints":[{"verified":true}]}})"), error) || multiple.breakpointWarning().find("Missing symbols") == std::string::npos) return 28;
    if (!multiple.receive(frame(R"({"type":"response","request_seq":4,"success":true,"body":{"breakpoints":[{"verified":false,"message":"Outdated"}]}})"), error) || multiple.breakpointWarning().find("Outdated") != std::string::npos) return 29;
    if (!multiple.receive(frame(R"({"type":"event","event":"stopped","body":{"reason":"breakpoint","threadId":1}})"), error)) return 30;
    if (!multiple.receive(frame(R"({"type":"event","event":"terminated"})"), error) || multiple.state() != State::Ended || !multiple.stack().empty() || !multiple.variables().empty()) return 31;
    if (!multiple.receive(frame(R"({"type":"event","event":"stopped","body":{"reason":"late","threadId":1}})"), error) || multiple.state() != State::Ended) return 32;
    Session disconnected;
    disconnected.start("/tmp/program", "/tmp", "/tmp/file.cpp", {});
    (void)disconnected.takeOutbound();
    if (!disconnected.receive(frame(R"({"type":"event","event":"stopped","body":{"reason":"pause","threadId":7}})"), error)) return 33;
    disconnected.transportClosed("Adapter disconnected", false);
    if (disconnected.state() != State::Failed || disconnected.message() != "Adapter disconnected" ||
        !disconnected.stack().empty() || !disconnected.variables().empty() || !disconnected.takeOutbound().empty()) return 34;
    if (disconnected.resume("continue", error)) return 35;
    Session malformed;
    malformed.start("/tmp/program", "/tmp", "/tmp/file.cpp", {});
    (void)malformed.takeOutbound();
    if (!malformed.receive(frame(R"({"type":"event","event":"stopped","body":{"reason":"pause","threadId":7}})"), error)) return 36;
    if (malformed.receive(frame("{invalid"), error) || malformed.state() != State::Failed ||
        error.empty() || malformed.message() != error || !malformed.stack().empty() ||
        !malformed.variables().empty() || !malformed.takeOutbound().empty()) return 37;
    Session rejected;
    rejected.start("/tmp/program", "/tmp", "/tmp/file.cpp", {});
    (void)rejected.takeOutbound();
    if (!rejected.receive(frame(R"({"type":"event","event":"stopped","body":{"reason":"pause","threadId":7}})"), error)) return 38;
    (void)rejected.takeOutbound();
    if (!rejected.resume("next", error)) return 39;
    (void)rejected.takeOutbound();
    if (!rejected.receive(frame(R"({"type":"response","request_seq":3,"success":false,"message":"Step rejected"})"), error) ||
        rejected.state() != State::Stopped || !rejected.messageIsError() || rejected.message().find("Step rejected") == std::string::npos ||
        rejected.takeOutbound().find("stackTrace") == std::string::npos) return 40;
    Session breakpointRetry;
    breakpointRetry.start("/tmp/program", "/tmp", "/tmp/file.cpp", {3});
    (void)breakpointRetry.takeOutbound();
    if (!breakpointRetry.receive(frame(R"({"type":"event","event":"initialized"})"), error)) return 41;
    (void)breakpointRetry.takeOutbound();
    if (!breakpointRetry.receive(frame(R"({"type":"response","request_seq":2,"success":false,"message":"Unavailable"})"), error) ||
        breakpointRetry.breakpointWarning().find("Unavailable") == std::string::npos || breakpointRetry.messageIsError()) return 42;
    if (!breakpointRetry.setBreakpoints("/tmp/file.cpp", {3}, error)) return 43;
    (void)breakpointRetry.takeOutbound();
    if (!breakpointRetry.receive(frame(R"({"type":"response","request_seq":4,"success":true,"body":{"breakpoints":[{"verified":true}]}})"), error) ||
        !breakpointRetry.breakpointWarning().empty() || breakpointRetry.messageIsError()) return 44;
    return 0;
}
