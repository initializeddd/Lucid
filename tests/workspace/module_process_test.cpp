#include "modules/ModuleProcess.hpp"
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <filesystem>
#include <string>
int main(int argc, char** argv) {
    using namespace lucid::modules;
    if (argc > 1) {
        std::string mode = argv[1];
        if (mode == "args") { if (argc != 3 || std::string(argv[2]) != "hello ; $world") return 3; char c; if (std::cin.get(c)) return 4; std::cout << "ok"; return 0; }
#ifdef _WIN32
        if (mode == "handle") { auto value = static_cast<std::uintptr_t>(std::stoull(argv[2])); DWORD flags = 0; return GetHandleInformation(reinterpret_cast<HANDLE>(value), &flags) ? 5 : 0; }
#endif
        if (mode == "output") { std::cout << std::string(10000, 'x'); return 0; }
        if (mode == "wait") { std::this_thread::sleep_for(std::chrono::seconds(5)); return 0; }
    }
    auto self = std::filesystem::absolute(argv[0]);
    auto r = runModuleProcess(self, {"args", "hello ; $world"}); if (!r.error.empty() || r.exitCode || r.output != "ok") return 1;
    r = runModuleProcess(self, {"output"}, std::chrono::seconds(2), 10); if (r.exitCode || !r.outputTruncated || r.output.size() != 10) return 2;
    r = runModuleProcess(self, {"wait"}, std::chrono::milliseconds(100)); if (!r.timedOut) return 3;
    std::atomic_bool cancel{false};
    std::thread trigger([&] { std::this_thread::sleep_for(std::chrono::milliseconds(50)); cancel.store(true); });
    r = runModuleProcess(self, {"wait"}, std::chrono::seconds(3), 1024, &cancel);
    trigger.join(); if (!r.cancelled || r.timedOut) return 7;
    if (runModuleProcess("relative", {}).error.empty()) return 4;
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE}; HANDLE sentinel = CreateEventW(&sa, TRUE, FALSE, nullptr); if (!sentinel) return 5;
    r = runModuleProcess(self, {"handle", std::to_string(reinterpret_cast<std::uintptr_t>(sentinel))}); CloseHandle(sentinel);
    if (!r.error.empty() || r.exitCode != 0) return 6;
#endif
    return 0;
}
