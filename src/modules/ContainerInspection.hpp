#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
struct ContainerInfo { std::string id, image, names, status, state; };
struct ContainerListing { std::vector<ContainerInfo> containers; std::vector<std::string> warnings; };
struct ContainerLog { std::string text; bool truncated = false; };
struct ContainerDetails { std::string id, name, image, status, health; std::vector<std::string> mounts, ports; bool incomplete = false; };
// Accepts Docker ps --format json (JSON lines) or Podman ps --format json
// (JSON array). Unknown fields are reported; this does not contact a runtime.
[[nodiscard]] bool parseContainerListing(std::string_view input, ContainerListing& output, std::string& error);
// Preserve raw log text up to limit. Logs can contain sensitive data.
[[nodiscard]] ContainerLog parseContainerLog(std::string_view input, std::size_t limit = 1024U * 1024U);
// Parses one Docker or Podman inspect result. Only selected metadata is retained;
// environment variables, credentials and command arguments are never returned.
[[nodiscard]] bool parseContainerDetails(std::string_view input, ContainerDetails& output, std::string& error);
}
