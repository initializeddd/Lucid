#include "modules/WebPreviewBundle.hpp"
#include "modules/WebPreviewConfig.hpp"
#include "editor/json/Json.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace lucid::modules {
bool loadWebPreviewBundle(const workspace::Project& project, std::string_view json, WebPreviewBundle& bundle, std::string& error) {
    bundle = {}; error.clear();
    if (json.size() > 65536) { error = "Preview configuration exceeds 64 KiB"; return false; }
    editor::JsonError parseError; const auto root = editor::parseJson(json, parseError);
    if (!root || root->type != editor::JsonType::Object) { error = "Invalid preview configuration"; return false; }
    std::set<std::string> keys;
    for (const auto& child : root->children) if (!keys.insert(child.key).second) { error = "Duplicate preview configuration key"; return false; }
    const auto* entry = root->find("entry"), *assets = root->find("assets");
    if (!entry || entry->type != editor::JsonType::String || !assets || assets->type != editor::JsonType::Array || assets->children.empty() || assets->children.size() > 256) { error = "Set an HTML entry and 1–256 asset paths"; return false; }
    WebPreviewConfig config; config.entry = entry->stringValue; config.width = 800; config.height = 600;
    for (const auto& [name, destination] : {std::pair{"width", &config.width}, std::pair{"height", &config.height}}) {
        if (const auto* value = root->find(name)) {
            if (value->type != editor::JsonType::Number || !std::isfinite(value->numberValue) || value->numberValue < 100 || value->numberValue > 4096 || std::floor(value->numberValue) != value->numberValue) { error = "Invalid preview viewport size"; return false; }
            *destination = static_cast<std::size_t>(value->numberValue);
        }
    }
    std::string state;
    if (const auto* value = root->find("state")) {
        if (value->type != editor::JsonType::String || value->stringValue.empty() || (value->stringValue.front() != '?' && value->stringValue.front() != '#')) { error = "State must be a query or fragment"; return false; }
        state = value->stringValue; config.states.push_back({"selected", state});
    }
    if (!validateWebPreviewConfig(project.root(), config, error)) return false;
    if (config.width < 128 || config.width > 2048 || config.height < 128 || config.height > 2048) { error = "Web preview size must be 128–2048 pixels per side"; return false; }
    std::vector<WebPreviewAsset> snapshot; std::size_t total = 0; bool hasEntry = false;
    for (const auto& value : assets->children) {
        if (value.type != editor::JsonType::String || value.stringValue.empty() || value.stringValue.front() == '/') { error = "Asset paths must be project-relative"; return false; }
        const auto path = std::filesystem::path(value.stringValue);
        if (path.is_absolute() || path.has_root_name() || path.lexically_normal().generic_string() != value.stringValue) { error = "Invalid asset path"; return false; }
        for (const auto& part : path) if (part == "." || part == "..") { error = "Invalid asset path"; return false; }
        std::string content;
        if (!project.readFileBounded(value.stringValue, content, error, 16U * 1024U * 1024U - total)) return false;
        total += content.size(); hasEntry = hasEntry || value.stringValue == entry->stringValue;
        snapshot.push_back({"/" + value.stringValue, std::move(content)});
    }
    if (!hasEntry) { error = "Include the entry in the asset list"; return false; }
    if (!bundle.assets.load(std::move(snapshot), error)) return false;
    bundle.target = "/" + entry->stringValue + state; bundle.width = static_cast<unsigned>(config.width); bundle.height = static_cast<unsigned>(config.height);
    return true;
}
}
