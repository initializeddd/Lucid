#include "modules/DependencyRisk.hpp"
#include "editor/json/Json.hpp"
#include "debug/Protocol.hpp"
#include "modules/ApiRequests.hpp"
#include <algorithm>
#include <set>
#include <regex>
#include <map>
#include <utility>
namespace lucid::modules {
namespace {
using editor::JsonType; using editor::JsonValue;
constexpr std::size_t maxInput = 8 * 1024 * 1024, maxPackages = 20000, maxVulns = 1000;
const JsonValue* field(const JsonValue& value, const char* name, JsonType type) {
    auto* v = value.find(name); return v && v->type == type ? v : nullptr;
}
bool uniqueObjects(const JsonValue& v, std::size_t depth = 0) {
    if (depth > 64) return false;
    if (v.type == JsonType::Object) {
        std::set<std::string> seen;
        for (const auto& child : v.children) if (!seen.insert(child.key).second || !uniqueObjects(child, depth + 1)) return false;
    } else if (v.type == JsonType::Array) for (const auto& child : v.children) if (!uniqueObjects(child, depth + 1)) return false;
    return true;
}
bool parse(std::string_view text, JsonValue& out, std::string& error) {
    if (text.size() > maxInput) { error = "Input exceeds the 8 MiB limit."; return false; }
    editor::JsonError e; auto json = editor::parseJson(text, e);
    if (!json || json->type != JsonType::Object || !uniqueObjects(*json)) { error = "Invalid, duplicate-key, or deeply nested JSON object."; return false; }
    out = std::move(*json); return true;
}
bool flag(const JsonValue& value, const char* key) { auto* v = field(value, key, JsonType::Bool); return v && v->boolValue; }
std::string nameFromPath(const std::string& path) {
    auto pos = path.rfind("node_modules/");
    if (pos == std::string::npos || (pos && path[pos - 1] != '/')) return {};
    auto name = path.substr(pos + 13);
    if (name.empty()) return {};
    auto slash = name.find('/');
    if (name[0] == '@') {
        if (slash == std::string::npos || slash == 1 || slash + 1 == name.size() || name.find('/', slash + 1) != std::string::npos) return {};
    } else if (slash != std::string::npos) return {};
    return name;
}
bool verifiedRegistryPackage(const std::string& name, const std::string& version, const JsonValue& item) {
    // Only canonical npm registry tarball URLs with an unambiguous identity are covered.
    // Other registries, git/file links and aliases are deliberately omitted.
    const auto* resolved = field(item, "resolved", JsonType::String);
    if (!resolved || name.empty() || name.size() > 214) return false;
    for (unsigned char c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '@' || c == '/' || c == '-' || c == '_' || c == '.')) return false;
    auto slash = name.rfind('/');
    auto basename = slash == std::string::npos ? name : name.substr(slash + 1);
    auto expected = "https://registry.npmjs.org/" + name + "/-/" + basename + "-" + version + ".tgz";
    return resolved->stringValue == expected;
}
bool exactVersion(const std::string& v) {
    // Require an exact npm semantic version, including optional prerelease/build identifiers.
    static const std::regex pattern(R"(^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-[0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*)?(\+[0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*)?$)");
    return v.size() <= 256 && std::regex_match(v, pattern);
}
bool validIdentifier(std::string_view text, std::size_t limit) {
    if (text.empty() || text.size() > limit) return false;
    return std::all_of(text.begin(), text.end(), [](unsigned char c) { return c >= 33 && c <= 126; });
}
bool goModuleName(std::string_view name) {
    if (name.empty() || name.size() > 512 || name.front() == '/' || name.back() == '/') return false;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= name.size(); ++i) {
        if (i == name.size() || name[i] == '/') {
            const auto part = name.substr(start, i - start);
            if (part.empty() || part == "." || part == ".." || part.front() == '.') return false;
            start = i + 1;
        } else {
            const unsigned char c = name[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~' || c == '+')) return false;
        }
    }
    return true;
}
bool goVersion(std::string_view version) {
    return version.size() > 1 && version.front() == 'v' && exactVersion(std::string(version.substr(1)));
}
bool goSumHash(std::string_view hash) {
    if (!hash.starts_with("h1:") || hash.size() != 47 || hash.back() != '=') return false;
    return std::all_of(hash.begin() + 3, hash.end() - 1, [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '+' || c == '/'; });
}
bool crateName(std::string_view name) {
    return !name.empty() && name.size() <= 64 && std::all_of(name.begin(), name.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    });
}
bool supportedSri(std::string_view value) {
    std::size_t prefix = 0, encoded = 0, padding = 0;
    if (value.starts_with("sha256-")) { prefix = 7; encoded = 44; padding = 1; }
    else if (value.starts_with("sha384-")) { prefix = 7; encoded = 64; }
    else if (value.starts_with("sha512-")) { prefix = 7; encoded = 88; padding = 2; }
    else return false;
    if (value.size() != prefix + encoded) return false;
    for (std::size_t i = prefix; i < value.size() - padding; ++i) {
        const unsigned char c = value[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '+' || c == '/')) return false;
    }
    return std::all_of(value.end() - static_cast<std::ptrdiff_t>(padding), value.end(), [](char c) { return c == '='; });
}
}
bool parseCargoDependencyRisk(std::string_view lockfile, DependencyRiskReport& report, std::string& error) {
    report = {}; error.clear();
    if (lockfile.size() > maxInput) { error = "Input exceeds the 8 MiB limit."; return false; }
    // Multiline TOML strings would make a line-oriented table boundary ambiguous.
    if (lockfile.find("\"\"\"") != std::string_view::npos || lockfile.find("'''") != std::string_view::npos) { error = "Multiline TOML strings are not supported."; return false; }
    const auto trim = [](std::string_view value) {
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) value.remove_prefix(1);
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) value.remove_suffix(1);
        return value;
    };
    struct Entry { std::string name, version, source, checksum; std::set<std::string> keys; bool unsupported = false; } entry;
    DependencyRiskReport result; std::set<std::pair<std::string, std::string>> seen;
    std::size_t skipped = 0, tables = 0; bool inPackage = false, versionSeen = false, root = true; int dependencyDepth = 0;
    const auto scanArray = [&](std::string_view value) {
        char quote = 0; bool escape = false;
        for (const char c : value) {
            if (quote) { if (escape) escape = false; else if (quote == '"' && c == '\\') escape = true; else if (c == quote) quote = 0; }
            else if (c == '"' || c == '\'') quote = c;
            else if (c == '[') { if (++dependencyDepth > 32) return false; }
            else if (c == ']' && --dependencyDepth < 0) return false;
        }
        return quote == 0;
    };
    auto finish = [&]() {
        if (!inPackage) return true;
        ++tables;
        if (tables > maxPackages) { error = "Too many lockfile packages."; return false; }
        const bool registry = entry.source == "registry+https://github.com/rust-lang/crates.io-index" || entry.source == "sparse+https://index.crates.io/";
        const bool checksum = entry.checksum.size() == 64 && std::all_of(entry.checksum.begin(), entry.checksum.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
        if (entry.unsupported || !crateName(entry.name) || !exactVersion(entry.version) || !registry || !checksum) ++skipped;
        else {
            if (!seen.emplace(entry.name, entry.version).second) { error = "Duplicate crates.io package."; return false; }
            result.packages.push_back({entry.name + "@" + entry.version, entry.name, entry.version, "crates.io", false, false, false, {}});
            result.packages.back().declaredSource = entry.source;
            result.packages.back().declaredChecksum = "sha256:" + entry.checksum;
        }
        entry = {}; return true;
    };
    for (std::size_t offset = 0; offset < lockfile.size();) {
        auto end = lockfile.find('\n', offset); if (end == std::string_view::npos) end = lockfile.size();
        auto line = lockfile.substr(offset, end - offset); offset = end == lockfile.size() ? end : end + 1;
        if (line.size() > 16384) { error = "Cargo.lock line exceeds size limit."; return false; }
        // Strip comments only outside quoted TOML strings.
        char quote = 0; bool escaped = false;
        for (std::size_t i = 0; i < line.size(); ++i) {
            const char c = line[i];
            if (quote) { if (escaped) escaped = false; else if (quote == '"' && c == '\\') escaped = true; else if (c == quote) quote = 0; }
            else if (c == '"' || c == '\'') quote = c;
            else if (c == '#') { line = line.substr(0, i); break; }
        }
        line = trim(line); if (line.empty()) continue;
        if (dependencyDepth) { if (!scanArray(line)) { error = "Invalid Cargo dependency array."; return false; } continue; }
        if (line == "[[package]]") { if (!versionSeen) { error = "Cargo.lock version must precede packages."; return false; } if (!finish()) return false; inPackage = true; root = false; continue; }
        if (line.front() == '[' && line.back() == ']') { if (!finish()) return false; inPackage = false; root = false; continue; }
        const auto equals = line.find('='); if (equals == std::string_view::npos) continue;
        const auto key = trim(line.substr(0, equals)), value = trim(line.substr(equals + 1));
        if (inPackage && key == "dependencies") {
            if (value.empty() || value.front() != '[' || !scanArray(value)) { error = "Invalid Cargo dependency array."; return false; }
            continue;
        }
        if (root && key == "version") {
            if (versionSeen || (value != "3" && value != "4")) { error = "Only Cargo.lock v3/v4 is supported."; return false; }
            versionSeen = true; continue;
        }
        if (!inPackage || (key != "name" && key != "version" && key != "source" && key != "checksum")) continue;
        if (!entry.keys.insert(std::string(key)).second) { error = "Duplicate Cargo package field."; return false; }
        if (value.size() < 2 || value.front() != '"' || value.back() != '"' || value.substr(1, value.size() - 2).find_first_of("\\\"") != std::string_view::npos) { entry.unsupported = true; continue; }
        const std::string text(value.substr(1, value.size() - 2));
        if (key == "name") entry.name = text; else if (key == "version") entry.version = text; else if (key == "source") entry.source = text; else entry.checksum = text;
    }
    if (dependencyDepth) { error = "Unclosed Cargo dependency array."; return false; }
    if (!finish()) return false;
    if (!versionSeen) { error = "Only Cargo.lock v3/v4 is supported."; return false; }
    result.limitations.push_back("Only exact crates.io registry packages with checksums are assessed; workspace, Git, alternate registry and malformed entries are skipped.");
    result.limitations.push_back("Direct dependencies and enabled feature or target reachability are not determined from Cargo.lock.");
    result.limitations.push_back("OSV data may be incomplete; results do not establish exploitability or runtime reachability.");
    if (skipped) result.limitations.push_back(std::to_string(skipped) + " package entries were skipped.");
    report = std::move(result); return true;
}
bool parseGoSumDependencyRisk(std::string_view goSum, DependencyRiskReport& report, std::string& error) {
    report = {}; error.clear();
    if (goSum.size() > maxInput) { error = "Input exceeds the 8 MiB limit."; return false; }
    DependencyRiskReport result; std::map<std::pair<std::string, std::string>, std::string> seen;
    std::size_t offset = 0, skipped = 0, metadata = 0;
    while (offset < goSum.size()) {
        auto end = goSum.find('\n', offset); if (end == std::string_view::npos) end = goSum.size();
        auto line = goSum.substr(offset, end - offset); offset = end == goSum.size() ? end : end + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.empty()) continue;
        const auto first = line.find(' '), second = first == std::string_view::npos ? first : line.find(' ', first + 1);
        if (line.size() > 4096 || first == std::string_view::npos || second == std::string_view::npos || line.find(' ', second + 1) != std::string_view::npos) { ++skipped; continue; }
        auto name = line.substr(0, first), version = line.substr(first + 1, second - first - 1), hash = line.substr(second + 1);
        if (!goModuleName(name) || !goSumHash(hash)) { ++skipped; continue; }
        if (version.ends_with("/go.mod")) { version.remove_suffix(7); if (goVersion(version)) ++metadata; else ++skipped; continue; }
        if (!goVersion(version)) { ++skipped; continue; }
        const auto key = std::make_pair(std::string(name), std::string(version));
        const auto [existing, inserted] = seen.emplace(key, hash);
        if (!inserted) { if (existing->second != hash) { error = "Conflicting Go checksums for the same module version."; return false; } continue; }
        if (result.packages.size() >= maxPackages) { error = "Too many lockfile packages."; return false; }
        result.packages.push_back({key.first + "@" + key.second, key.first, key.second, "Go", false, false, false, {}});
        result.packages.back().declaredSource = "go.sum checksum entry; source not established";
        result.packages.back().declaredChecksum = std::string(hash);
    }
    result.limitations.push_back("go.sum includes checksum history, not the selected build list; unused versions may be reported.");
    result.limitations.push_back("Direct dependencies, local replacements and modules without checksum entries cannot be determined from go.sum.");
    result.limitations.push_back("OSV data may be incomplete; results do not establish exploitability or runtime reachability.");
    if (metadata) result.limitations.push_back(std::to_string(metadata) + " go.mod-only checksum entries were not assessed as module archives.");
    if (skipped) result.limitations.push_back(std::to_string(skipped) + " unsupported or malformed checksum entries were skipped.");
    report = std::move(result); return true;
}
bool parseNpmDependencyRisk(std::string_view lockfile, DependencyRiskReport& report, std::string& error) {
    report = {}; error.clear(); JsonValue json;
    if (!parse(lockfile, json, error)) return false;
    const auto* version = field(json, "lockfileVersion", JsonType::Number);
    if (version && version->numberValue == 1) {
        const auto* dependencies = json.find("dependencies");
        if (dependencies && dependencies->type != JsonType::Object) { error = "Invalid lockfile dependency map."; return false; }
        DependencyRiskReport result; std::size_t visited = 0, skipped = 0;
        const auto visit = [&](auto&& self, const JsonValue& map, const std::string& prefix) -> bool {
            for (const auto& item : map.children) {
                if (++visited > maxPackages) { error = "Too many lockfile packages."; return false; }
                if (item.type != JsonType::Object) { error = "Invalid package entry."; return false; }
                const auto path = prefix + "node_modules/" + item.key;
                if (path.size() > 16384) { error = "Dependency path exceeds size limit."; return false; }
                const auto name = nameFromPath(path);
                const auto* explicitName = field(item, "name", JsonType::String);
                const auto* ver = field(item, "version", JsonType::String);
                if (name != item.key || (explicitName && explicitName->stringValue != name) || !ver || !exactVersion(ver->stringValue) || !verifiedRegistryPackage(name, ver->stringValue, item)) ++skipped;
                else {
                    result.packages.push_back({path, name, ver->stringValue, "npm", false, flag(item, "dev"), flag(item, "optional"), {}});
                    result.packages.back().declaredSource = field(item, "resolved", JsonType::String)->stringValue;
                    if (const auto* integrity = field(item, "integrity", JsonType::String); integrity && supportedSri(integrity->stringValue)) result.packages.back().declaredChecksum = integrity->stringValue;
                }
                if (const auto* children = item.find("dependencies")) {
                    if (children->type != JsonType::Object) { error = "Invalid nested dependency map."; return false; }
                    if (!self(self, *children, path + "/")) return false;
                }
            }
            return true;
        };
        if (dependencies && !visit(visit, *dependencies, {})) return false;
        result.limitations.push_back("npm package-lock v1 does not establish direct dependencies from the lockfile alone; directness is not inferred.");
        result.limitations.push_back("Only canonical npm registry tarball URLs matching the install name and exact version are covered; aliases and other sources are skipped.");
        result.limitations.push_back("OSV data may be incomplete; results do not establish exploitability or runtime reachability.");
        if (skipped) result.limitations.push_back(std::to_string(skipped) + " package entries were skipped (aliases, ambiguous sources, unsupported paths, or missing/non-exact versions).");
        report = std::move(result); return true;
    }
    const auto* packages = field(json, "packages", JsonType::Object);
    if (!version || (version->numberValue != 2 && version->numberValue != 3) || !packages) { error = "Only npm package-lock v1, or v2/v3 with a packages map, is supported."; return false; }
    if (packages->children.size() > maxPackages + 1) { error = "Too many lockfile packages."; return false; }
    const auto* root = field(*packages, "", JsonType::Object);
    if (!root) { error = "Lockfile root package is missing."; return false; }
    std::set<std::string> direct, aliases, resolvedDirect;
    for (auto key : {"dependencies", "devDependencies", "optionalDependencies", "peerDependencies"}) {
        const auto* raw = root->find(key);
        if (raw && raw->type != JsonType::Object) { error = "Invalid root dependency map."; return false; }
        if (raw) for (const auto& child : raw->children) {
            if (child.type != JsonType::String) { error = "Invalid dependency specifier."; return false; }
            direct.insert(child.key);
            if (child.stringValue.rfind("npm:", 0) == 0) aliases.insert(child.key);
        }
    }
    DependencyRiskReport result; std::size_t skipped = 0;
    for (const auto& item : packages->children) {
        if (item.key.empty()) continue;
        if (item.type != JsonType::Object) { error = "Invalid package entry."; return false; }
        auto name = nameFromPath(item.key);
        const auto* explicitName = field(item, "name", JsonType::String);
        const auto* ver = field(item, "version", JsonType::String);
        // npm aliases may have a different resolved package name than the install path.
        if (explicitName && explicitName->stringValue != name) { ++skipped; continue; }
        auto installName = nameFromPath(item.key);
        const bool topLevel = item.key == "node_modules/" + installName;
        if (flag(item, "link") || name.empty() || !ver || !exactVersion(ver->stringValue) ||
            !verifiedRegistryPackage(name, ver->stringValue, item) || (topLevel && aliases.count(installName))) { ++skipped; continue; }
        if (topLevel && direct.count(installName)) resolvedDirect.insert(installName);
        result.packages.push_back({item.key, name, ver->stringValue, "npm", topLevel && direct.count(installName) != 0, flag(item, "dev"), flag(item, "optional"), {}});
        result.packages.back().declaredSource = field(item, "resolved", JsonType::String)->stringValue;
        if (const auto* integrity = field(item, "integrity", JsonType::String); integrity && supportedSri(integrity->stringValue)) result.packages.back().declaredChecksum = integrity->stringValue;
    }
    result.limitations.push_back("Only npm package-lock v2/v3 packages with explicit versions are covered; other ecosystems and manifests are not checked.");
    result.limitations.push_back("OSV data may be incomplete; results do not establish exploitability or runtime reachability.");
    if (resolvedDirect.size() < direct.size()) result.limitations.push_back(std::to_string(direct.size() - resolvedDirect.size()) + " direct dependencies could not be mapped to an exact installed package.");
    result.limitations.push_back("Only canonical npm registry tarball URLs matching the install name and exact version are covered; aliases and other sources are skipped.");
    if (skipped) result.limitations.push_back(std::to_string(skipped) + " package entries were skipped (links, aliases, ambiguous sources, unsupported paths, or missing/non-exact versions).");
    report = std::move(result); return true;
}
bool applyOsvBatchResponse(std::string_view response, DependencyRiskReport& report, std::string& error) {
    error.clear(); JsonValue json;
    if (!parse(response, json, error)) return false;
    auto* results = field(json, "results", JsonType::Array);
    if (!results || results->children.size() != report.packages.size()) { error = "OSV result count does not match submitted packages."; return false; }
    auto updated = report;
    for (std::size_t i = 0; i < results->children.size(); ++i) {
        const auto& item = results->children[i];
        if (item.type != JsonType::Object || item.find("next_page_token")) { error = "Invalid or paginated OSV response; results are incomplete."; return false; }
        updated.packages[i].vulnerabilities.clear();
        auto* vulns = item.find("vulns"); if (!vulns) continue;
        if (vulns->type != JsonType::Array || vulns->children.size() > maxVulns) { error = "Invalid OSV vulnerability list."; return false; }
        for (const auto& vuln : vulns->children) {
            auto* id = field(vuln, "id", JsonType::String);
            if (vuln.type != JsonType::Object || !id || !validIdentifier(id->stringValue, 256)) { error = "Invalid OSV vulnerability ID."; return false; }
            updated.packages[i].vulnerabilities.push_back(id->stringValue);
        }
    }
    report = std::move(updated); return true;
}
OsvResponse requestOsvBatch(std::string_view body, const std::atomic_bool* cancel) {
    ApiRequest request{"OSV batch", "POST", "https://api.osv.dev/v1/querybatch", std::string(body), {{"Content-Type", "application/json"}}};
    auto result = sendApiRequest(request, cancel);
    return {result.status, std::move(result.body), std::move(result.error)};
}
OsvResponse requestOsvDetail(std::string_view id, const std::atomic_bool* cancel) {
    if (id.empty() || id.size() > 256 || id.front() == '.' || id.back() == '.' || id.find("..") != std::string_view::npos || !std::all_of(id.begin(), id.end(), [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'; })) return {0, {}, "Invalid advisory ID"};
    ApiRequest request{"OSV advisory", "GET", "https://api.osv.dev/v1/vulns/" + std::string(id), "", {}};
    auto result = sendApiRequest(request, cancel); return {result.status, std::move(result.body), std::move(result.error)};
}
bool parseOsvDetail(std::string_view json, std::string_view expectedId, OsvDetail& detail, std::string& error) {
    detail = {}; error.clear(); JsonValue root;
    if (json.size() > 1024U * 1024U || !parse(json, root, error)) { if (error.empty()) error = "Advisory exceeds size limit"; return false; }
    const auto* id = field(root, "id", JsonType::String);
    if (!id || id->stringValue != expectedId) { error = "Advisory ID does not match request"; return false; }
    OsvDetail out; out.id = id->stringValue;
    auto optionalText = [&](const JsonValue& node, const char* key, std::string& value, std::size_t limit) {
        const auto* v = node.find(key); if (!v) return true;
        if (v->type != JsonType::String || v->stringValue.size() > limit || v->stringValue.find('\0') != std::string::npos) return false;
        value = v->stringValue; return true;
    };
    if (!optionalText(root, "summary", out.summary, 16384) || !optionalText(root, "details", out.details, 262144)) { error = "Invalid advisory text"; return false; }
    if (const auto* aliases = root.find("aliases")) {
        if (aliases->type != JsonType::Array || aliases->children.size() > 100) { error = "Invalid advisory aliases"; return false; }
        for (const auto& a : aliases->children) if (a.type != JsonType::String || !validIdentifier(a.stringValue, 256)) { error = "Invalid advisory alias"; return false; } else out.aliases.push_back(a.stringValue);
    }
    if (const auto* affected = root.find("affected")) {
        if (affected->type != JsonType::Array || affected->children.size() > 1000) { error = "Invalid affected package list"; return false; }
        for (const auto& a : affected->children) {
            const auto* package = field(a, "package", JsonType::Object);
            const auto* ecosystem = package ? field(*package, "ecosystem", JsonType::String) : nullptr;
            const auto* name = package ? field(*package, "name", JsonType::String) : nullptr;
            if (!ecosystem || (ecosystem->stringValue != "npm" && ecosystem->stringValue != "Go" && ecosystem->stringValue != "crates.io") || !name || !validIdentifier(name->stringValue, 512)) continue;
            const auto* ranges = a.find("ranges"); if (!ranges) continue;
            if (ranges->type != JsonType::Array || ranges->children.size() > 1000) { error = "Invalid affected ranges"; return false; }
            for (const auto& range : ranges->children) {
                const auto* events = field(range, "events", JsonType::Array); if (!events || events->children.size() > 1000) { error = "Invalid range events"; return false; }
                for (const auto& event : events->children) if (const auto* fixed = event.find("fixed")) {
                    if (fixed->type != JsonType::String || !validIdentifier(fixed->stringValue, 256) || out.fixedVersions.size() >= 1000) { error = "Invalid fixed version"; return false; }
                    out.fixedVersions.push_back(name->stringValue + " " + fixed->stringValue);
                }
            }
        }
    }
    if (const auto* refs = root.find("references")) {
        if (refs->type != JsonType::Array || refs->children.size() > 100) { error = "Invalid advisory references"; return false; }
        for (const auto& ref : refs->children) { const auto* url = field(ref, "url", JsonType::String); if (url && url->stringValue.size() <= 2048 && url->stringValue.starts_with("https://") && validIdentifier(url->stringValue, 2048)) out.references.push_back(url->stringValue); }
    }
    detail = std::move(out); return true;
}
bool queryOsvAdvisories(DependencyRiskReport& report, const OsvTransport& transport, std::string& error, const std::atomic_bool* cancel) {
    error.clear();
    if (!transport || report.packages.size() > maxPackages) { error = "Invalid OSV query."; return false; }
    if (cancel && cancel->load()) { error = "OSV query cancelled."; return false; }
    for (const auto& package : report.packages) {
        if (!((package.ecosystem == "npm" && validIdentifier(package.name, 214) && exactVersion(package.version)) ||
              (package.ecosystem == "Go" && goModuleName(package.name) && goVersion(package.version)) ||
              (package.ecosystem == "crates.io" && crateName(package.name) && exactVersion(package.version)))) { error = "Unsupported package identity."; return false; }
    }
    auto updated = report;
    for (auto& package : updated.packages) package.vulnerabilities.clear();
    std::size_t total = 0;
    struct Pending { std::size_t index; std::string token; std::set<std::string> seen; unsigned page = 0; };
    for (std::size_t first = 0; first < report.packages.size(); first += 100) {
        std::vector<Pending> pending;
        const auto end = std::min(first + 100, report.packages.size());
        for (std::size_t i = first; i < end; ++i) pending.push_back({i, {}, {}, 0});
        while (!pending.empty()) {
            if (cancel && cancel->load()) { error = "OSV query cancelled."; return false; }
            std::string body = "{\"queries\":[";
            for (std::size_t i = 0; i < pending.size(); ++i) {
                if (i) body += ',';
                const auto& p = report.packages[pending[i].index];
                body += "{\"package\":{\"ecosystem\":" + debug::jsonString(p.ecosystem) + ",\"name\":" + debug::jsonString(p.name) + "},\"version\":" + debug::jsonString(p.version);
                if (!pending[i].token.empty()) body += ",\"page_token\":" + debug::jsonString(pending[i].token);
                body += '}';
            }
            body += "]}";
            const auto response = transport(body, cancel);
            if (cancel && cancel->load()) { error = "OSV query cancelled."; return false; }
            if (!response.error.empty() || response.status != 200) { error = "OSV request failed."; return false; }
            JsonValue root;
            if (!parse(response.body, root, error)) return false;
            const auto* results = field(root, "results", JsonType::Array);
            if (!results || results->children.size() != pending.size()) { error = "OSV result count does not match submitted packages."; return false; }
            std::vector<Pending> next;
            for (std::size_t i = 0; i < pending.size(); ++i) {
                const auto& result = results->children[i]; auto& state = pending[i];
                if (result.type != JsonType::Object) { error = "Invalid OSV response."; return false; }
                if (const auto* vulns = result.find("vulns")) {
                    if (vulns->type != JsonType::Array || vulns->children.size() > maxVulns || vulns->children.size() > 100000 - total) { error = "OSV result limit exceeded."; return false; }
                    auto& ids = updated.packages[state.index].vulnerabilities;
                    for (const auto& vuln : vulns->children) {
                        const auto* id = field(vuln, "id", JsonType::String);
                        if (vuln.type != JsonType::Object || !id || !validIdentifier(id->stringValue, 256)) { error = "Invalid OSV vulnerability ID."; return false; }
                        ids.push_back(id->stringValue); ++total;
                    }
                }
                if (const auto* token = result.find("next_page_token")) {
                    if (token->type != JsonType::String || !validIdentifier(token->stringValue, 4096) || ++state.page >= 20 || !state.seen.insert(token->stringValue).second) { error = "Invalid or excessive OSV pagination."; return false; }
                    state.token = token->stringValue; next.push_back(std::move(state));
                }
            }
            pending = std::move(next);
        }
    }
    report = std::move(updated); return true;
}
}
