#include "modules/Registry.hpp"
#include <string>
#include <vector>

int main() {
    using namespace lucid::modules;
    std::vector<RegistryEntry> entries; std::string error, contents;
    if (parseCatalog(R"({"modules":[],"modules":[]})", entries, error) || !entries.empty()) return 1;
    if (parseCatalog(R"({"modules":[{"id":"a","id":"b","package":"owner/a","version":"1"}]})", entries, error) || !entries.empty()) return 2;
    if (parseCatalog(std::string(1024U * 1024U + 1, ' '), entries, error)) return 3;
    constexpr auto media = "application/vnd.lucid.module.v1+json";
    const std::string manifest = R"({"layers":[{"mediaType":"application/vnd.lucid.module.v1+json","digest":"sha256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"}]})";
    auto transport = [&](std::string_view url, const std::vector<std::string>&, std::string& output, std::string&) {
        if (url.find("/token?") != std::string_view::npos) output = R"({"token":"abc"})";
        else if (url.find("/manifests/") != std::string_view::npos) output = manifest;
        else { output = "unverified"; return false; }
        return true;
    };
    contents = "old";
    if (fetchPackage("owner/pkg", "1", media, contents, error, transport) || !contents.empty()) return 4;
    auto duplicateToken = [&](std::string_view, const std::vector<std::string>&, std::string& output, std::string&) { output = R"({"token":"a","token":"b"})"; return true; };
    if (fetchPackage("owner/pkg", "1", media, contents, error, duplicateToken)) return 5;
    auto invalidToken = [&](std::string_view, const std::vector<std::string>&, std::string& output, std::string&) { output = R"({"token":"a\u0000b"})"; return true; };
    if (fetchPackage("owner/pkg", "1", media, contents, error, invalidToken)) return 6;
    auto duplicateManifest = [&](std::string_view url, const std::vector<std::string>&, std::string& output, std::string&) {
        output = url.find("/token?") != std::string_view::npos ? R"({"token":"abc"})" : R"({"layers":[],"layers":[]})"; return true;
    };
    if (fetchPackage("owner/pkg", "1", media, contents, error, duplicateManifest)) return 7;
    auto success = [&](std::string_view url, const std::vector<std::string>&, std::string& output, std::string&) {
        output = url.find("/token?") != std::string_view::npos ? R"({"token":"abc"})" : url.find("/manifests/") != std::string_view::npos ? manifest : "abc"; return true;
    };
    if (!fetchPackage("owner/pkg", "1", media, contents, error, success) || contents != "abc") return 8;
    if (!parseCatalog(R"({"modules":[{"id":"a","package":"owner/pkg","version":"_RC1."}]})", entries, error) || entries[0].version != "_RC1.") return 9;
    bool exactTagSeen=false;
    auto tagged=[&](std::string_view url, const std::vector<std::string>& headers, std::string& output, std::string& err) {
        if (url.find("/manifests/_RC1.") != std::string_view::npos) exactTagSeen=true;
        return success(url,headers,output,err);
    };
    if (!fetchPackage("owner/pkg", "_RC1.", media, contents, error, tagged) || !exactTagSeen || contents != "abc") return 10;
    for (const std::string& tag : std::vector<std::string>{"", ".bad", "-bad", "a/b", "a?x", "a#x", "a:b", std::string(129, 'a')}) {
        if (fetchPackage("owner/pkg", tag, media, contents, error, success) || !contents.empty()) return 11;
    }
    if (!fetchPackage("owner/pkg", std::string(128, 'A'), media, contents, error, success)) return 12;
    std::string id, other;
    if (!packageModuleId("owner/simple-name", id, error) || id != "simple-name") return 13;
    if (!packageModuleId("owner/my.module", id, error) || id.size() != 22 || id.rfind("my-module-",0)!=0) return 14;
    if (!packageModuleId("other/my.module", other, error) || id == other) return 15;
    if (!packageModuleId("owner/" + std::string(100,'a'), id, error) || id.size()!=80) return 16;
    if (packageModuleId("owner/../escape", id, error) || !id.empty()) return 17;
    const std::string module = R"({"name":"Example","version":"1.0","description":"Example module","requires":["snippet-v1","snippet-placeholders-v2"],"permissions":{"project":false,"network":false,"processes":false,"settings":false},"commands":[{"title":"Insert","insert":"x"}]})";
    if (!parseCatalog(R"({"modules":[{"id":"example","package":"owner/example","version":"latest","name":"Example","description":"Example module","requires":["snippet-placeholders-v2","snippet-v1"]}]})",entries,error) || !catalogManifestMatches(entries[0],module,error)) return 18;
    entries[0].name="Different"; if (catalogManifestMatches(entries[0],module,error) || error.empty()) return 19;
    entries[0].name="Example"; entries[0].requirements.clear(); if (catalogManifestMatches(entries[0],module,error)) return 20;
    if (!parseCatalog(R"({"modules":[{"id":"example","package":"owner/example","version":"latest"}]})",entries,error) || !catalogManifestMatches(entries[0],module,error)) return 21;
    if (catalogManifestMatches(entries[0],"{}",error)) return 22;
    return 0;
}
