#include "modules/SecretScan.hpp"
#include "workspace/git/GitReview.hpp"
#include <algorithm>
#include <picosha2.h>
#include <string_view>

namespace lucid::modules {
namespace {
constexpr std::size_t maxFileBytes = 16U * 1024U * 1024U, maxTotalBytes = 256U * 1024U * 1024U, maxFiles = 10000;
bool asciiWord(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}
bool alphaNumeric(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}
bool gitlabTokenCharacter(unsigned char c) { return alphaNumeric(c) || c == '_' || c == '-'; }
}
SecretScanResult scanSecretText(std::string_view path, std::string_view contents, const std::atomic_bool* cancel, std::size_t limit) {
    SecretScanResult result;
    if (contents.size() > maxFileBytes) { result.incomplete = true; ++result.skipped; return result; }
    std::size_t line = 0, column = 0; std::string digest;
        for (std::size_t i = 0; i < contents.size(); ++i) {
            if ((i & 4095U) == 0 && cancel && cancel->load()) { result.incomplete = true; return result; }
            const auto tail = std::string_view(contents).substr(i);
            std::string_view kind;
            if ((i == 0 || !asciiWord(static_cast<unsigned char>(contents[i - 1]))) && (tail.starts_with("AKIA") || tail.starts_with("ASIA")) && tail.size() >= 20 &&
                std::all_of(tail.begin() + 4, tail.begin() + 20, [](unsigned char c) { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); }) &&
                (tail.size() == 20 || !asciiWord(static_cast<unsigned char>(tail[20])))) kind = "AWS access key ID";
            else if ((i == 0 || !asciiWord(static_cast<unsigned char>(contents[i - 1]))) &&
                     (tail.starts_with("ghp_") || tail.starts_with("gho_") || tail.starts_with("ghu_") || tail.starts_with("ghs_") || tail.starts_with("ghr_")) &&
                     tail.size() >= 40 && std::all_of(tail.begin() + 4, tail.begin() + 40, [](unsigned char c) { return alphaNumeric(c); }) &&
                     (tail.size() == 40 || !asciiWord(static_cast<unsigned char>(tail[40])))) kind = "GitHub token";
            else if ((i == 0 || !asciiWord(static_cast<unsigned char>(contents[i - 1]))) && tail.starts_with("github_pat_") &&
                     tail.size() >= 93 && std::all_of(tail.begin() + 11, tail.begin() + 33, [](unsigned char c) { return alphaNumeric(c); }) &&
                     tail[33] == '_' && std::all_of(tail.begin() + 34, tail.begin() + 93, [](unsigned char c) { return alphaNumeric(c); }) &&
                     (tail.size() == 93 || !asciiWord(static_cast<unsigned char>(tail[93])))) kind = "GitHub fine-grained token";
            else if ((i == 0 || !gitlabTokenCharacter(static_cast<unsigned char>(contents[i - 1]))) && tail.starts_with("glpat-") &&
                     tail.size() >= 26 && std::all_of(tail.begin() + 6, tail.begin() + 26, gitlabTokenCharacter) &&
                     (tail.size() == 26 || !gitlabTokenCharacter(static_cast<unsigned char>(tail[26])))) kind = "GitLab access token";
            else if (tail.starts_with("-----BEGIN ENCRYPTED PRIVATE KEY-----") || tail.starts_with("-----BEGIN PRIVATE KEY-----") || tail.starts_with("-----BEGIN RSA PRIVATE KEY-----") ||
                     tail.starts_with("-----BEGIN EC PRIVATE KEY-----") || tail.starts_with("-----BEGIN DSA PRIVATE KEY-----") ||
                     tail.starts_with("-----BEGIN OPENSSH PRIVATE KEY-----") || tail.starts_with("-----BEGIN PGP PRIVATE KEY BLOCK-----")) kind = "Private key";
            if (!kind.empty()) {
                if (result.findings.size() >= limit) { result.incomplete = true; return result; }
                if (digest.empty()) digest = picosha2::hash256_hex_string(contents.begin(), contents.end());
                result.findings.push_back({std::string(path), std::string(kind), line, column, digest});
            }
            const auto c = static_cast<unsigned char>(contents[i]);
            if (c == '\n') { ++line; column = 0; }
            else if ((c & 0xc0U) != 0x80U) ++column;
        }
    return result;
}

SecretScanResult scanSecrets(const workspace::Project& project, const std::atomic_bool* cancel, std::size_t limit) {
    SecretScanResult result;
    std::size_t bytes = 0, files = 0;
    for (const auto& path : project.files()) {
        if (++files > maxFiles) { result.incomplete = true; return result; }
        if (cancel && cancel->load()) { result.incomplete = true; return result; }
        std::string contents, error;
        if (!project.readFile(path, contents, error, cancel)) {
            if (cancel && cancel->load()) { result.incomplete = true; return result; }
            ++result.skipped; result.incomplete = true; continue;
        }
        if (contents.size() > maxTotalBytes - bytes) { result.incomplete = true; return result; }
        bytes += contents.size();
        if (std::find(contents.begin(), contents.end(), '\0') != contents.end()) { ++result.skipped; result.incomplete = true; continue; }
        auto file = scanSecretText(path, contents, cancel, limit - result.findings.size());
        result.skipped += file.skipped;
        result.findings.insert(result.findings.end(), std::make_move_iterator(file.findings.begin()), std::make_move_iterator(file.findings.end()));
        if (file.incomplete) { result.incomplete = true; return result; }
    }
    return result;
}

SecretScanResult scanStagedSecrets(const std::filesystem::path& root, const workspace::GitCommitPreview& preview, const std::atomic_bool* cancel, std::size_t limit) {
    SecretScanResult result;
    std::size_t bytes = 0, files = 0;
    for (const auto& path : preview.paths) {
        if (++files > maxFiles) { result.incomplete = true; return result; }
        if (cancel && cancel->load()) { result.incomplete = true; return result; }
        std::string contents, error; bool present = false;
        if (!workspace::gitReadReviewedBlob(root, preview, path, contents, present, error)) { ++result.skipped; result.incomplete = true; continue; }
        if (!present) continue;
        if (contents.size() > maxTotalBytes - bytes) { result.incomplete = true; return result; }
        bytes += contents.size();
        if (std::find(contents.begin(), contents.end(), '\0') != contents.end()) { ++result.skipped; result.incomplete = true; continue; }
        auto file = scanSecretText(path, contents, cancel, limit - result.findings.size());
        result.skipped += file.skipped;
        result.findings.insert(result.findings.end(), std::make_move_iterator(file.findings.begin()), std::make_move_iterator(file.findings.end()));
        if (file.incomplete) { result.incomplete = true; return result; }
    }
    workspace::GitCommitPreview current; std::string error;
    if (!workspace::gitCommitPreview(root, current, error) || current.indexTree != preview.indexTree || current.head != preview.head || current.paths != preview.paths) result.incomplete = true;
    return result;
}
} // namespace lucid::modules
