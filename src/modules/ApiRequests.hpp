#pragma once
#include <map>
#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::workspace { class Project; }
namespace lucid::modules {
struct ApiRequest {
    std::string name, method, url, body;
    std::map<std::string, std::string> headers;
};
struct ApiEnvironmentVariable {
    std::string value;
    // A display hint only. Values remain plaintext in memory and source files.
    bool secret = false;
};
struct ApiEnvironment {
    std::string name;
    std::map<std::string, ApiEnvironmentVariable> variables;
};
// Parses a separate JSON file: {"environments":[{"name":"dev",
// "variables":{"PORT":{"type":"number","value":"8080"},
// "TOKEN":{"type":"string","value":"...","secret":true}}}]}.
// Types are string, number (an exact JSON number encoded as a string), and
// boolean (a JSON boolean). No I/O, network, expansion, or secure storage.
// On failure environments is empty and errors never contain variable values.
[[nodiscard]] bool parseApiEnvironments(std::string_view json,
    std::vector<ApiEnvironment>& environments, std::string& error);
// JSON format: {"requests":[{"name":"...","method":"GET","url":"https://...",
// "headers":{"Header":"value"},"body":"optional"}]}. Variables in URL, headers
// and body use {{NAME}} and are replaced from variables. Values can contain secrets;
// callers should never log resolved requests without redacting them.
// This only prepares requests; it does not send them or access files, network or processes.
[[nodiscard]] bool parseApiRequests(std::string_view json,
    const std::map<std::string, std::string>& variables,
    std::vector<ApiRequest>& requests, std::string& error);
// Resolves a selected profile while preventing secret-marked values from being
// inserted into the URL scheme or authority, where they could appear in the
// destination confirmation or DNS. Secret values in paths, queries, headers
// and bodies remain plaintext and are sent only when the caller sends a request.
[[nodiscard]] bool parseApiRequestsFromEnvironment(std::string_view json,
    const ApiEnvironment& environment, std::vector<ApiRequest>& requests, std::string& error);

// A byte edit against the exact collection snapshot supplied to the function.
// The caller must check the document identity and revision before applying it.
struct ApiCollectionEdit { std::size_t offset = 0, length = 0; std::string replacement; };
[[nodiscard]] bool listApiRequestTemplates(std::string_view collection, std::vector<std::string>& names, std::string& error);
// Operates on raw templates without resolving variables or logging values.
// Unknown JSON fields and all unrelated bytes are preserved. Removal should be
// explicitly confirmed by the user before the edit is applied.
[[nodiscard]] bool renameApiRequest(std::string_view collection, std::size_t index,
    std::string_view newName, ApiCollectionEdit& edit, std::string& error);
[[nodiscard]] bool removeApiRequest(std::string_view collection, std::size_t index,
    ApiCollectionEdit& edit, std::string& error);
// Adds a new placeholder request, without using any resolved request values.
[[nodiscard]] bool appendApiRequestTemplate(std::string_view collection, std::string_view name,
    ApiCollectionEdit& edit, std::string& error);

struct ApiResponse {
    long status = 0;
    std::string headers, body, error;
    // Wall time spent in transport, in milliseconds; zero for validation failures.
    std::int64_t elapsedMilliseconds = 0;
};
// Explicitly saves a complete response body byte-for-byte, including binary data.
// The project-relative destination must be new and its existing parent path must
// not contain symlinks. Response bodies may contain sensitive data.
[[nodiscard]] bool saveApiResponseBody(workspace::Project& project, std::string_view relativePath,
    const ApiResponse& response, std::string& error, const std::atomic_bool* cancel = nullptr);
// An explicit, one-shot request. Never follows redirects or logs resolved values.
// Response headers and body are limited to 64 KiB and 4 MiB respectively.
// Framing headers and URL credentials are rejected. Redirects and proxy use
// are disabled; errors do not include request URL, headers, or response content.
// Calling this function contacts the requested host; the caller must obtain user consent.
[[nodiscard]] ApiResponse sendApiRequest(const ApiRequest& request, const std::atomic_bool* cancel = nullptr);
}
