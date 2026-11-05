#include "modules/ApiRequests.hpp"
#include <string>
#include <cstdlib>
using namespace lucid::modules;
int main() {
    std::vector<ApiRequest> requests; std::string error;
    const std::string input = R"({"requests":[{"name":"Test","method":"POST","url":"https://example.com/{{ID}}","headers":{"Authorization":"Bearer {{TOKEN}}"},"body":"{\"id\":\"{{ID}}\"}"}]})";
    bool ok = parseApiRequests(input, {{"ID", "123"}, {"TOKEN", "secret"}}, requests, error) && requests.size() == 1 && requests[0].url == "https://example.com/123" && requests[0].headers.at("Authorization") == "Bearer secret" && requests[0].body == "{\"id\":\"123\"}";
    ok = ok && !parseApiRequests(input, {{"ID", "123"}}, requests, error) && requests.empty() && error.find("TOKEN") != std::string::npos && error.find("secret") == std::string::npos;
    ok = ok && !parseApiRequests(input, {{"ID", "123"}, {"TOKEN", "x\r\nInjected: yes"}}, requests, error) && requests.empty();
    ok = ok && !parseApiRequests(R"({"requests":[{"name":"x","method":"GET","url":"https://x","headers":{"X":"a","x":"b"}}]})", {}, requests, error);
    ok = ok && !parseApiRequests(R"({"requests":[{"name":"x","method":"GET","url":"https://{{MISSING"}]})", {}, requests, error);
    ok = ok && !parseApiRequests(R"({"requests":[{"name":"x","method":"GET","url":"https:///x"}]})", {}, requests, error);
    ok = ok && !parseApiRequests(R"({"requests":[{"name":"x","method":"GET","url":"https://user:secret@example.com/"}]})", {}, requests, error);
    ok = ok && !parseApiRequests(R"({"requests":[],"requests":[]})", {}, requests, error);
    ok = ok && !parseApiRequests(input, {{"ID", "123"}, {"TOKEN", std::string(2 * 1024 * 1024 + 1, 'x')}}, requests, error);
    std::vector<ApiEnvironment> environments;
    const std::string profile = R"({"environments":[{"name":"local","variables":{"ID":{"type":"number","value":"123"},"TOKEN":{"type":"string","value":"secret","secret":true},"ON":{"type":"boolean","value":true}}},{"name":"staging","variables":{}}]})";
    ok = ok && parseApiEnvironments(profile, environments, error) && environments.size() == 2 && environments[0].variables.at("ID").value == "123" &&
        environments[0].variables.at("TOKEN").secret && environments[0].variables.at("TOKEN").value == "secret" && environments[0].variables.at("ON").value == "true";
    std::map<std::string, std::string> selected;
    if (ok) for (const auto& [key, value] : environments[0].variables) selected.emplace(key, value.value);
    ok = ok && parseApiRequests(input, selected, requests, error) && requests[0].headers.at("Authorization") == "Bearer secret";
    ApiEnvironment secretEnvironment{"safe", {{"TOKEN", {"s3cr3t", true}}, {"HOST", {"example.com", false}}, {"PREFIX", {"https://", false}}}};
    ok = ok && parseApiRequestsFromEnvironment(R"({"requests":[{"name":"safe","method":"GET","url":"{{PREFIX}}{{HOST}}/x?token={{TOKEN}}"}]})", secretEnvironment, requests, error) &&
        requests.size() == 1 && requests[0].url == "https://example.com/x?token=s3cr3t";
    for (const auto* unsafe : {
        R"({"requests":[{"name":"unsafe","method":"GET","url":"https://{{TOKEN}}.example/x"}]})",
        R"({"requests":[{"name":"unsafe","method":"GET","url":"{{PREFIX}}{{TOKEN}}.example/x"}]})",
        R"({"requests":[{"name":"safe","method":"GET","url":"https://example/x"},{"name":"unsafe","method":"GET","url":"https://{{TOKEN}}.example/x"}]})"}) {
        if (parseApiRequestsFromEnvironment(unsafe, secretEnvironment, requests, error) || !requests.empty() || error.find("s3cr3t") != std::string::npos) ok = false;
    }
    secretEnvironment.variables["TOKEN"].value.clear();
    ok = ok && !parseApiRequestsFromEnvironment(R"({"requests":[{"name":"unsafe","method":"GET","url":"https://example{{TOKEN}}/x"}]})", secretEnvironment, requests, error) && requests.empty();
    for (const auto* invalid : {
        R"({"environments":[{"name":"dev","variables":{"X":{"type":"number","value":"01"}}}]})",
        R"({"environments":[{"name":"dev","variables":{"X":{"type":"number","value":"NaN"}}}]})",
        R"({"environments":[{"name":"dev","variables":{"X":{"type":"boolean","value":"true"}}}]})",
        R"({"environments":[{"name":"dev","variables":{"X":{"type":"string","value":"secret","value":"other"}}}]})",
        R"({"environments":[{"name":"dev","variables":{}},{"name":"dev","variables":{}}]})",
        R"({"environments":[{"name":"dev","variables":{"X":{"type":"string","value":"secret","other":true}}}]})"}) {
        if (parseApiEnvironments(invalid, environments, error) || !environments.empty() || error.empty() || error.find("secret") != std::string::npos) ok = false;
    }
    ok = ok && !parseApiEnvironments(std::string(1024U * 1024U + 1, 'x'), environments, error) && environments.empty();
    std::atomic_bool cancelled{true};
    ok = ok && sendApiRequest({"", "GET", "http://127.0.0.1/", "", {}}, &cancelled).error == "Request cancelled";
    ok = ok && !sendApiRequest({"", "GET", "file:///etc/passwd", "", {}}).error.empty();
    ok = ok && !sendApiRequest({"", "GET", "http://127.0.0.1/", "", {{"X", "a\r\nb"}}}).error.empty();
    ok = ok && !sendApiRequest({"", "GET", "http://@127.0.0.1/", "", {}}).error.empty();
    ok = ok && !sendApiRequest({"", "HEAD", "http://127.0.0.1/", "body", {}}).error.empty();
    ok = ok && !sendApiRequest({"", "POST", "http://127.0.0.1/", "x", {{"Content-Length", "3"}}}).error.empty();
    ok = ok && !sendApiRequest({"", "GET", "http://127.0.0.1/", "", {{"X-Test", "a"}, {"x-test", "b"}}}).error.empty();
    if (const char* port = std::getenv("LUCID_TEST_HTTP_PORT")) {
        const std::string base = std::string("http://127.0.0.1:") + port;
        const auto response = sendApiRequest({"", "POST", base + "/post", "hello", {{"X-Test", "yes"}}});
        ok = ok && response.error.empty() && response.status == 201 && response.body == "hello:yes";
        const auto empty = sendApiRequest({"", "POST", base + "/empty", "", {{"X-Empty", ""}}});
        ok = ok && empty.error.empty() && empty.status == 201 && empty.body == "0:present";
        const auto redirect = sendApiRequest({"", "GET", base + "/redirect", "", {}});
        ok = ok && redirect.error.empty() && redirect.status == 302;
        const auto truncated = sendApiRequest({"", "GET", base + "/truncated?token=do-not-display", "", {}});
        ok = ok && truncated.status == 200 && truncated.error == "The HTTP response was interrupted or incomplete" && truncated.error.find("do-not-display") == std::string::npos;
        const auto slow = sendApiRequest({"", "GET", base + "/slow", "", {}});
        ok = ok && slow.status == 200 && slow.error.empty() && slow.body == "ok" && slow.elapsedMilliseconds >= 60;
        const auto large = sendApiRequest({"", "GET", base + "/large", "", {}});
        ok = ok && large.error == "Response exceeds size limit" && large.body.size() <= 4U * 1024U * 1024U;
    }
    return ok ? 0 : 1;
}
