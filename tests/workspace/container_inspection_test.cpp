#include "modules/ContainerInspection.hpp"
#include <string>
using namespace lucid::modules;
int main() {
    ContainerListing listing; std::string error;
    if (!parseContainerListing("{\"ID\":\"abc\",\"Image\":\"alpine\",\"Names\":\"demo\",\"Status\":\"Up\",\"Ports\":\"80\"}\n{\"ID\":\"def\",\"State\":\"exited\"}\n", listing, error) || listing.containers.size() != 2 || listing.containers[0].names != "demo" || listing.warnings.empty()) return 1;
    if (!parseContainerListing(R"([{"Id":"123","Image":"img","Names":["one","two"],"State":"running","Status":"Up"}])", listing, error) || listing.containers.size() != 1 || listing.containers[0].names != "one, two") return 2;
    if (parseContainerListing("{\"ID\":\"ok\"}\ninvalid", listing, error) || !listing.containers.empty()) return 3;
    if (parseContainerListing(R"({"ID":"a","Id":"b"})", listing, error)) return 4;
    if (parseContainerListing(std::string(8U*1024U*1024U+1,'x'), listing, error)) return 5;
    auto log = parseContainerLog("hello\nworld", 5); if (!log.truncated || log.text != "hello") return 6;
    log = parseContainerLog("abc", 3); if (log.truncated || log.text != "abc") return 7;
    if (!parseContainerListing("[]", listing, error) || !listing.containers.empty()) return 8;
    ContainerDetails details;
    if (!parseContainerDetails(R"([{"Id":"abc123","Name":"/web","Config":{"Image":"web:latest","Env":["SECRET=hidden"]},"State":{"Status":"running","Health":{"Status":"healthy"}},"Mounts":[{"Type":"bind","Source":"/host","Destination":"/data"}],"NetworkSettings":{"Ports":{"80/tcp":[{"HostIp":"127.0.0.1","HostPort":"8080"}],"81/tcp":null}}}])",details,error) || details.id!="abc123" || details.image!="web:latest" || details.health!="healthy" || details.mounts.size()!=1 || details.ports.size()!=2 || details.incomplete) return 9;
    if (parseContainerDetails(R"([{"Id":"a","Id":"b"}])",details,error) || !details.id.empty()) return 10;
    if (!parseContainerDetails(R"([{"Id":"a"}])",details,error) || !details.incomplete) return 11;
    if (parseContainerDetails("[]",details,error) || parseContainerDetails(std::string(1024U*1024U+1,'x'),details,error)) return 12;
    return 0;
}
