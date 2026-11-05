#include "modules/DependencyRisk.hpp"
#include <string>
using namespace lucid::modules;
int main() {
    const std::string lock = R"({"lockfileVersion":3,"packages":{"":{"dependencies":{"foo":"^1","@a/b":"^2"},"devDependencies":{"bar":"^3"}},"node_modules/foo":{"version":"1.2.3","resolved":"https://registry.npmjs.org/foo/-/foo-1.2.3.tgz","integrity":"sha256-AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="},"node_modules/@a/b":{"version":"2.0.0","resolved":"https://registry.npmjs.org/@a/b/-/b-2.0.0.tgz"},"node_modules/bar":{"version":"3.0.0","dev":true,"resolved":"https://registry.npmjs.org/bar/-/bar-3.0.0.tgz"},"node_modules/foo/node_modules/bar":{"version":"3.1.0","resolved":"https://registry.npmjs.org/bar/-/bar-3.1.0.tgz"},"node_modules/local":{"link":true,"resolved":"../local"}}})";
    DependencyRiskReport report; std::string error;
    bool ok = parseNpmDependencyRisk(lock, report, error) && report.packages.size() == 4 && report.limitations.size() == 4;
    ok = ok && report.packages[0].name == "foo" && report.packages[0].direct && report.packages[1].name == "@a/b" && report.packages[1].direct;
    ok = ok && report.packages[2].dev && report.packages[2].direct && !report.packages[3].direct;
    ok = ok && report.packages[0].declaredSource == "https://registry.npmjs.org/foo/-/foo-1.2.3.tgz" && report.packages[0].declaredChecksum == "sha256-AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=" && report.packages[1].declaredChecksum.empty();
    ok = ok && applyOsvBatchResponse(R"({"results":[{"vulns":[{"id":"OSV-1"}]},{},{},{}]})", report, error) && report.packages[0].vulnerabilities[0] == "OSV-1";
    ok = ok && !applyOsvBatchResponse(R"({"results":[{}]})", report, error) && report.packages[0].vulnerabilities[0] == "OSV-1";
    ok = ok && !applyOsvBatchResponse(R"({"results":[{"next_page_token":"x"},{},{},{}]})", report, error);
    ok = ok && !parseNpmDependencyRisk(R"({"lockfileVersion":1,"dependencies":[]})", report, error) && report.packages.empty();
    const auto legacy = R"({"lockfileVersion":1,"dependencies":{"a":{"version":"1.0.0","resolved":"https://registry.npmjs.org/a/-/a-1.0.0.tgz","dependencies":{"@scope/b":{"version":"2.1.0","resolved":"https://registry.npmjs.org/@scope/b/-/b-2.1.0.tgz"}}},"alias":{"version":"1.0.0","resolved":"https://example.com/alias.tgz"}}})";
    ok = ok && parseNpmDependencyRisk(legacy, report, error) && report.packages.size()==2 && report.packages[0].name=="a" && !report.packages[0].direct && report.packages[1].name=="@scope/b" && report.packages[1].path=="node_modules/a/node_modules/@scope/b" && report.limitations.size()==4;
    ok = ok && !parseNpmDependencyRisk(R"({"lockfileVersion":1,"dependencies":{"a":{"version":"1.0.0","dependencies":[]}}})", report, error) && report.packages.empty();
    ok = ok && !parseNpmDependencyRisk(R"({"lockfileVersion":3,"lockfileVersion":3,"packages":{"":{}}})", report, error);
    ok = ok && parseNpmDependencyRisk(R"({"lockfileVersion":2,"packages":{"":{},"node_modules/a":{"version":"^1"}}})", report, error) && report.packages.empty();
    ok = ok && parseNpmDependencyRisk(R"({"lockfileVersion":3,"packages":{"":{},"node_modules/a":{"version":"1.0.0","resolved":"https://registry.npmjs.org/a/-/a-1.0.0.tgz","integrity":"sha256-malformed"}}})", report, error) && report.packages.size() == 1 && report.packages[0].declaredChecksum.empty();
    const std::string ambiguous = R"({"lockfileVersion":3,"packages":{"":{"dependencies":{"alias":"npm:real@1.0.0","missing":"*"}},"node_modules/alias":{"version":"1.0.0","resolved":"https://registry.npmjs.org/real/-/real-1.0.0.tgz"},"node_modules/foo/node_modules/alias":{"version":"1.0.0","resolved":"https://registry.npmjs.org/real/-/real-1.0.0.tgz"},"node_modules/missing":{"version":"1.0.0"},"node_modules/spoof":{"version":"1.0.0","resolved":"https://registry.npmjs.org/spoof/-/spoof-1.0.0.tgz.evil"},"node_modules/other":{"name":"real","version":"1.0.0","resolved":"https://registry.npmjs.org/real/-/real-1.0.0.tgz"}}})";
    ok = ok && parseNpmDependencyRisk(ambiguous, report, error) && report.packages.empty() && report.limitations.size() == 5;
    ok = ok && parseNpmDependencyRisk(lock, report, error);
    auto previous = report;
    unsigned calls = 0;
    const OsvTransport paged = [&](std::string_view request, const std::atomic_bool*) -> OsvResponse {
        ++calls;
        if (request.find("node_modules") != std::string_view::npos || request.find("registry.npmjs.org") != std::string_view::npos || request.find("sha256-") != std::string_view::npos || request.find("\"ecosystem\":\"npm\"") == std::string_view::npos) return {500, {}, {}};
        if (calls == 1) return {200, R"({"results":[{"vulns":[{"id":"GHSA-first"}],"next_page_token":"next"},{},{},{}]})", {}};
        if (calls == 2 && request.find("\"page_token\":\"next\"") != std::string_view::npos) return {200, R"({"results":[{"vulns":[{"id":"GHSA-second"}]}]})", {}};
        return {500, {}, {}};
    };
    ok = ok && queryOsvAdvisories(report, paged, error) && calls == 2 && report.packages[0].vulnerabilities.size() == 2 && report.packages[0].vulnerabilities[1] == "GHSA-second";
    previous = report;
    unsigned invalidCalls = 0;
    const OsvTransport invalid = [&](std::string_view, const std::atomic_bool*) { return OsvResponse{200, ++invalidCalls == 1 ? R"({"results":[{"next_page_token":"repeated"},{},{},{}]})" : R"({"results":[{"next_page_token":"repeated"}]})", {}}; };
    ok = ok && !queryOsvAdvisories(report, invalid, error) && invalidCalls == 2 && report.packages[0].vulnerabilities == previous.packages[0].vulnerabilities;
    std::atomic_bool cancelled{true}; calls = 0;
    ok = ok && !queryOsvAdvisories(report, paged, error, &cancelled) && calls == 0 && report.packages[0].vulnerabilities == previous.packages[0].vulnerabilities;
    report.packages.assign(101, previous.packages[0]); calls = 0;
    const OsvTransport batches = [&](std::string_view body, const std::atomic_bool*) -> OsvResponse {
        ++calls; std::size_t count = 0, pos = 0;
        while ((pos = body.find("\"package\":", pos)) != std::string_view::npos) { ++count; ++pos; }
        if (count != (calls == 1 ? 100U : 1U)) return {500, {}, {}};
        std::string response = "{\"results\":[";
        for (std::size_t i = 0; i < count; ++i) { if (i) response += ','; response += "{}"; }
        return {200, response + "]}", {}};
    };
    ok = ok && queryOsvAdvisories(report, batches, error) && calls == 2 && report.packages[100].vulnerabilities.empty();
    OsvDetail detail;
    ok = ok && parseOsvDetail(R"({"id":"GHSA-1","summary":"Issue","details":"Description","aliases":["CVE-1"],"affected":[{"package":{"ecosystem":"npm","name":"foo"},"ranges":[{"events":[{"introduced":"0"},{"fixed":"2.0.0"}]}]}],"references":[{"url":"https://example.test/advisory"}]})", "GHSA-1", detail, error)
        && detail.summary == "Issue" && detail.fixedVersions == std::vector<std::string>{"foo 2.0.0"} && detail.references.size() == 1;
    ok = ok && !parseOsvDetail(R"({"id":"GHSA-2"})", "GHSA-1", detail, error) && detail.id.empty();
    // Mirrors the shape of a public OSV result: multiple npm packages, a
    // last_affected range with no announced fix, and a different ecosystem.
    ok = ok && parseOsvDetail(R"({"id":"GHSA-example","aliases":["CVE-2021-1"],"affected":[{"package":{"name":"lodash","ecosystem":"npm"},"ranges":[{"type":"ECOSYSTEM","events":[{"introduced":"0"},{"fixed":"4.17.21"}]}]},{"package":{"name":"lodash.template","ecosystem":"npm"},"ranges":[{"events":[{"introduced":"0"},{"last_affected":"4.5.0"}]}]},{"package":{"name":"lodash-rails","ecosystem":"RubyGems"},"ranges":[{"events":[{"fixed":"4.17.21"}]}]}],"references":[{"type":"ADVISORY","url":"https://example.test/advisory"}]})", "GHSA-example", detail, error)
        && detail.fixedVersions == std::vector<std::string>{"lodash 4.17.21"} && detail.references.size() == 1;
    ok = ok && !parseOsvDetail(R"({"id":"GHSA-1","id":"GHSA-1"})", "GHSA-1", detail, error);
    ok = ok && !requestOsvDetail("../private").error.empty();
    const std::string hash = "h1:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
    const std::string goSum = "example.com/a v1.2.3 " + hash + "\n" +
        "example.com/a v1.2.3/go.mod " + hash + "\n" +
        "example.com/a v1.2.3 " + hash + "\n" +
        "github.com/Org/lib v0.0.0-20240101000000-abcdef123456 " + hash + "\n" +
        "../bad v1.0.0 " + hash + "\n" + "example.com/b v1.0.0 invalid\n";
    ok = ok && parseGoSumDependencyRisk(goSum, report, error) && report.packages.size() == 2 &&
        report.packages[0].name == "example.com/a" && report.packages[0].version == "v1.2.3" &&
        report.packages[0].ecosystem == "Go" && !report.packages[0].direct && report.limitations.size() == 5;
    ok = ok && report.packages[0].declaredChecksum == hash && report.packages[0].declaredSource.find("source not established") != std::string::npos;
    calls = 0;
    const OsvTransport goTransport = [&](std::string_view body, const std::atomic_bool*) -> OsvResponse {
        ++calls;
        if (body.find("\"ecosystem\":\"Go\"") == std::string_view::npos || body.find("example.com/a") == std::string_view::npos || body.find("v1.2.3") == std::string_view::npos) return {500, {}, {}};
        return {200, R"({"results":[{"vulns":[{"id":"GO-1"}]},{}]})", {}};
    };
    ok = ok && queryOsvAdvisories(report, goTransport, error) && calls == 1 && report.packages[0].vulnerabilities == std::vector<std::string>{"GO-1"};
    const auto conflicting = "example.com/a v1.2.3 " + hash + "\nexample.com/a v1.2.3 h1:BAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\n";
    ok = ok && !parseGoSumDependencyRisk(conflicting, report, error) && report.packages.empty() && error.find("Conflicting") != std::string::npos;
    ok = ok && !parseGoSumDependencyRisk(std::string(8U * 1024U * 1024U + 1, 'x'), report, error) && report.packages.empty();
    const std::string cargo = "version = 4\n[[package]]\nname = \"serde\"\nversion = \"1.0.200\"\nsource = \"registry+https://github.com/rust-lang/crates.io-index\"\nchecksum = \"" + std::string(64, 'a') + "\"\ndependencies = [\n \"serde_derive\",\n]\n[[package]]\nname = \"local\"\nversion = \"1.0.0\"\n[[package]]\nname = \"foreign\"\nversion = \"1.0.0\"\nsource = \"git+https://example.test/repo\"\nchecksum = \"" + std::string(64, 'a') + "\"\n";
    ok = ok && parseCargoDependencyRisk(cargo, report, error) && report.packages.size() == 1 && report.packages[0].name == "serde" && report.packages[0].ecosystem == "crates.io" && report.limitations.size() == 4;
    ok = ok && report.packages[0].declaredSource == "registry+https://github.com/rust-lang/crates.io-index" && report.packages[0].declaredChecksum == "sha256:" + std::string(64, 'a');
    calls = 0;
    const OsvTransport cargoTransport = [&](std::string_view body, const std::atomic_bool*) -> OsvResponse {
        ++calls; if (body.find("\"ecosystem\":\"crates.io\"") == std::string_view::npos || body.find("serde") == std::string_view::npos) return {500, {}, {}};
        return {200, R"({"results":[{"vulns":[{"id":"RUSTSEC-1"}]}]})", {}};
    };
    ok = ok && queryOsvAdvisories(report, cargoTransport, error) && calls == 1 && report.packages[0].vulnerabilities == std::vector<std::string>{"RUSTSEC-1"};
    ok = ok && !parseCargoDependencyRisk("version = 2\n", report, error) && report.packages.empty();
    ok = ok && !parseCargoDependencyRisk("[[package]]\nversion = \"1.0.0\"\n", report, error) && report.packages.empty();
    ok = ok && !parseCargoDependencyRisk("version = 4\n[[package]]\nname = \"one\"\nname = \"two\"\n", report, error) && report.packages.empty();
    ok = ok && !parseCargoDependencyRisk("version = 4\nx = \"\"\"multiline\"\"\"\n", report, error) && report.packages.empty();
    ok = ok && !parseCargoDependencyRisk("version = 4\n[[package]]\nname = \"x\"\ndependencies = [\n\"y\",\n", report, error) && report.packages.empty();
    ok = ok && !parseCargoDependencyRisk(std::string(8U * 1024U * 1024U + 1, 'x'), report, error) && report.packages.empty();
    return ok ? 0 : 1;
}
