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

// Derive a valid local module ID from a GHCR repository. Existing simple names
// remain unchanged; dotted or long names receive a stable hash suffix. The
// installer still refuses to overwrite any existing module with the same ID.
[[nodiscard]] bool packageModuleId(std::string_view package, std::string& id, std::string& error);

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
