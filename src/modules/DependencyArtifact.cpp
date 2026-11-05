#include "modules/DependencyRisk.hpp"
#include "modules/DependencyGoZip.hpp"
#include "modules/Sha2Vendor.hpp"
#include "workspace/Project.hpp"
#include <picosha2.h>
#include <algorithm>
#include <array>
#include <string>
namespace lucid::modules {
namespace {
std::string base64(const unsigned char* data, std::size_t length) {
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result; result.reserve(((length + 2) / 3) * 4);
    for (std::size_t i = 0; i < length; i += 3) {
        const auto count = std::min<std::size_t>(3, length - i);
        const unsigned value = static_cast<unsigned>(data[i]) << 16 | (count > 1 ? static_cast<unsigned>(data[i + 1]) << 8 : 0U) | (count > 2 ? static_cast<unsigned>(data[i + 2]) : 0U);
        result += alphabet[(value >> 18) & 63U]; result += alphabet[(value >> 12) & 63U];
        result += count > 1 ? alphabet[(value >> 6) & 63U] : '='; result += count > 2 ? alphabet[value & 63U] : '=';
    }
    return result;
}
bool cargoChecksum(std::string_view value) {
    return value.starts_with("sha256:") && value.size() == 71 && std::all_of(value.begin() + 7, value.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}
enum class Algorithm { None, Sha256, Sha384, Sha512 };
Algorithm npmAlgorithm(std::string_view value) {
    Algorithm algorithm = Algorithm::None; std::size_t encoded = 0, padding = 0;
    if (value.starts_with("sha256-")) { algorithm = Algorithm::Sha256; encoded = 44; padding = 1; }
    else if (value.starts_with("sha384-")) { algorithm = Algorithm::Sha384; encoded = 64; }
    else if (value.starts_with("sha512-")) { algorithm = Algorithm::Sha512; encoded = 88; padding = 2; }
    if (algorithm == Algorithm::None || value.size() != 7 + encoded) return Algorithm::None;
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (std::size_t i = 7; i < value.size() - padding; ++i) if (alphabet.find(value[i]) == std::string_view::npos) return Algorithm::None;
    for (std::size_t i = value.size() - padding; i < value.size(); ++i) if (value[i] != '=') return Algorithm::None;
    const auto last = alphabet.find(value[value.size() - padding - 1]);
    if ((padding == 1 && (last & 3U) != 0) || (padding == 2 && (last & 15U) != 0)) return Algorithm::None;
    return algorithm;
}
}
DependencyArtifactVerification verifyDependencyArtifact(const workspace::Project& project, const DependencyRiskPackage& package,
    std::string_view relativePath, const std::atomic_bool* cancel) {
    DependencyArtifactVerification result;
    if (cancel && cancel->load()) { result.status = DependencyArtifactStatus::Cancelled; return result; }
    const bool cargo = package.ecosystem == "crates.io" && cargoChecksum(package.declaredChecksum);
    const bool go = package.ecosystem == "Go" && package.declaredChecksum.starts_with("h1:") && npmAlgorithm("sha256-" + package.declaredChecksum.substr(3)) == Algorithm::Sha256;
    const Algorithm algorithm = (cargo || go) ? Algorithm::Sha256 : package.ecosystem == "npm" ? npmAlgorithm(package.declaredChecksum) : Algorithm::None;
    if (algorithm == Algorithm::None) { result.status = DependencyArtifactStatus::Unsupported; result.error = "No supported SHA-2 lockfile checksum"; return result; }
    std::string bytes, error;
    if (!project.readFileBounded(relativePath, bytes, error, 64U * 1024U * 1024U, cancel)) {
        if (cancel && cancel->load()) result.status = DependencyArtifactStatus::Cancelled;
        else { result.status = DependencyArtifactStatus::Error; result.error = "Could not read the selected project artifact"; }
        return result;
    }
    picosha2::hash256_one_by_one sha256; sha384_ctx sha384{}; sha512_ctx sha512{};
    if (algorithm == Algorithm::Sha384) lucid_sha384_init(&sha384);
    if (algorithm == Algorithm::Sha512) lucid_sha512_init(&sha512);
    const std::size_t total = bytes.size();
    for (std::size_t i = 0; i < total;) {
        if (cancel && cancel->load()) { result.status = DependencyArtifactStatus::Cancelled; return result; }
        const auto count = std::min<std::size_t>(64U * 1024U, total - i);
        sha256.process(bytes.begin() + static_cast<std::ptrdiff_t>(i), bytes.begin() + static_cast<std::ptrdiff_t>(i + count));
        const auto* data = reinterpret_cast<const uint8*>(bytes.data() + i);
        if (algorithm == Algorithm::Sha384) lucid_sha384_update(&sha384, data, count);
        if (algorithm == Algorithm::Sha512) lucid_sha512_update(&sha512, data, count);
        i += count;
    }
    if (cancel && cancel->load()) { result.status = DependencyArtifactStatus::Cancelled; return result; }
    sha256.finish(); std::array<picosha2::byte_t, picosha2::k_digest_size> digest{}; sha256.get_hash_bytes(digest.begin(), digest.end());
    result.sha256 = picosha2::bytes_to_hex_string(digest); result.bytesRead = total;
    std::string actual;
    if (go && !hashGoModuleZip(bytes, package.name, package.version, actual, error, cancel)) {
        result.sha256.clear(); result.bytesRead = 0;
        if (cancel && cancel->load()) result.status = DependencyArtifactStatus::Cancelled;
        else { result.status = DependencyArtifactStatus::Error; result.error = error; }
        return result;
    }
    if (algorithm == Algorithm::Sha384) { std::array<uint8, SHA384_DIGEST_SIZE> bytes{}; lucid_sha384_final(&sha384, bytes.data()); actual = "sha384-" + base64(bytes.data(), bytes.size()); }
    else if (algorithm == Algorithm::Sha512) { std::array<uint8, SHA512_DIGEST_SIZE> bytes{}; lucid_sha512_final(&sha512, bytes.data()); actual = "sha512-" + base64(bytes.data(), bytes.size()); }
    else if (!go) actual = cargo ? "sha256:" + result.sha256 : "sha256-" + base64(digest.data(), digest.size());
    result.status = actual == package.declaredChecksum ? DependencyArtifactStatus::Matched : DependencyArtifactStatus::Mismatched;
    return result;
}
}
