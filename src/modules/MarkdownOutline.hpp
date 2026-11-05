#pragma once
#include "editor/document/Document.hpp"
#include <cstddef>
#include <string>
#include <vector>
namespace lucid::modules {
struct MarkdownHeading { std::string text; std::size_t line = 0; int level = 0; };
struct MarkdownOutline { std::vector<MarkdownHeading> headings; bool truncated = false; };
// Extracts ATX and root-level Setext headings outside fenced blocks. Setext block
// recognition uses MD4C; line numbers are zero-based. No HTML is executed.
[[nodiscard]] MarkdownOutline markdownOutline(const editor::Document& document, std::size_t limit = 1000);
}
