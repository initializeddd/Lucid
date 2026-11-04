#include "modules/Registry.hpp"

#include "editor/json/Json.hpp"
#include <curl/curl.h>
#include <picosha2.h>

#include <algorithm>
#include <memory>
#include <mutex>

namespace lucid::modules {
namespace {
constexpr std::size_t kMaxResponse = 1024 * 1024;

bool validName(std::string_view value, bool slash) {
    if (value.empty() || value.size() > 160) return false;
    const auto alphanumeric = [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); };
    if (!alphanumeric(value.front()) || !alphanumeric(value.back()) || value.find("..") != std::string_view::npos) return false;
    std::size_t separators = 0;
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char c = value[i];
        if (c == '/') { if (!i || i + 1 == value.size() || !alphanumeric(value[i - 1]) || !alphanumeric(value[i + 1])) return false; ++separators; continue; }
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')) return false;
    }
    return slash ? separators == 1 && value.front() != '/' && value.back() != '/' : separators == 0;
}

const editor::JsonValue* field(const editor::JsonValue& json, std::string_view name, editor::JsonType type) {
    const auto* value = json.find(name);
    return value && value->type == type ? value : nullptr;
}

bool get(std::string_view url, const std::vector<std::string>& headers,
         std::string& output, std::string& error) {
    static std::once_flag initialized;
    std::call_once(initialized, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    using Handle = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
    Handle curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl) { error = "Cannot initialize module download"; return false; }
    curl_slist* raw = nullptr;
    for (const auto& header : headers) {
        auto* next = curl_slist_append(raw, header.c_str());
        if (!next) { curl_slist_free_all(raw); error = "Cannot prepare request"; return false; }
        raw = next;
    }
    using Headers = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;
    Headers requestHeaders(raw, curl_slist_free_all);
    const std::string address(url);
    output.clear();
    curl_easy_setopt(curl.get(), CURLOPT_URL, address.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, requestHeaders.get());
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "Lucid-Modules/1.0");
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 4L);
    curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl.get(), CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, +[](char* data, std::size_t size, std::size_t count, void* context) -> std::size_t {
        auto& result = *static_cast<std::string*>(context);
        if (size && count > (kMaxResponse - result.size()) / size) return 0;
        const auto bytes = size * count;
        result.append(data, bytes);
        return bytes;
    });
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &output);
    const auto result = curl_easy_perform(curl.get());
    long status = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
    if (result != CURLE_OK) {
        if (status == 401 || status == 403 || status == 404)
            error = "GitHub package is private, unavailable, or was not found";
        else error = std::string("GitHub package request failed: ") + curl_easy_strerror(result);
        output.clear(); return false;
    }
    return true;
}
} // namespace

bool parseCatalog(std::string_view text, std::vector<RegistryEntry>& entries, std::string& error) {
    entries.clear();
    editor::JsonError parseError;
    const auto json = editor::parseJson(text, parseError);
    const auto* list = json && json->type == editor::JsonType::Object ? field(*json, "modules", editor::JsonType::Array) : nullptr;
    if (!list || list->children.size() > 1000) { error = "Invalid module catalog"; return false; }
    for (const auto& item : list->children) {
        const auto* id = field(item, "id", editor::JsonType::String);
        const auto* package = field(item, "package", editor::JsonType::String);
        const auto* version = field(item, "version", editor::JsonType::String);
        const auto* name = item.find("name");
        const auto* description = item.find("description");
        if (item.type != editor::JsonType::Object || !id || !package || !version ||
            (name && (name->type != editor::JsonType::String || name->stringValue.size() > 128)) ||
            (description && (description->type != editor::JsonType::String || description->stringValue.size() > 512)) ||
            id->stringValue.size() > 80 || id->stringValue.find('.') != std::string::npos ||
            !validName(id->stringValue, false) || !validName(package->stringValue, true) ||
            !validName(version->stringValue, false) ||
            std::any_of(entries.begin(), entries.end(), [&](const auto& e) { return e.id == id->stringValue; })) {
            entries.clear(); error = "Invalid or duplicate module catalog entry"; return false;
        }
        entries.push_back({id->stringValue, package->stringValue, version->stringValue,
                           name ? name->stringValue : std::string{},
                           description ? description->stringValue : std::string{}});
    }
    error.clear(); return true;
}

bool fetchPackage(std::string_view package, std::string_view version, std::string_view mediaType,
                  std::string& contents, std::string& error) {
    return fetchPackage(package, version, mediaType, contents, error, get);
}

bool fetchPackage(std::string_view package, std::string_view version, std::string_view mediaType,
                  std::string& contents, std::string& error, const RegistryRequest& request) {
    contents.clear();
    if (!validName(package, true) || !validName(version, false) ||
        (mediaType != "application/vnd.lucid.module.v1+json" && mediaType != "application/vnd.lucid.catalog.v1+json")) {
        error = "Invalid package reference"; return false;
    }
    const std::string repo(package);
    std::string tokenJson;
    if (!request("https://ghcr.io/token?scope=repository:" + repo + ":pull", {}, tokenJson, error)) return false;
    editor::JsonError parseError;
    const auto tokenObject = editor::parseJson(tokenJson, parseError);
    const auto* token = tokenObject ? field(*tokenObject, "token", editor::JsonType::String) : nullptr;
    if (!token || token->stringValue.empty() || token->stringValue.find_first_of("\r\n") != std::string::npos) { error = "Invalid GitHub package token"; return false; }
    const std::vector<std::string> headers = {"Authorization: Bearer " + token->stringValue, "Accept: application/vnd.oci.image.manifest.v1+json"};
    const std::string base = "https://ghcr.io/v2/" + repo;
    std::string manifest;
    if (!request(base + "/manifests/" + std::string(version), headers, manifest, error)) return false;
    const auto json = editor::parseJson(manifest, parseError);
    const auto* layers = json ? field(*json, "layers", editor::JsonType::Array) : nullptr;
    if (!layers || layers->children.size() != 1) { error = "Package must contain exactly one module layer"; return false; }
    const auto* type = field(layers->children[0], "mediaType", editor::JsonType::String);
    const auto* digest = field(layers->children[0], "digest", editor::JsonType::String);
    if (!type || type->stringValue != mediaType || !digest || digest->stringValue.size() != 71 ||
        !digest->stringValue.starts_with("sha256:") ||
        !std::all_of(digest->stringValue.begin() + 7, digest->stringValue.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); })) {
        error = "Invalid module package layer"; return false;
    }
    if (!request(base + "/blobs/" + digest->stringValue, headers, contents, error)) return false;
    if (picosha2::hash256_hex_string(contents) != digest->stringValue.substr(7)) {
        contents.clear(); error = "Module package failed integrity verification"; return false;
    }
    error.clear(); return true;
}
} // namespace lucid::modules
