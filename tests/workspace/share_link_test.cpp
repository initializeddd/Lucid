#include "workspace/collaboration/ShareLink.hpp"
using namespace lucid::workspace::collaboration;
int main() {
    std::string a, b, error;
    if (!newShareToken(a, error) || !newShareToken(b, error) || a.size() != 64 || a == b || !sameToken(a, a) || sameToken(a, b)) return 1;
    ShareLink original{"host.tailnet.test", 12345, a}, parsed;
    if (!parseShareLink(formatShareLink(original), parsed, error) || parsed.host != original.host || parsed.port != original.port || parsed.token != a) return 2;
    if (parseShareLink("lucid://localhost:0/" + a, parsed, error) || parseShareLink("lucid://host:1/" + a + "/extra", parsed, error) || parseShareLink("https://host:1/" + a, parsed, error)) return 3;
    if (parseShareLink("lucid://host:65536/" + a, parsed, error) || parseShareLink("lucid://user@host:1/" + a, parsed, error) || sameToken(a, a.substr(1))) return 4;
    return 0;
}
