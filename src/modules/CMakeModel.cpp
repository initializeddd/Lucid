#include "modules/CMakeModel.hpp"
#include "editor/json/Json.hpp"
#include <algorithm>
#include <fstream>
#include <map>
#include <optional>
#include <system_error>

namespace lucid::modules {
namespace {
using editor::JsonType;
const editor::JsonValue* field(const editor::JsonValue& item, const char* name, JsonType type) {
    const auto* result = item.find(name); return result && result->type == type ? result : nullptr;
}
std::optional<editor::JsonValue> readReply(const std::filesystem::path& root, const std::string& name, std::size_t& budget, std::string& error) {
    if (name.empty() || name.size() > 255 || name == "." || name == ".." || name.find_first_of("/\\") != std::string::npos || name.find('\0') != std::string::npos || name.find(':') != std::string::npos) { error = "Unsafe CMake reply filename"; return {}; }
    std::filesystem::path path;
    try { path = root / std::filesystem::path(std::u8string(name.begin(), name.end())); }
    catch (const std::exception&) { error = "Invalid CMake reply filename"; return {}; }
    std::error_code ec;
    if (std::filesystem::symlink_status(path, ec).type() != std::filesystem::file_type::regular || ec) { error = "CMake reply is missing or unsafe"; return {}; }
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > 4U * 1024U * 1024U) { error = "CMake reply exceeds the size limit"; return {}; }
    if (size > budget) { error = "CMake replies exceed the aggregate size limit"; return {}; }
    budget -= static_cast<std::size_t>(size);
    std::ifstream in(path, std::ios::binary); std::string contents(static_cast<std::size_t>(size) + 1, '\0');
    in.read(contents.data(), static_cast<std::streamsize>(contents.size())); const auto count = in.gcount();
    if (in.bad() || count < 0 || static_cast<std::uintmax_t>(count) != size) { error = "CMake reply changed while reading"; return {}; }
    contents.resize(static_cast<std::size_t>(count)); editor::JsonError jsonError;
    auto json = editor::parseJson(contents, jsonError);
    if (!json || json->type != JsonType::Object) { error = "Invalid CMake reply JSON"; return {}; }
    return json;
}
bool strings(const editor::JsonValue& item, const char* name, const char* key, std::vector<std::string>& values) {
    const auto* array = item.find(name); if (!array) return true;
    if (array->type != JsonType::Array || array->children.size() > 10000) return false;
    for (const auto& child : array->children) { const auto* v = field(child, key, JsonType::String); if (!v || v->stringValue.size() > 16384) return false; values.push_back(v->stringValue); }
    return true;
}
}
bool readCMakeModel(const std::filesystem::path& buildDirectory, CMakeModel& model, std::string& error) {
    model = {}; error.clear();
    const auto replies = buildDirectory / ".cmake/api/v1/reply";
    std::error_code ec; std::filesystem::directory_iterator it(replies, ec), end; std::string index; std::size_t entriesSeen = 0; std::size_t budget = 32U * 1024U * 1024U;
    for (; !ec && it != end; it.increment(ec)) {
        if (++entriesSeen > 100000) { error = "CMake reply directory exceeds entry limit"; return false; }
        std::string name;
        try { const auto utf8 = it->path().filename().generic_u8string(); name.assign(utf8.begin(), utf8.end()); }
        catch (const std::exception&) { error = "CMake reply filename cannot be represented as UTF-8"; return false; }
        if (name.starts_with("index-") && name.ends_with(".json") && name > index && it->symlink_status(ec).type() == std::filesystem::file_type::regular) index = name;
    }
    if (ec || index.empty()) { error = "No CMake File API reply; configure the build with a codemodel-v2 query"; return false; }
    const auto indexJson = readReply(replies, index, budget, error); if (!indexJson) return false;
    const auto* objects = field(*indexJson, "objects", JsonType::Array);
    if (!objects || objects->children.size() > 1000) { error = "Invalid CMake reply index"; return false; }
    std::string codemodelFile, cacheFile;
    for (const auto& object : objects->children) {
        const auto* kind = field(object, "kind", JsonType::String); const auto* version = field(object, "version", JsonType::Object);
        const auto* major = version ? field(*version, "major", JsonType::Number) : nullptr; const auto* file = field(object, "jsonFile", JsonType::String);
        if (kind && major && major->numberValue == 2 && file) {
            if (kind->stringValue == "codemodel") codemodelFile = file->stringValue;
            else if (kind->stringValue == "cache") cacheFile = file->stringValue;
        }
    }
    if (codemodelFile.empty()) { error = "CMake codemodel v2 was not requested; configure with a codemodel-v2 query"; return false; }
    const auto code = readReply(replies, codemodelFile, budget, error); if (!code) return false;
    const auto* paths = field(*code, "paths", JsonType::Object); const auto* configs = field(*code, "configurations", JsonType::Array);
    const auto* source = paths ? field(*paths, "source", JsonType::String) : nullptr; const auto* build = paths ? field(*paths, "build", JsonType::String) : nullptr;
    if (!source || !build || !configs || configs->children.size() > 100) { error = "Invalid CMake codemodel"; return false; }
    CMakeModel value; value.sourceDirectory = source->stringValue; value.buildDirectory = build->stringValue;
    const auto requestedBuild = std::filesystem::weakly_canonical(buildDirectory, ec);
    if (ec) { error = "Cannot resolve CMake build directory"; return false; }
    std::filesystem::path reportedBuild;
    try { reportedBuild = std::filesystem::weakly_canonical(std::filesystem::path(std::u8string(value.buildDirectory.begin(), value.buildDirectory.end())), ec); }
    catch (const std::exception&) { error = "Invalid CMake build directory path"; return false; }
    if (ec || requestedBuild != reportedBuild) { error = "CMake reply belongs to a different build directory"; return false; }
    if (!cacheFile.empty()) {
        const auto cache = readReply(replies, cacheFile, budget, error); if (!cache) return false;
        const auto* entries = field(*cache, "entries", JsonType::Array);
        if (!entries || entries->children.size() > 100000) { error = "Invalid CMake cache"; return false; }
        for (const auto& entry : entries->children) {
            const auto* name = field(entry, "name", JsonType::String); const auto* valueField = field(entry, "value", JsonType::String); const auto* type = field(entry, "type", JsonType::String);
            if (!name || !valueField || !type || name->stringValue.size() > 1024 || valueField->stringValue.size() > 16384) { error = "Invalid CMake cache entry"; return false; }
            if (type->stringValue != "BOOL") continue;
            CMakeOption option{name->stringValue, valueField->stringValue, {}};
            if (const auto* properties = entry.find("properties")) {
                if (properties->type != JsonType::Array || properties->children.size() > 1000) { error = "Invalid CMake cache properties"; return false; }
                for (const auto& property : properties->children) {
                    const auto* key = field(property, "name", JsonType::String); const auto* text = field(property, "value", JsonType::String);
                    if (!key || !text || text->stringValue.size() > 16384) { error = "Invalid CMake cache property"; return false; }
                    if (key->stringValue == "HELPSTRING") option.description = text->stringValue;
                }
            }
            value.options.push_back(std::move(option));
        }
        value.optionsAvailable = true;
    }
    for (const auto& config : configs->children) {
        const auto* name = field(config, "name", JsonType::String); const auto* targets = field(config, "targets", JsonType::Array);
        if (!name || !targets || targets->children.size() > 10000 || value.targets.size() + targets->children.size() > 10000) { error = "Invalid or oversized CMake target list"; return false; }
        std::map<std::string,std::string> targetNames;
        for (const auto& reference : targets->children) {
            const auto* id=field(reference,"id",JsonType::String); const auto* targetName=field(reference,"name",JsonType::String);
            if (id && targetName) {
                if(id->stringValue.empty() || id->stringValue.size()>16384 || targetName->stringValue.size()>16384 || !targetNames.emplace(id->stringValue,targetName->stringValue).second) { error="Invalid or duplicate CMake target identity"; return false; }
            }
        }
        for (const auto& reference : targets->children) {
            const auto* filename = field(reference, "jsonFile", JsonType::String); if (!filename) { error = "Missing CMake target reply"; return false; }
            const auto target = readReply(replies, filename->stringValue, budget, error); if (!target) return false;
            const auto* targetName = field(*target, "name", JsonType::String); const auto* type = field(*target, "type", JsonType::String);
            if (!targetName || !type) { error = "Invalid CMake target"; return false; }
            CMakeTarget output; output.configuration = name->stringValue; output.name = targetName->stringValue; output.type = type->stringValue;
            if(const auto* dependencies=target->find("dependencies")) {
                if(dependencies->type!=JsonType::Array || dependencies->children.size()>10000) {error="Invalid CMake dependencies";return false;}
                for(const auto& dependency:dependencies->children) {
                    const auto* id=field(dependency,"id",JsonType::String);
                    if(!id || id->stringValue.empty() || id->stringValue.size()>16384) {error="Invalid CMake dependency";return false;}
                    const auto resolved=targetNames.find(id->stringValue);
                    output.dependencies.push_back(resolved==targetNames.end() ? id->stringValue : resolved->second);
                }
            }
            if(!strings(*target,"artifacts","path",output.artifacts)) {error="Invalid CMake artifacts";return false;}
            if (const auto* groups = target->find("compileGroups")) {
                if (groups->type != JsonType::Array || groups->children.size() > 10000) { error = "Invalid CMake compile groups"; return false; }
                for (const auto& group : groups->children) {
                    CMakeCompileGroup g; const auto* language = field(group, "language", JsonType::String);
                    if (!language || !strings(group, "includes", "path", g.includes) || !strings(group, "defines", "define", g.definitions) || !strings(group, "compileCommandFragments", "fragment", g.flags)) { error = "Invalid CMake compile group"; return false; }
                    g.language = language->stringValue; output.groups.push_back(std::move(g));
                }
            }
            if (const auto* sources = target->find("sources")) {
                if (sources->type != JsonType::Array || sources->children.size() > 100000) { error = "Invalid CMake sources"; return false; }
                for (const auto& item : sources->children) {
                    const auto* path = field(item, "path", JsonType::String); const auto* group = item.find("compileGroupIndex"); const auto* generated = item.find("isGenerated");
                    if (!path || path->stringValue.size() > 16384 || (group && (group->type != JsonType::Number || group->numberValue < 0 || group->numberValue >= output.groups.size() || group->numberValue != static_cast<int>(group->numberValue))) || (generated && generated->type != JsonType::Bool)) { error = "Invalid CMake source"; return false; }
                    output.sources.push_back({path->stringValue, group ? static_cast<int>(group->numberValue) : -1, generated && generated->boolValue});
                }
            }
            value.targets.push_back(std::move(output));
        }
    }
    model = std::move(value); return true;
}
namespace {
bool cachePathSafe(const workspace::Project& project, std::string_view relative, std::string& error) {
    const auto resolved = project.resolve(relative, error);
    if (resolved.empty()) return false;
    try {
        const auto input = std::filesystem::path(std::u8string(relative.begin(), relative.end()));
        if (input.filename() != "CMakeCache.txt") { error = "Expected a CMakeCache.txt path"; return false; }
        std::error_code ec;
        if (std::filesystem::symlink_status(project.root() / input, ec).type() != std::filesystem::file_type::regular || ec) {
            error = "CMake cache is missing, a symlink, or not a regular file"; return false;
        }
    } catch (const std::exception&) { error = "Invalid CMake cache path"; return false; }
    return true;
}
}
bool prepareCMakeBoolCacheUpdate(const workspace::Project& project, std::string_view relativeCachePath,
    std::string_view optionName, std::string_view expectedValue, bool enabled, CMakeCacheUpdate& update, std::string& error) {
    update = {}; error.clear();
    if (optionName.empty() || optionName.size() > 1024 || optionName.find_first_of(":=\r\n\0", 0, 5) != std::string_view::npos ||
        expectedValue.size() > 16384 || expectedValue.find_first_of("\r\n\0", 0, 3) != std::string_view::npos) {
        error = "Invalid CMake option name or value"; return false;
    }
    if (!cachePathSafe(project, relativeCachePath, error)) return false;
    std::string contents;
    if (!project.readFileBounded(relativeCachePath, contents, error, 4U * 1024U * 1024U)) return false;
    std::size_t match = std::string::npos, length = 0, count = 0;
    for (std::size_t start = 0; start < contents.size();) {
        auto end = contents.find('\n', start); if (end == std::string::npos) end = contents.size();
        auto lineEnd = end; if (lineEnd > start && contents[lineEnd-1] == '\r') --lineEnd;
        const std::string_view line(contents.data() + start, lineEnd - start);
        if (!line.starts_with("//") && !line.starts_with('#')) {
            const auto colon = line.find(':'); const auto equals = line.find('=');
            if (colon != std::string_view::npos && line.substr(0, colon) == optionName) {
                ++count;
                if (equals == std::string_view::npos || equals <= colon || line.substr(colon+1, equals-colon-1) != "BOOL" || line.substr(equals+1) != expectedValue) {
                    error = "CMake cache option changed or is not BOOL; reload the model"; return false;
                }
                match = start + equals + 1; length = line.size() - equals - 1;
            } else if (equals != std::string_view::npos && line.substr(0, equals) == optionName) {
                error = "Malformed CMake cache option"; return false;
            }
        }
        start = end == contents.size() ? end : end + 1;
    }
    if (count != 1) { error = "CMake cache option missing or duplicated; reload the model"; return false; }
    update.path = std::string(relativeCachePath); update.original = std::move(contents); update.replacement = update.original;
    update.replacement.replace(match, length, enabled ? "ON" : "OFF"); return true;
}
bool saveCMakeBoolCacheUpdate(workspace::Project& project, const CMakeCacheUpdate& update, std::string& error) {
    if (!cachePathSafe(project, update.path, error)) return false;
    return project.writeFile(update.path, update.replacement, error, &update.original);
}

}
