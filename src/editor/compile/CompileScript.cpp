#include "editor/compile/CompileScript.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace lucid::editor {
namespace {

[[nodiscard]] bool isAsciiSpace(char32_t value) noexcept {
    return value == U' ' || value == U'\t' || value == U'\r';
}

[[nodiscard]] std::u32string_view trim(std::u32string_view text) {
    while (!text.empty() && isAsciiSpace(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && isAsciiSpace(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

void appendUtf8(std::string& output, char32_t codepoint) {
    if (codepoint <= 0x7F) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

[[nodiscard]] std::string toUtf8(std::u32string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const char32_t codepoint : text) {
        appendUtf8(result, codepoint);
    }
    return result;
}

[[nodiscard]] std::string lowerAscii(std::string text) {
    for (char& character : text) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    }
    return text;
}

[[nodiscard]] bool startsWith(std::u32string_view text, std::u32string_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

void finishFunction(std::vector<CompileFunction>& functions, CompileFunction& active,
                    int endLine) {
    active.endLine = std::max(active.definitionLine, endLine);
    functions.push_back(std::move(active));
    active = CompileFunction{};
}

} // namespace

CompilePlatform currentCompilePlatform() noexcept {
#if defined(__APPLE__)
    return CompilePlatform::MacOS;
#elif defined(_WIN32)
    return CompilePlatform::Windows;
#elif defined(__linux__)
    return CompilePlatform::Linux;
#else
    return CompilePlatform::Any;
#endif
}

CompilePlatform parseCompilePlatform(std::string_view text) noexcept {
    const std::string platform = lowerAscii(std::string(text));
    if (platform == "macos" || platform == "mac" || platform == "osx" ||
        platform == "darwin") {
        return CompilePlatform::MacOS;
    }
    if (platform == "windows" || platform == "win" || platform == "win32") {
        return CompilePlatform::Windows;
    }
    if (platform == "linux") {
        return CompilePlatform::Linux;
    }
    return CompilePlatform::Any;
}

bool CompileFunction::runsHere() const noexcept {
    return platform == CompilePlatform::Any ||
           platform == currentCompilePlatform();
}

CompileScript parseCompileScript(const std::vector<std::u32string>& lines) {
    CompileScript script;
    bool inBlockComment = false;
    bool hasActiveFunction = false;
    CompileFunction active;

    for (std::size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const std::u32string& line = lines[lineIndex];
        std::size_t index = 0;

        while (index <= line.size()) {
            if (!inBlockComment) {
                const std::size_t open = line.find(U"/*", index);
                if (open == std::u32string::npos) {
                    break;
                }
                index = open + 2;
                inBlockComment = true;
            }

            const std::size_t close = line.find(U"*/", index);
            const std::size_t end = close == std::u32string::npos ? line.size() : close;
            std::u32string_view content(line.data() + index, end - index);
            const std::u32string_view trimmed = trim(content);
            std::size_t leading = 0;
            while (leading < content.size() && isAsciiSpace(content[leading])) {
                ++leading;
            }
            const int columnOffset = static_cast<int>(index + leading);

            if (!trimmed.empty() && trimmed.front() == U'@') {
                if (startsWith(trimmed, U"@end")) {
                    if (hasActiveFunction) {
                        finishFunction(script.functions, active,
                                       static_cast<int>(lineIndex));
                        hasActiveFunction = false;
                    }
                } else {
                    if (hasActiveFunction) {
                        finishFunction(script.functions, active,
                                       static_cast<int>(lineIndex) - 1);
                    }

                    CompileFunction function;
                    function.definitionLine = static_cast<int>(lineIndex);
                    function.endLine = static_cast<int>(lineIndex);
                    function.directiveColumn = columnOffset;
                    function.directiveLength = static_cast<int>(trimmed.size());

                    std::u32string_view directive = trimmed.substr(1);
                    const std::size_t colon = directive.find(U':');
                    std::u32string_view name = directive;
                    if (colon != std::u32string_view::npos) {
                        name = directive.substr(0, colon);
                        function.platform =
                            parseCompilePlatform(toUtf8(directive.substr(colon + 1)));
                    }
                    function.directiveLength = static_cast<int>(trimmed.size());
                    function.name = toUtf8(trim(name));
                    if (!function.name.empty()) {
                        active = std::move(function);
                        hasActiveFunction = true;
                    }
                }
            } else if (!trimmed.empty() && trimmed.front() == U'$' &&
                       hasActiveFunction) {
                CompileCommand command;
                command.text = toUtf8(trim(trimmed.substr(1)));
                command.line = static_cast<int>(lineIndex);
                if (!command.text.empty()) {
                    active.commands.push_back(std::move(command));
                }
            }

            if (close == std::u32string::npos) {
                break;
            }
            if (hasActiveFunction) {
                finishFunction(script.functions, active,
                               static_cast<int>(lineIndex));
                hasActiveFunction = false;
            }
            inBlockComment = false;
            index = close + 2;
        }

        if (inBlockComment && hasActiveFunction && index >= line.size()) {
            continue;
        }
    }

    if (hasActiveFunction) {
        finishFunction(script.functions, active,
                       lines.empty() ? 0 : static_cast<int>(lines.size() - 1));
    }

    return script;
}

const CompileFunction* findCompileFunctionAt(const CompileScript& script,
                                             int line) noexcept {
    for (const CompileFunction& function : script.functions) {
        if (line >= function.definitionLine && line <= function.endLine) {
            return &function;
        }
    }
    return nullptr;
}

std::string expandCompileVariables(
    std::string_view command,
    const std::unordered_map<std::string, std::string>& variables) {
    std::string result;
    result.reserve(command.size());

    std::size_t index = 0;
    while (index < command.size()) {
        if (command[index] != '%') {
            result.push_back(command[index]);
            ++index;
            continue;
        }
        const std::size_t close = command.find('%', index + 1);
        if (close == std::string_view::npos) {
            result.append(command.substr(index));
            break;
        }
        const std::string_view name = command.substr(index + 1, close - index - 1);
        const auto found = variables.find(std::string(name));
        if (found != variables.end()) {
            result += found->second;
        } else {
            result.append(command.substr(index, close - index + 1));
        }
        index = close + 1;
    }

    return result;
}

std::string shellQuote(std::string_view value) {
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
}

} // namespace lucid::editor
