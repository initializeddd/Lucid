#include "modules/SecretScan.hpp"
#include <atomic>
#include <string>
using namespace lucid::modules;
int main() {
    const std::string text = "β " + std::string("ASIA") + std::string(16, 'A') + "\n" +
        std::string("github_pat_") + std::string(22, 'a') + "_" + std::string(59, 'b') + "\n-----BEGIN ENCRYPTED PRIVATE KEY-----\n";
    const auto result = scanSecretText("fixture", text);
    if (result.incomplete || result.findings.size() != 3 || result.findings[0].line != 0 || result.findings[0].column != 2 ||
        result.findings[0].kind != "AWS access key ID" || result.findings[1].kind != "GitHub fine-grained token" ||
        result.findings[2].kind != "Private key" || result.findings[0].contentSha256.size() != 64 ||
        result.findings[0].contentSha256 != result.findings[1].contentSha256) return 1;
    const auto altered = scanSecretText("fixture", text + "x");
    if (altered.findings.empty() || altered.findings[0].contentSha256 == result.findings[0].contentSha256) return 2;
    const auto oversized = scanSecretText("fixture", std::string(16U*1024U*1024U+1,'x'));
    if (!scanSecretText("fixture", text, nullptr, 1).incomplete || !oversized.incomplete || oversized.skipped != 1) return 3;
    std::atomic_bool cancelled{true}; if (!scanSecretText("fixture", text, &cancelled).incomplete) return 4;
    if (!scanSecretText("fixture", "x" + std::string("ASIA") + std::string(16, 'A')).findings.empty()) return 5;
    const auto extraKeys = scanSecretText("fixture", "-----BEGIN DSA PRIVATE KEY-----\n-----BEGIN PGP PRIVATE KEY BLOCK-----\n-----BEGIN PGP PUBLIC KEY BLOCK-----\n");
    if (extraKeys.findings.size() != 2 || extraKeys.findings[0].kind != "Private key" || extraKeys.findings[1].kind != "Private key" || extraKeys.findings[1].line != 1) return 6;
    const auto token = std::string("glpat-") + "abCD09_-" + std::string(12, 'Z');
    const auto gitlab = scanSecretText("fixture", "\xce\xb2 \"" + token + "\"\n");
    if (gitlab.incomplete || gitlab.findings.size() != 1 || gitlab.findings[0].kind != "GitLab access token" || gitlab.findings[0].line != 0 || gitlab.findings[0].column != 3 || gitlab.findings[0].path != "fixture" || gitlab.findings[0].contentSha256.size() != 64) return 7;
    for (const auto& invalid : {"x" + token, "-" + token, token + "x", token + "-", token.substr(0, token.size()-1), std::string("glpat-") + std::string(19,'A') + "!"})
        if (!scanSecretText("fixture", invalid).findings.empty()) return 8;
    if (!scanSecretText("fixture", token, nullptr, 0).incomplete) return 9;
    return 0;
}
