#include "editor/diagnostics/CompilerDiagnostics.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lucid::editor {
namespace {

[[nodiscard]] std::string shellQuote(std::string_view value) {
#if defined(_WIN32)
    std::string quoted = "\"";
    for (const char character : value) {
        if (character == '"') {
            quoted += "\\\"";
        } else {
            quoted.push_back(character);
        }
    }
    quoted.push_back('"');
    return quoted;
#else
    std::string quoted = "'";
    for (const char character : value) {
        if (character == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(character);
        }
    }
    quoted.push_back('\'');
    return quoted;
#endif
}

#if defined(_WIN32)
[[nodiscard]] std::FILE* openCommandPipe(const char* command) {
    return _popen(command, "r");
}

int closeCommandPipe(std::FILE* pipe) { return _pclose(pipe); }
#else
[[nodiscard]] std::FILE* openCommandPipe(const char* command) {
    return popen(command, "r");
}

int closeCommandPipe(std::FILE* pipe) { return pclose(pipe); }
#endif

[[nodiscard]] std::string tempFileName(bool cpp) {
    static std::atomic<std::uint64_t> counter{0};
    const auto stamp = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const std::uint64_t id = stamp + counter.fetch_add(1);
    return "lucid-diag-" + std::to_string(id) + (cpp ? ".cpp" : ".c");
}

[[nodiscard]] std::uint32_t parseRangeLength(const std::string& range,
                                             std::uint64_t line) {
    const std::size_t dash = range.find('-');
    if (dash == std::string::npos) {
        return 1;
    }

    const auto parsePosition = [](std::string_view text, std::uint64_t& parsedLine,
                                  std::uint64_t& parsedColumn) {
        const std::size_t colon = text.find(':');
        if (colon == std::string_view::npos) {
            return false;
        }
        try {
            parsedLine = std::stoull(std::string(text.substr(0, colon)));
            parsedColumn = std::stoull(std::string(text.substr(colon + 1)));
            return true;
        } catch (...) {
            return false;
        }
    };

    std::uint64_t endLine = 0;
    std::uint64_t endColumn = 0;
    if (!parsePosition(std::string_view(range).substr(dash + 1), endLine, endColumn) ||
        endLine != line || endColumn <= 0) {
        return 1;
    }

    std::uint64_t startLine = 0;
    std::uint64_t startColumn = 0;
    if (!parsePosition(std::string_view(range).substr(0, dash), startLine,
                       startColumn) ||
        startColumn == 0 || endColumn <= startColumn) {
        return 1;
    }
    return static_cast<std::uint32_t>(endColumn - startColumn);
}

} // namespace

std::vector<Diagnostic> parseCompilerDiagnostics(std::string_view output,
                                                 std::string_view sourcePath) {
    std::vector<Diagnostic> diagnostics;
    const std::regex pattern(
        R"(^(.+?):([0-9]+):([0-9]+)(?::\{([^}]*)\})?: (fatal error|error|warning|note): (.*)$)");

    std::size_t start = 0;
    while (start <= output.size()) {
        const std::size_t end = output.find('\n', start);
        std::string_view line = output.substr(
            start, end == std::string_view::npos ? std::string_view::npos
                                                 : end - start);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        std::match_results<std::string_view::const_iterator> match;
        if (std::regex_match(line.begin(), line.end(), match, pattern)) {
            const std::string path(match[1].first, match[1].second);
            const std::string severity(match[5].first, match[5].second);
            if (path == sourcePath && (severity == "error" ||
                                       severity == "fatal error")) {
                try {
                    const std::uint64_t lineNumber =
                        std::stoull(std::string(match[2].first, match[2].second));
                    const std::uint64_t column =
                        std::stoull(std::string(match[3].first, match[3].second));
                    if (lineNumber > 0 && column > 0) {
                        std::string message(match[6].first, match[6].second);
                        std::string code;
                        if (message.size() > 4 && message.back() == ']') {
                            const std::size_t open = message.rfind(" [");
                            if (open != std::string::npos) {
                                code = message.substr(open + 2,
                                                      message.size() - open - 3);
                                message.erase(open);
                            }
                        }
                        const std::uint32_t length =
                            match[4].matched
                                ? parseRangeLength(
                                      std::string(match[4].first, match[4].second),
                                      lineNumber)
                                : 1;
                        diagnostics.emplace_back(
                            static_cast<std::uint32_t>(lineNumber - 1),
                            static_cast<std::uint32_t>(column),
                            length > 0 ? length : 1, std::move(message),
                            std::move(code));
                    }
                } catch (...) {
                }
            }
        }

        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }

    return diagnostics;
}

CompilerResult runCompilerDiagnostics(const CompilerRequest& request) {
    CompilerResult result;

    std::error_code filesystemError;
    const std::filesystem::path tempDirectory =
        std::filesystem::temp_directory_path(filesystemError);
    if (filesystemError) {
        result.error = "Could not find a temporary directory";
        return result;
    }

    const std::filesystem::path sourcePath = tempDirectory / tempFileName(request.cpp);
    {
        std::ofstream stream(sourcePath, std::ios::binary | std::ios::trunc);
        if (!stream) {
            result.error = "Could not create compiler input";
            return result;
        }
        stream.write(request.content.data(),
                     static_cast<std::streamsize>(request.content.size()));
        if (!stream) {
            std::filesystem::remove(sourcePath, filesystemError);
            result.error = "Could not write compiler input";
            return result;
        }
    }

    std::string command = request.cpp ? "clang++" : "clang";
    command += request.cpp ? " -x c++ -std=c++20" : " -x c -std=c17";
    command +=
        " -fsyntax-only -ferror-limit=12 -fno-color-diagnostics"
        " -fno-caret-diagnostics -fdiagnostics-print-source-range-info";
    if (!request.projectRoot.empty()) {
        command += " -I " + shellQuote(request.projectRoot);
    }
    const std::filesystem::path sourceDirectory =
        std::filesystem::path(request.filePath).parent_path();
    if (!sourceDirectory.empty() &&
        sourceDirectory.generic_string() != request.projectRoot) {
        command += " -I " + shellQuote(sourceDirectory.generic_string());
    }
    command += " " + shellQuote(sourcePath.generic_string()) + " 2>&1";

    std::FILE* pipe = openCommandPipe(command.c_str());
    if (pipe == nullptr) {
        std::filesystem::remove(sourcePath, filesystemError);
        result.error = "Could not start the compiler";
        return result;
    }

    std::string output;
    char buffer[4096];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    const int status = closeCommandPipe(pipe);
    std::filesystem::remove(sourcePath, filesystemError);

    if (status == -1) {
        result.error = "The compiler did not run";
        return result;
    }
    if (output.find("command not found") != std::string::npos) {
        result.error = request.cpp ? "clang++ was not found"
                                   : "clang was not found";
        return result;
    }

    result.diagnostics =
        parseCompilerDiagnostics(output, sourcePath.generic_string());
    return result;
}

} // namespace lucid::editor
