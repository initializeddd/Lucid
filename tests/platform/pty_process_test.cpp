#include "platform/terminal/PtyProcess.hpp"

#include <chrono>
#include <string>
#include <thread>

int main() {
    lucid::platform::PtyProcess process;
    std::string error;
    if (!process.start(".", {"printf 'hello\\n'", "printf 'world\\n'"}, error)) {
        return 1;
    }

    for (int attempt = 0; attempt < 600; ++attempt) {
        process.update();
        if (process.output().find("hello") != std::string::npos &&
            process.output().find("world") != std::string::npos) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    const std::string output(process.output());
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

    return 0;
}
