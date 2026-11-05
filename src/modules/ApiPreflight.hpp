#pragma once
#include "modules/ApiRequests.hpp"
#include <atomic>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
enum ApiReferenceLocation : unsigned { ApiRefUrl=1, ApiRefHeader=2, ApiRefBody=4 };
struct ApiVariableReference { std::string name; unsigned locations=0; bool missing=false,secret=false; };
struct ApiRequestPreflight { std::string name; std::vector<ApiVariableReference> references; bool invalidPlaceholder=false, secretInUrl=false; };
struct ApiPreflightReport { std::vector<ApiRequestPreflight> requests; std::vector<std::string> unusedEnvironmentVariables; bool unusedReliable=true; };
// Read-only metadata analysis of one collection and one selected environment.
// Collection defaults are overridden by selected environment variables. Does
// not interpolate or return values, inspect destinations, issue requests or
// guarantee that a request can be sent; the normal send checks remain required.
// Missing variables, malformed markers and secret-marked URL usage are shown
// separately. If markers are malformed, unusedReliable is false and the unused
// list must not be presented as complete. Variable names and request names may still be sensitive.
[[nodiscard]] bool preflightApiEnvironment(std::string_view collection,const ApiEnvironment& environment,
    ApiPreflightReport& report,std::string& error,const std::atomic_bool* cancel=nullptr);
}
