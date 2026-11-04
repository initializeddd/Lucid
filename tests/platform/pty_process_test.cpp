#include "platform/terminal/PtyProcess.hpp"
#include "editor/compile/CompileScript.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <unordered_map>

int main() {
#if defined(_WIN32)
    // Compile terminals are not implemented on Windows yet.
    return 0;
#else
    lucid::platform::PtyProcess process;
    std::string error;
    if (!process.start(".", {"printf 'hello\\n'", "false", "printf 'world\\n'", "printf '\\303\\251\\bX\\n'"}, error)) {
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

    const auto fail = [&](int code) {
        std::fprintf(stderr, "pty test failed with code %d\n", code);
        std::fprintf(stderr, "exit codes:");
        for (const int value : exitCodes) std::fprintf(stderr, " %d", value);
        std::fprintf(stderr, "\noutput:\n%s\n", output.c_str());
        return code;
    };

    if (output.find("hello") == std::string::npos) {
        return fail(2);
    }
    if (output.find("world") == std::string::npos) {
        return fail(3);
    }
    if (output.find("__LUCID_DONE_") != std::string::npos) {
        return fail(4);
    }
    if (output.find('\r') != std::string::npos ||
        output.find('\x1b') != std::string::npos) {
        return fail(5);
    }

    if (exitCodes != std::vector<int>{0, 1, 0, 0}) return fail(6);
    if (output.find("[Command 2 failed with exit code 1]") == std::string::npos) return fail(24);
    if (output.find("[Command 1 finished in ") == std::string::npos ||
        output.find(" ms; exit code 0]") == std::string::npos ||
        output.find(" ms; exit code 1]") == std::string::npos) return fail(25);
    if (output.find("\xc3\xa9") != std::string::npos || output.find("X\n") == std::string::npos ||
        output.find('\xc3') != std::string::npos) return fail(7);
    if (!process.start(".", {"awk 'BEGIN { for (i=0; i<33001; i++) printf \"\\303\\251\"; printf \"\\n\" }'"}, error)) return 8;
    for (int attempt = 0; attempt < 600 && !process.commandsDone(); ++attempt) {
        process.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    const std::string longOutput(process.output());
    const bool done = process.commandsDone();
    process.close();
    if (!done || longOutput.size() < 64000 || (static_cast<unsigned char>(longOutput.front()) & 0xc0U) == 0x80U) {
        std::fprintf(stderr, "long output failed: done=%d size=%zu first=0x%02x\n", done ? 1 : 0,
                     longOutput.size(), longOutput.empty() ? 0 : static_cast<unsigned char>(longOutput.front()));
        return 9;
    }
    const std::string path = "/tmp/a b'\"$HOME`bad`\\folder";
    const std::unordered_map<std::string, std::string> vars{{"project", path}};
    const auto expand = [&vars](std::string_view command) { return lucid::editor::expandCompileShellVariables(command, vars); };
    if (!process.start(".", {expand("printf '<%s>\\n' %project%"),
                              expand("printf '<%s>\\n' \"%project%\""),
                              expand("printf '<%s>\\n' '%project%'"),
                              expand("printf '<%s>\\n' \"$(printf '%s' %project%)\"")}, error)) return 10;
    for (int attempt = 0; attempt < 600 && !process.commandsDone(); ++attempt) {
        process.update(); std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    const std::string quotedOutput(process.output());
    const auto quotedCodes = process.exitCodes();
    const bool quotedDone = process.commandsDone(); process.close();
    const auto expected = "<" + path + ">";
    std::size_t count = 0, pos = 0;
    while ((pos = quotedOutput.find(expected, pos)) != std::string::npos) { ++count; pos += expected.size(); }
    if (!quotedDone || count != 4 || quotedCodes != std::vector<int>{0, 0, 0, 0}) return 11;
    if (!process.start("/lucid-test-directory-that-does-not-exist", {"printf 'should-not-run\\n'"}, error)) return 12;
    for (int attempt = 0; attempt < 600 && !process.commandsDone(); ++attempt) {
        process.update(); std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    const std::string failedOutput(process.output());
    const auto failedCodes = process.exitCodes();
    const bool failedDone = process.commandsDone(); process.close();
    if (!failedDone || failedOutput.find("cannot enter the project directory") == std::string::npos ||
        failedOutput.find("\nshould-not-run\n") != std::string::npos ||
        failedOutput.find("exited before reporting") == std::string::npos || failedCodes != std::vector<int>{-1}) return 13;
    if (!process.start(".", {"printf 'READY\\n'; wc -c"}, error)) return 14;
    for (int attempt = 0; attempt < 600 && process.output().find("READY\n") == std::string_view::npos; ++attempt) {
        process.update(); std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (process.output().find("READY\n") == std::string_view::npos) return 15;
    std::string input;
    for (int i = 0; i < 1000; ++i) input += std::string(99, 'x') + "\n";
    process.write(input); process.write("\x04");
    for (int attempt = 0; attempt < 1200 && !process.commandsDone(); ++attempt) {
        process.update(); std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    const auto inputOutput = std::string(process.output());
    const auto inputCodes = process.exitCodes();
    const bool inputDone = process.commandsDone(); process.close();
    if (!inputDone || inputOutput.find("100000") == std::string::npos || inputCodes != std::vector<int>{0}) return 16;
    if (!process.start(".", {"printf 'Loading %d%%\\rLoading %d%%\\rComplete\\nPreserved\\r\\n' 10 20"}, error)) return 17;
    for (int attempt = 0; attempt < 600 && !process.commandsDone(); ++attempt) {
        process.update(); std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    const auto progress = std::string(process.output());
    const bool progressDone = process.commandsDone(); process.close();
    if (!progressDone || progress.find("Complete\nPreserved\n") == std::string::npos ||
        progress.find("Loading 10%") != std::string::npos || progress.find("Loading 20%") != std::string::npos) return 18;
    if (!process.start(".", {"printf 'commented\\n' # explanation", "printf 'semicolon\\n';"}, error)) return 19;
    for (int attempt = 0; attempt < 600 && !process.commandsDone(); ++attempt) {
        process.update(); std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    const auto syntaxOutput = std::string(process.output());
    const auto syntaxCodes = process.exitCodes();
    const bool syntaxDone = process.commandsDone(); process.close();
    if (!syntaxDone || syntaxOutput.find("commented\n") == std::string::npos ||
        syntaxOutput.find("semicolon\n") == std::string::npos || syntaxCodes != std::vector<int>{0, 0}) return 20;
    if (!process.start(".", {"unset LUCID_DONE_MARKER", "printf() { return 9; }", "true"}, error)) return 25;
    for (int attempt = 0; attempt < 600 && !process.commandsDone(); ++attempt) {
        process.update(); std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    const bool alteredDone = process.commandsDone();
    const auto alteredCodes = process.exitCodes(); process.close();
    if (!alteredDone || alteredCodes != std::vector<int>{0, 0, 0}) return 26;
    const auto marker = std::filesystem::temp_directory_path() /
        ("lucid-pty-child-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto command = "sh -c 'trap \"\" HUP; printf \"CHILD-READY\\n\"; sleep 1; printf alive > \"$1\"' sh " +
        lucid::editor::shellQuote(marker.string());
    if (!process.start(".", {command}, error)) return 21;
    for (int attempt = 0; attempt < 600 && process.output().find("CHILD-READY\n") == std::string_view::npos; ++attempt) {
        process.update(); std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (process.output().find("CHILD-READY\n") == std::string_view::npos) return 22;
    process.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(1300));
    if (std::filesystem::exists(marker)) { std::filesystem::remove(marker); return 23; }
    if (!process.start(".", {"printf 'lucid-clean-output\\n'"}, error)) return 27;
    for (int attempt = 0; attempt < 600 && !process.commandsDone(); ++attempt) {
        process.update(); std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    const auto cleanOutput = std::string(process.output());
    const bool cleanDone = process.commandsDone(); process.close();
    if (!cleanDone || !cleanOutput.starts_with("lucid-clean-output\n") ||
        cleanOutput.find("[Command 1 finished in ") == std::string::npos ||
        cleanOutput.find(" ms; exit code 0]") == std::string::npos) return 28;
    return 0;
#endif
}
