#pragma once
#include <atomic>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::workspace { class Project; }
namespace lucid::modules {
struct DependencyRiskPackage {
    std::string path, name, version, ecosystem = "npm";
    bool direct = false, dev = false, optional = false;
    std::vector<std::string> vulnerabilities; // OSV IDs, not an assessment of exploitability.
    // Declared by the lockfile only; neither the source nor the digest has been
    // independently verified. Empty checksum means missing or unsupported.
    std::string declaredSource = {}, declaredChecksum = {};
};
struct DependencyRiskReport {
    std::vector<DependencyRiskPackage> packages;
    std::vector<std::string> limitations;
};
// Supports npm package-lock v1 and v2/v3 with a packages map. Directness is
// unknown for v1. Builds one OSV
// query per returned package in exactly that order; no network request is made.
[[nodiscard]] bool parseNpmDependencyRisk(std::string_view lockfile, DependencyRiskReport& report, std::string& error);
// Enumerates exact module/version checksum entries in go.sum. This is not the
// selected build list: stale checksums may cause false positives and local
// replacements and missing checksums are not assessed. No network is used.
[[nodiscard]] bool parseGoSumDependencyRisk(std::string_view goSum, DependencyRiskReport& report, std::string& error);
// Supports canonical crates.io registry entries in Cargo.lock v3/v4. Workspace,
// Git and other registry entries are skipped and reported as coverage limits.
[[nodiscard]] bool parseCargoDependencyRisk(std::string_view lockfile, DependencyRiskReport& report, std::string& error);
// Reviews exact public-PyPI entries in Pipfile.lock; environment markers and
// dependency reachability are not evaluated. No network request is made.
[[nodiscard]] bool parsePipfileDependencyRisk(std::string_view lockfile, DependencyRiskReport& report, std::string& error);
enum class DependencyArtifactStatus { Matched, Mismatched, Unsupported, Error, Cancelled };
struct DependencyArtifactVerification {
    DependencyArtifactStatus status = DependencyArtifactStatus::Error;
    std::string sha256, error;
    std::size_t bytesRead = 0;
};
// Explicitly reads a selected project-contained artifact (maximum 64 MiB) and
// compares its bytes with crates.io SHA-256, npm SHA-256/384/512 SRI, or a Go
// h1 module ZIP hash. ZIP64, other compression methods and oversized entries
// are rejected. No network or process is used.
// A match verifies only the bytes read against the lockfile declaration: it
// does not establish publisher identity, lockfile trust or current file state.
[[nodiscard]] DependencyArtifactVerification verifyDependencyArtifact(const workspace::Project& project,
    const DependencyRiskPackage& package, std::string_view relativePath, const std::atomic_bool* cancel = nullptr);
// Accept an OSV querybatch response whose results correspond exactly to report.packages.
// On failure the report remains unchanged. Does not fetch vulnerability details.
[[nodiscard]] bool applyOsvBatchResponse(std::string_view response, DependencyRiskReport& report, std::string& error);

struct OsvResponse { long status = 0; std::string body, error; };
using OsvTransport = std::function<OsvResponse(std::string_view body, const std::atomic_bool* cancel)>;
// Queries in batches of at most 100 and follows OSV's per-query pagination.
// Reports are replaced atomically only after all results are validated. At most
// 20 pages per package and 100,000 vulnerability IDs are accepted. A request
// sends package names and versions to OSV; callers must explain and obtain consent.
[[nodiscard]] bool queryOsvAdvisories(DependencyRiskReport& report, const OsvTransport& transport,
                                      std::string& error, const std::atomic_bool* cancel = nullptr);
// Explicit network transport: HTTPS to api.osv.dev only, no redirect following.
[[nodiscard]] OsvResponse requestOsvBatch(std::string_view body, const std::atomic_bool* cancel = nullptr);
struct OsvDetail { std::string id, summary, details; std::vector<std::string> aliases, fixedVersions, references; };
// Only canonical OSV IDs are allowed in the URL. No request occurs until called.
[[nodiscard]] OsvResponse requestOsvDetail(std::string_view id, const std::atomic_bool* cancel = nullptr);
[[nodiscard]] bool parseOsvDetail(std::string_view json, std::string_view expectedId, OsvDetail& detail, std::string& error);
}
