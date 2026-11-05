#pragma once
#include "modules/DesignTokens.hpp"
#include <atomic>
#include <cstddef>
#include <string>
#include <vector>
namespace lucid::modules {
enum class DesignAliasStatus { Literal, Resolved, Missing, Cycle, Ambiguous, Unsupported, DepthLimit };
struct DesignAliasResult {
    DesignAliasStatus status=DesignAliasStatus::Literal;
    // Indexes refer only to the exact token vector supplied to the resolver.
    // Missing/unsupported references use noIndex. Values are not copied.
    static constexpr std::size_t noIndex=static_cast<std::size_t>(-1);
    std::size_t targetIndex=noIndex, resolvedIndex=noIndex, hops=0;
};
// Resolves exact whole-value `{token.path}` references within one extracted JSON
// token report, preserving original token indexes. Embedded expressions, JSON
// pointers, CSS var(), composite values, cross-file references and type
// compatibility are unsupported. At most 10,000 tokens, 4 MiB of names/values,
// and 64 alias hops are accepted. Duplicate names are ambiguous. No I/O/network.
// Returns false on invalid/bounded input or cancellation and clears results.
[[nodiscard]] bool resolveDesignTokenAliases(const std::vector<DesignToken>& tokens,
    std::vector<DesignAliasResult>& results, std::string& error, const std::atomic_bool* cancel=nullptr);
}
