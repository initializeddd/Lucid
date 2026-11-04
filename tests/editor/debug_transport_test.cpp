#include "debug/AdapterProcess.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
int main(int argc, char** argv) {
    if (argc != 2) return 1;
    lucid::debug::AdapterProcess process; std::string error, result, chunk;
    if (process.start("lucid-nonexistent-debug-adapter", {}, error) || error.empty()) { std::cerr << "Missing adapter was accepted"; return 2; }
    if (!process.start(argv[1], {}, error)) { std::cerr << error; return 3; }
    std::string sent(256U * 1024U, 'a');
    for (std::size_t i = 0; i < sent.size(); ++i) sent[i] = static_cast<char>(' ' + i % 90);
    if (!process.send(sent, error)) { std::cerr << "Send failed: " << error; return 4; }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (result.size() < sent.size() && std::chrono::steady_clock::now() < deadline) {
        if (!process.poll(chunk, error)) { std::cerr << error; return 5; }
        result += chunk; std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (result != sent) { std::cerr << "Transport lost or reordered bytes: " << result.size(); return 6; }
    process.close();
    if (process.alive() || process.send("x", error)) { std::cerr << "Closed adapter accepted input"; return 7; }
    if (!process.start(argv[1], {}, error)) { std::cerr << "Restart failed: " << error; return 8; }
    process.close();
    if (process.alive()) return 9;
#if defined(_WIN32)
    if (!process.start(argv[1], {"--diagnostic-failure"}, error)) return 10;
    const auto failureDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (process.poll(chunk, error) && std::chrono::steady_clock::now() < failureDeadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (error.find("fixture diagnostic failure") == std::string::npos || error.find("42") == std::string::npos) {
        std::cerr << "Missing adapter diagnostics: " << error; return 11;
    }
#endif
    return 0;
}
