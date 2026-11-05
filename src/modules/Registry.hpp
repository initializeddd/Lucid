#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <functional>

namespace lucid::modules {

struct RegistryEntry {
    std::string id, package, version, name, description;
    std::vector<std::string> requirements;
};
using RegistryRequest = std::function<bool(std::string_view, const std::vector<std::string>&,
                                           std::string&, std::string&)>;

// Catalog entries refer to GHCR packages as owner/name and version tags. Tags
// can be moved by the publisher; layer digest validation is not a signature.
[[nodiscard]] bool parseCatalog(std::string_view json, std::vector<RegistryEntry>& entries,
                                std::string& error);
// Fetch a single Lucid JSON layer from a public GHCR OCI package, verifying its digest.
[[nodiscard]] bool fetchPackage(std::string_view package, std::string_view version,
                                std::string_view mediaType, std::string& contents,
                                std::string& error);
// The transport overload permits deterministic verification of the registry protocol.
[[nodiscard]] bool fetchPackage(std::string_view package, std::string_view version,
                                std::string_view mediaType, std::string& contents,
                                std::string& error, const RegistryRequest& request);

} // namespace lucid::modules
