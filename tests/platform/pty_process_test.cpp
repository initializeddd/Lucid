#include "platform/terminal/PtyProcess.hpp"

#include <chrono>
#include <string>
#include <thread>

int main() {
#if defined(_WIN32)
    // Compile terminals are not implemented on Windows yet.
    return 0;
#else
    lucid::platform::PtyProcess process;
    std::string error;
    if (!process.start(".", {"printf 'hello\\n'", "false", "printf 'world\\n'"}, error)) {
        return 1;
    }

    for (int attempt = 0; attempt < 600; ++attempt) {
        process.update();
        if (process.commandsDone()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    const std::string output(process.output());
    const auto exitCodes = process.exitCodes();
    process.close();

    if (output.find("hello") == std::string::npos) {
        return 2;
    }
    if (output.find("world") == std::string::npos) {
        return 3;
    }
    if (output.find("__LUCID_DONE_") != std::string::npos) {
        return 4;
    }
    if (output.find('\r') != std::string::npos ||
        output.find('\x1b') != std::string::npos) {
        return 5;
    }

    if (exitCodes != std::vector<int>{0, 1, 0}) return 6;
    return 0;
#endif
}
