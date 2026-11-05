#pragma once
#include "modules/ApiRequests.hpp"
#include <atomic>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
enum class ApiAssertionKind { Status, BodyContains, JsonStringEquals };
struct ApiAssertion { ApiAssertionKind kind; long status=0; std::string pointer, expected; };
struct ApiAssertionResult { ApiAssertionKind kind; bool passed=false; std::string detail; };
struct ApiAssertionReport { std::vector<ApiAssertionResult> results; std::string error; };
// Accepts {"assertions":[{"type":"status","value":200},
// {"type":"body_contains","value":"ok"}, {"type":"json_string_equals",
// "pointer":"/data/id","value":"123"}]}. All fields are required and
// unknown or duplicate fields are rejected. Values can contain secrets.
[[nodiscard]] bool parseApiAssertions(std::string_view json, std::vector<ApiAssertion>& assertions, std::string& error);
// Local-only; never sends or stores data. Fixed result messages do not reveal
// expected values, response contents, JSON paths or transport error text. Failed
// or incomplete transport responses are rejected instead of tested.
[[nodiscard]] ApiAssertionReport evaluateApiAssertions(const ApiResponse& response,
    const std::vector<ApiAssertion>& assertions, const std::atomic_bool* cancel=nullptr);
}
