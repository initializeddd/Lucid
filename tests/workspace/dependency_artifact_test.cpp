#include "modules/DependencyRisk.hpp"
#include "workspace/Project.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
using namespace lucid::modules;
int main() {
    const auto root = std::filesystem::temp_directory_path() / ("lucid-dependency-artifact-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path, ec); } } cleanup{root};
    std::filesystem::create_directories(root); bool created = false; std::string error;
    auto project = lucid::workspace::Project::openOrCreate(root, created, error, false); if (!error.empty()) return 1;
    { std::ofstream file(root / "package.tgz", std::ios::binary); file << "abc"; }
    DependencyRiskPackage package; package.ecosystem = "npm"; package.declaredChecksum = "sha256-ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0=";
    const std::string hex = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    auto result = verifyDependencyArtifact(project, package, "package.tgz");
    if (result.status != DependencyArtifactStatus::Matched || result.sha256 != hex || result.bytesRead != 3 || !result.error.empty()) return 2;
    package.declaredChecksum = "sha384-ywB1P0WjXou1oD1pmsZQBycsMqsO3tFjGotgWkP/W+2AhgcroefMI1i67KE0yCWn";
    if (verifyDependencyArtifact(project, package, "package.tgz").status != DependencyArtifactStatus::Matched) return 12;
    package.declaredChecksum = "sha512-3a81oZNherrMQXNJriBBMRLm+k6JqX6iCp7u5ktV05ohkpkqJ0/BqDa6PCOj/uu9RU1EI2Q86A4qmslPpUyknw==";
    if (verifyDependencyArtifact(project, package, "package.tgz").status != DependencyArtifactStatus::Matched) return 13;
    package.declaredChecksum[7] = '4';
    if (verifyDependencyArtifact(project, package, "package.tgz").status != DependencyArtifactStatus::Mismatched) return 19;
    package.declaredChecksum = "sha512-3a81oZNherrMQXNJriBBMRLm+k6JqX6iCp7u5ktV05ohkpkqJ0/BqDa6PCOj/uu9RU1EI2Q86A4qmslPpUyknx==";
    if (verifyDependencyArtifact(project, package, "package.tgz").status != DependencyArtifactStatus::Unsupported) return 14;
    { std::ofstream file(root / "empty", std::ios::binary); }
    package.declaredChecksum = "sha384-OLBgp1GsljhM2TJ+sbHjaiH9txEUvgdDTAzHv2P24donTt6/529l+9Ua0vFImLlb";
    if (verifyDependencyArtifact(project, package, "empty").status != DependencyArtifactStatus::Matched) return 15;
    package.declaredChecksum = "sha512-z4PhNX7vuL3xVChQ1m2AB9Yg5AULVxXcg/SpIdNs6c5H0NE8XYXysP+DGNKHfuwvY7kxvUdBeoGlODJ6+SfaPg==";
    if (verifyDependencyArtifact(project, package, "empty").status != DependencyArtifactStatus::Matched) return 16;
    { std::ofstream file(root / "stream", std::ios::binary); for (unsigned i = 0; i < 262144; ++i) file.put(static_cast<char>(i & 255)); }
    package.declaredChecksum = "sha384-DBgPRaDShV4NlOSBAuMYSAHBBAwnKLxbwApshpkliv5CkYfQJF93+uEnJLYo1q0U";
    if (verifyDependencyArtifact(project, package, "stream").status != DependencyArtifactStatus::Matched) return 17;
    package.declaredChecksum = "sha512-luM/6Hwkl5JUontoIHeapLLCX04LJzPXg5XxnfDJ+us3zpy19SRz1eX3wRzkGplv9LRwlyE0NTAiakUaVkUCnA==";
    if (verifyDependencyArtifact(project, package, "stream").status != DependencyArtifactStatus::Matched) return 18;
    struct Vector { std::size_t length; const char* sha384; const char* sha512; };
    for (const auto& vector : {
        Vector{111,"PDeVUFHLXDAm+U1VHVteKsONVyrk4HFyCF/tgfhGa4+Q3COo/83qC42OWOj9rMgK","+pEhx7MrngFzPQNM/HjL9n+SbH7YPoIgDvhoGBlpIXYLS+/0hATfgRuVOCgnRGFnPGjQTil7DreytNYPxrVmog=="},
        Vector{112,"GH1OB8swYQPGmWe/VE0N++kEJXdZnHPDMKvAy2TGEjbV7VZe4ZEZ2MMXeaOPeR/N","wB0IDv1JJ3ahxDvSPdmdCi5ibUgeFnguddVMJQO13DK9BfDxujPlaLiP0tlwkptxnsuxUvWPEwpAfIgwYEtwyg=="},
        Vector{128,"7bEnMKNmCYs7K+rHWjvvGwlpsVxI4hY8I9lplPjRvvdgx+J/PEZNOCn1bA1TgIsL","tz0ZKaphWTTmGocVlrPzszNZ9CuBdWAuiffgbl9liiQ2Z4B+0wAxS5XKzdV58+M6vfvjUZCVGahG1GXFlYLzIQ=="},
        Vector{129,"Obb1p7DngdvEGfcuSbMOqsEPLJjEQDvGENoxBn/RtI8yQTjIYV0rSW0I1z1ehlMm","T2geC9U82ktaIEHMigby6r3kT7FslR+9W4dwLweuq2EVZbGcR/3jBYcXfruFLjlxu9jT/TDaGNcQN9+9mEIEKQ=="}
    }) {
        { std::ofstream file(root / "boundary", std::ios::binary | std::ios::trunc); file << std::string(vector.length, 'a'); }
        package.declaredChecksum = std::string("sha384-") + vector.sha384;
        if (verifyDependencyArtifact(project, package, "boundary").status != DependencyArtifactStatus::Matched) return 20;
        package.declaredChecksum = std::string("sha512-") + vector.sha512;
        if (verifyDependencyArtifact(project, package, "boundary").status != DependencyArtifactStatus::Matched) return 21;
    }
    package.ecosystem = "crates.io"; package.declaredChecksum = "sha256:" + hex;
    if (verifyDependencyArtifact(project, package, "package.tgz").status != DependencyArtifactStatus::Matched) return 3;
    package.declaredChecksum[7] = '0'; if (verifyDependencyArtifact(project, package, "package.tgz").status != DependencyArtifactStatus::Mismatched) return 4;
    package.ecosystem = "Go"; package.declaredChecksum = "h1:ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0=";
    if (verifyDependencyArtifact(project, package, "package.tgz").status != DependencyArtifactStatus::Error) return 5;
    const auto fixture = std::filesystem::path(__FILE__).parent_path().parent_path() / "fixtures/go-module-uppercase-v3.2.1.zip";
    std::filesystem::copy_file(fixture, root / "module.zip");
    package.name = "github.com/Masterminds/semver/v3"; package.version = "v3.2.1"; package.declaredChecksum = "h1:RN9w6+7QoMeJVGyfmbcgs28Br8cvmnucEXnY0rYXWg0=";
    if (verifyDependencyArtifact(project, package, "module.zip").status != DependencyArtifactStatus::Matched) return 22;
    package.declaredChecksum[3] = 'S'; if (verifyDependencyArtifact(project, package, "module.zip").status != DependencyArtifactStatus::Mismatched) return 23;
    package.ecosystem = "npm"; package.declaredChecksum = "sha512-a";
    if (verifyDependencyArtifact(project, package, "package.tgz").status != DependencyArtifactStatus::Unsupported) return 6;
    package.declaredChecksum = "sha256-ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa1=";
    if (verifyDependencyArtifact(project, package, "package.tgz").status != DependencyArtifactStatus::Unsupported) return 7;
    package.declaredChecksum = "sha256-ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0=";
    if (verifyDependencyArtifact(project, package, "../outside").status != DependencyArtifactStatus::Error) return 8;
    std::atomic_bool cancel{true}; if (verifyDependencyArtifact(project, package, "package.tgz", &cancel).status != DependencyArtifactStatus::Cancelled) return 9;
    { std::ofstream file(root / "package.tgz", std::ios::binary | std::ios::trunc); file << "different"; }
    if (verifyDependencyArtifact(project, package, "package.tgz").status != DependencyArtifactStatus::Mismatched) return 10;
    std::filesystem::resize_file(root / "package.tgz", 64U * 1024U * 1024U + 1);
    if (verifyDependencyArtifact(project, package, "package.tgz").status != DependencyArtifactStatus::Error) return 11;
    return 0;
}
