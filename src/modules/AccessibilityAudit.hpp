#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
struct AccessibilityFinding { std::string rule, message; std::size_t line = 0, column = 0; };
struct AccessibilityAudit { std::vector<AccessibilityFinding> findings; bool incomplete = false; std::string limitation; };
// Static heuristic: no DOM, script execution, CSS, accessibility tree or network.
[[nodiscard]] AccessibilityAudit auditHtml(std::string_view html, std::size_t maxBytes = 1024*1024, std::size_t maxTags = 10000, std::size_t maxFindings = 1000);
}
