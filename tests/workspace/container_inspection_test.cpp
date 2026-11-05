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
    return 0;
}
