#include "debug/AdapterProcess.hpp"
#include "debug/Session.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
using namespace lucid::debug;
int main(int argc, char** argv) {
    if (!std::getenv("LUCID_TEST_LLDB")) return 0;
    if (argc != 4) { std::cerr << "Expected fixture sources and program"; return 1; }
    const auto source = std::filesystem::absolute(argv[1]);
    const auto program = std::filesystem::absolute(argv[2]);
    const auto helper = std::filesystem::absolute(argv[3]);
    const auto dir = program.parent_path();
    AdapterProcess process; std::string error;
    if (!process.startDefault(error)) { std::cerr << error; return 2; }
    Session session; session.start(program.string(), dir.string(), std::map<std::string, std::vector<int>>{{source.string(), {4}}, {helper.string(), {2}}});
    bool stopped = false, stepped = false, secondFile = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(25);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto outbound = session.takeOutbound();
        if (!outbound.empty() && !process.send(outbound, error)) { std::cerr << error; return 3; }
        std::string incoming;
        if (!process.poll(incoming, error)) { std::cerr << error << ": " << session.message(); return 4; }
        if (!incoming.empty() && !session.receive(incoming, error)) { std::cerr << error; return 5; }
        if (session.state() == State::Failed) { std::cerr << session.message(); return 6; }
        if (session.state() == State::Stopped && !session.stack().empty() && !session.variables().empty()) {
            if (!stopped) {
                if (session.stack()[0].line != 4) { std::cerr << "Unexpected line " << session.stack()[0].line; return 7; }
                bool valueFound = false;
                for (const auto& variable : session.variables()) if (variable.name == "n" && variable.value.find('7') != std::string::npos) valueFound = true;
                if (!valueFound) { std::cerr << "Expected n = 7 at breakpoint"; return 11; }
                stopped = true; if (!session.resume("next", error)) return 8;
            } else if (!stepped) {
                bool changed = false;
                for (const auto& variable : session.variables()) if (variable.name == "n" && variable.value.find('8') != std::string::npos && variable.changed) changed = true;
                if (!changed) { std::cerr << "Expected n to change to 8 after stepping"; return 12; }
                stepped = true; if (!session.resume("continue", error)) return 8;
            }
            else { secondFile = std::filesystem::path(session.stack()[0].path).filename() == helper.filename() && session.stack()[0].line == 2; break; }
        }
        if (session.state() == State::Ended && !secondFile) { std::cerr << "Ended before breakpoint: " << session.output(); return 9; }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    session.disconnect(); const auto outbound = session.takeOutbound(); (void)process.send(outbound, error); std::string remaining; (void)process.poll(remaining, error);
    process.close();
    if (!stopped || !stepped || !secondFile) { std::cerr << "Timed out: " << session.message(); return 10; }
    return 0;
}
