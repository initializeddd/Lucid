#include "workspace/FunctionIndex.hpp"

#include "editor/document/Document.hpp"
#include "editor/json/Json.hpp"
#include "workspace/Project.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lucid::workspace {
namespace {

constexpr std::string_view kCachePath = ".lucid/cache/functions.json";
constexpr std::uintmax_t kMaxIndexedFileSize = 2U * 1024U * 1024U;
constexpr std::size_t kTextProbeSize = 4096;

struct FileSnapshot {
    std::string path;
    std::uintmax_t size = 0;
    std::int64_t modified = 0;
    std::vector<ProjectFunction> functions;
};

struct FunctionCache {
    std::vector<FileSnapshot> files;
};

[[nodiscard]] std::string lowerAscii(std::string text) {
    for (char& character : text) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    }
    return text;
}

[[nodiscard]] std::string jsonEscape(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size() + 8);
    for (const char character : text) {
        switch (character) {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(character) < 0x20U) {
                escaped += "\\u00";
                constexpr char kHex[] = "0123456789abcdef";
                escaped.push_back(kHex[(static_cast<unsigned char>(character) >> 4U) & 0xFU]);
                escaped.push_back(kHex[static_cast<unsigned char>(character) & 0xFU]);
            } else {
                escaped.push_back(character);
            }
            break;
        }
    }
    return escaped;
}

[[nodiscard]] std::string platformKey(editor::CompilePlatform platform) {
    switch (platform) {
    case editor::CompilePlatform::MacOS:
        return "macos";
    case editor::CompilePlatform::Windows:
        return "windows";
    case editor::CompilePlatform::Linux:
        return "linux";
    case editor::CompilePlatform::FreeBSD:
        return "freebsd";
    case editor::CompilePlatform::OpenBSD:
        return "openbsd";
    case editor::CompilePlatform::Any:
    default:
        return "any";
    }
}

[[nodiscard]] editor::CompilePlatform parsePlatform(std::string_view text) noexcept {
    const std::string lower = lowerAscii(std::string(text));
    if (lower == "macos") {
        return editor::CompilePlatform::MacOS;
    }
    if (lower == "windows") {
        return editor::CompilePlatform::Windows;
    }
    if (lower == "linux") {
        return editor::CompilePlatform::Linux;
    }
    if (lower == "freebsd") return editor::CompilePlatform::FreeBSD;
    if (lower == "openbsd") return editor::CompilePlatform::OpenBSD;
    return editor::CompilePlatform::Any;
}

[[nodiscard]] bool isText(std::string_view content) {
    const std::size_t probe = std::min(content.size(), kTextProbeSize);
    for (std::size_t index = 0; index < probe; ++index) {
        if (content[index] == '\0') {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool cacheNumber(const editor::JsonValue& object, std::string_view key,
                               double& value) {
    const editor::JsonValue* member = object.find(key);
    if (member == nullptr || member->type != editor::JsonType::Number) {
        return false;
    }
    value = member->numberValue;
    return true;
}

[[nodiscard]] std::string_view cacheString(const editor::JsonValue& object,
                                           std::string_view key) {
    const editor::JsonValue* member = object.find(key);
    if (member == nullptr || member->type != editor::JsonType::String) {
        return {};
    }
    return member->stringValue;
}

void appendFunction(std::string& output, const ProjectFunction& function, bool last) {
    output += "{\"name\":\"";
    output += jsonEscape(function.name);
    output += "\",\"platform\":\"";
    output += platformKey(function.platform);
    output += "\",\"line\":";
    output += std::to_string(function.line);
    output += ",\"endLine\":";
    output += std::to_string(function.endLine);
    output += ",\"directiveColumn\":";
    output += std::to_string(function.directiveColumn);
    output += ",\"directiveLength\":";
    output += std::to_string(function.directiveLength);
    output += ",\"commands\":[";
    for (std::size_t index = 0; index < function.commands.size(); ++index) {
        if (index > 0) {
            output += ',';
        }
        output += "{\"text\":\"";
        output += jsonEscape(function.commands[index].text);
        output += "\",\"line\":";
        output += std::to_string(function.commands[index].line);
        output += '}';
    }
    output += "]}";
    if (!last) {
        output += ',';
    }
}

[[nodiscard]] std::string serializeCache(const std::vector<FileSnapshot>& files) {
    std::string output = "{\"version\":1,\"files\":[";
    for (std::size_t fileIndex = 0; fileIndex < files.size(); ++fileIndex) {
        const FileSnapshot& file = files[fileIndex];
        if (fileIndex > 0) {
            output += ',';
        }
        output += "{\"path\":\"";
        output += jsonEscape(file.path);
        output += "\",\"size\":";
        output += std::to_string(file.size);
        output += ",\"mtime\":\"";
        output += std::to_string(file.modified);
        output += "\",\"functions\":[";
        for (std::size_t functionIndex = 0; functionIndex < file.functions.size();
             ++functionIndex) {
            appendFunction(output, file.functions[functionIndex],
                           functionIndex + 1 == file.functions.size());
        }
        output += "]}";
    }
    output += "]}";
    return output;
}

[[nodiscard]] bool deserializeCache(std::string_view text, FunctionCache& cache) {
    editor::JsonError jsonError;
    const std::optional<editor::JsonValue> root = editor::parseJson(text, jsonError);
    if (!root.has_value() || root->type != editor::JsonType::Object) {
        return false;
    }

    double version = 0.0;
    if (!cacheNumber(*root, "version", version) || version != 1.0) {
        return false;
    }
    const editor::JsonValue* files = root->find("files");
    if (files == nullptr || files->type != editor::JsonType::Array) {
        return false;
    }

    cache.files.clear();
    cache.files.reserve(files->children.size());
    for (const editor::JsonValue& fileValue : files->children) {
        if (fileValue.type != editor::JsonType::Object) {
            continue;
        }
        const std::string_view path = cacheString(fileValue, "path");
        const std::string_view modified = cacheString(fileValue, "mtime");
        double size = 0.0;
        if (path.empty() || modified.empty() ||
            !cacheNumber(fileValue, "size", size) || size < 0.0) {
            continue;
        }

        FileSnapshot file;
        file.path = std::string(path);
        file.size = static_cast<std::uintmax_t>(size);
        try {
            file.modified = std::stoll(std::string(modified));
        } catch (...) {
            continue;
        }

        const editor::JsonValue* functions = fileValue.find("functions");
        if (functions != nullptr && functions->type == editor::JsonType::Array) {
            for (const editor::JsonValue& functionValue : functions->children) {
                if (functionValue.type != editor::JsonType::Object) {
                    continue;
                }
                ProjectFunction function;
                function.name = std::string(cacheString(functionValue, "name"));
                if (function.name.empty()) {
                    continue;
                }
                function.file = file.path;
                function.platform = parsePlatform(cacheString(functionValue, "platform"));
                double line = 0.0;
                double endLine = 0.0;
                double column = 0.0;
                double length = 0.0;
                if (cacheNumber(functionValue, "line", line)) {
                    function.line = static_cast<int>(line);
                }
                if (cacheNumber(functionValue, "endLine", endLine)) {
                    function.endLine = static_cast<int>(endLine);
                }
                if (cacheNumber(functionValue, "directiveColumn", column)) {
                    function.directiveColumn = static_cast<int>(column);
                }
                if (cacheNumber(functionValue, "directiveLength", length)) {
                    function.directiveLength = static_cast<int>(length);
                }
                const editor::JsonValue* commands = functionValue.find("commands");
                if (commands != nullptr && commands->type == editor::JsonType::Array) {
                    for (const editor::JsonValue& commandValue : commands->children) {
                        if (commandValue.type != editor::JsonType::Object) {
                            continue;
                        }
                        editor::CompileCommand command;
                        command.text = std::string(cacheString(commandValue, "text"));
                        double commandLine = 0.0;
                        if (cacheNumber(commandValue, "line", commandLine)) {
                            command.line = static_cast<int>(commandLine);
                        }
                        if (!command.text.empty()) {
                            function.commands.push_back(std::move(command));
                        }
                    }
                }
                file.functions.push_back(std::move(function));
            }
        }
        cache.files.push_back(std::move(file));
    }
    return true;
}

[[nodiscard]] bool fileStamp(const Project& project, const std::string& path,
                             std::uintmax_t& size, std::int64_t& modified) {
    std::string error;
    const std::filesystem::path absolute = project.resolve(path, error);
    if (!error.empty()) {
        return false;
    }
    std::error_code filesystemError;
    size = std::filesystem::file_size(absolute, filesystemError);
    if (filesystemError) {
        return false;
    }
    const auto writeTime = std::filesystem::last_write_time(absolute, filesystemError);
    if (filesystemError) {
        return false;
    }
    modified = static_cast<std::int64_t>(writeTime.time_since_epoch().count());
    return true;
}

[[nodiscard]] int subsequenceScore(std::string_view text, std::string_view query) {
    std::size_t queryIndex = 0;
    int score = 0;
    int gap = 0;
    for (std::size_t index = 0; index < text.size() && queryIndex < query.size();
         ++index) {
        if (text[index] == query[queryIndex]) {
            ++queryIndex;
            score += 8 - std::min(gap, 6);
            gap = 0;
        } else {
            ++gap;
        }
    }
    return queryIndex == query.size() ? score : -1;
}

[[nodiscard]] int functionScore(const ProjectFunction& function,
                                std::string_view query) {
    if (query.empty()) {
        return 1;
    }
    const std::string name = lowerAscii(function.name);
    const std::string file = lowerAscii(function.file);
    if (name == query) {
        return 1000;
    }
    if (name.starts_with(query)) {
        return 900 - static_cast<int>(std::min<std::size_t>(name.size(), 100));
    }
    const std::size_t nameMatch = name.find(query);
    if (nameMatch != std::string::npos) {
        return 760 - static_cast<int>(std::min<std::size_t>(nameMatch, 100));
    }
    const std::size_t fileMatch = file.find(query);
    if (fileMatch != std::string::npos) {
        return 420 - static_cast<int>(std::min<std::size_t>(fileMatch, 100));
    }
    const int fuzzyName = subsequenceScore(name, query);
    if (fuzzyName >= 0) {
        return 260 + fuzzyName;
    }
    const int fuzzyFile = subsequenceScore(file, query);
    return fuzzyFile >= 0 ? 80 + fuzzyFile : -1;
}

} // namespace

bool ProjectFunction::runsHere() const noexcept {
    return platform == editor::CompilePlatform::Any ||
           platform == editor::currentCompilePlatform();
}

std::string_view functionPlatformName(editor::CompilePlatform platform) noexcept {
    switch (platform) {
    case editor::CompilePlatform::MacOS:
        return "macOS";
    case editor::CompilePlatform::Windows:
        return "Windows";
    case editor::CompilePlatform::Linux:
        return "Linux";
    case editor::CompilePlatform::FreeBSD:
        return "FreeBSD";
    case editor::CompilePlatform::OpenBSD:
        return "OpenBSD";
    case editor::CompilePlatform::Any:
    default:
        return {};
    }
}

FunctionIndex loadFunctionIndex(Project& project, std::string& error) {
    error.clear();
    FunctionIndex index;
    FunctionCache cached;
    bool cacheValid = false;

    std::string cacheText;
    std::string readError;
    if (project.readFile(kCachePath, cacheText, readError) &&
        deserializeCache(cacheText, cached)) {
        cacheValid = true;
    }

    std::vector<FileSnapshot> files;
    files.reserve(project.files().size());
    bool changed = !cacheValid;

    for (const std::string& relative : project.files()) {
        std::uintmax_t size = 0;
        std::int64_t modified = 0;
        if (!fileStamp(project, relative, size, modified)) {
            changed = true;
            continue;
        }

        const auto cachedFile = std::find_if(
            cached.files.begin(), cached.files.end(),
            [&relative](const FileSnapshot& file) { return file.path == relative; });
        if (cachedFile != cached.files.end() && cachedFile->size == size &&
            cachedFile->modified == modified) {
            FileSnapshot snapshot = *cachedFile;
            index.functions.insert(index.functions.end(), snapshot.functions.begin(),
                                   snapshot.functions.end());
            files.push_back(std::move(snapshot));
            continue;
        }

        changed = true;
        FileSnapshot snapshot;
        snapshot.path = relative;
        snapshot.size = size;
        snapshot.modified = modified;

        std::string content;
        if (size <= kMaxIndexedFileSize &&
            project.readFile(relative, content, readError) && isText(content)) {
            const editor::Document document = editor::Document::fromUtf8(content);
            const editor::CompileScript script =
                editor::parseCompileScript(document.lines());
            snapshot.functions.reserve(script.functions.size());
            for (const editor::CompileFunction& function : script.functions) {
                ProjectFunction indexed;
                indexed.name = function.name;
                indexed.file = relative;
                indexed.platform = function.platform;
                indexed.line = function.definitionLine;
                indexed.endLine = function.endLine;
                indexed.directiveColumn = function.directiveColumn;
                indexed.directiveLength = function.directiveLength;
                indexed.commands = function.commands;
                snapshot.functions.push_back(std::move(indexed));
            }
        }

        index.functions.insert(index.functions.end(), snapshot.functions.begin(),
                               snapshot.functions.end());
        files.push_back(std::move(snapshot));
    }

    if (changed) {
        std::string writeError;
        if (!project.writeFile(kCachePath, serializeCache(files), writeError) &&
            error.empty()) {
            error = writeError;
        }
    }

    std::sort(index.functions.begin(), index.functions.end(),
              [](const ProjectFunction& left, const ProjectFunction& right) {
                  if (left.file != right.file) {
                      return left.file < right.file;
                  }
                  if (left.line != right.line) {
                      return left.line < right.line;
                  }
                  return left.name < right.name;
              });
    index.usedCache = !changed;
    return index;
}

std::vector<std::size_t> searchFunctions(const std::vector<ProjectFunction>& functions,
                                         std::string_view query, std::size_t limit) {
    if (limit == 0) {
        return {};
    }
    const std::string normalized = lowerAscii(std::string(query));
    std::vector<std::pair<int, std::size_t>> ranked;
    ranked.reserve(functions.size());
    for (std::size_t index = 0; index < functions.size(); ++index) {
        const int score = functionScore(functions[index], normalized);
        if (score >= 0) {
            ranked.emplace_back(score, index);
        }
    }

    const auto count = std::min(limit, ranked.size());
    std::partial_sort(ranked.begin(), ranked.begin() + static_cast<std::ptrdiff_t>(count), ranked.end(),
              [&functions](const auto& left, const auto& right) {
                  if (left.first != right.first) {
                      return left.first > right.first;
                  }
                  const ProjectFunction& leftFunction = functions[left.second];
                  const ProjectFunction& rightFunction = functions[right.second];
                  if (leftFunction.name != rightFunction.name) {
                      return leftFunction.name < rightFunction.name;
                  }
                  if (leftFunction.file != rightFunction.file) return leftFunction.file < rightFunction.file;
                  if (leftFunction.line != rightFunction.line) return leftFunction.line < rightFunction.line;
                  return left.second < right.second;
              });

    std::vector<std::size_t> result;
    result.reserve(count);
    for (const auto& entry : ranked) {
        if (result.size() >= count) {
            break;
        }
        result.push_back(entry.second);
    }
    return result;
}

} // namespace lucid::workspace
