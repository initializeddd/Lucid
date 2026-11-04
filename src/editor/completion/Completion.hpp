#pragma once
#include <set>
#include <string>
#include <string_view>

namespace lucid::editor {
[[nodiscard]] bool isCompletionIdentifier(char32_t codepoint) noexcept;
[[nodiscard]] bool isCompletionIdentifierUtf8(std::string_view text) noexcept;
// Collects function-like identifiers for best-effort, language-independent completion.
void collectFunctionNames(std::u32string_view line, std::set<std::string>& names);
}
