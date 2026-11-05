#pragma once
#include "modules/ApiRequests.hpp"
#include <atomic>
#include <cstddef>
#include <string>
#include <string_view>
namespace lucid::modules {
// A byte-exact snapshot comparison. It intentionally contains neither body.
// Line locations are approximate changed ranges for lossless plain UTF-8 only.
struct ApiResponseComparison {
    bool identical=false, text=false;
    std::size_t baselineBytes=0, currentBytes=0, commonPrefix=0, commonSuffix=0;
    std::size_t baselineFirstLine=0, baselineLastLine=0, currentFirstLine=0, currentLastLine=0;
    std::string error;
};
[[nodiscard]] ApiResponseComparison compareApiResponseBody(std::string_view baseline, const ApiResponse& response,
                                                          const std::atomic_bool* cancel=nullptr);
}
