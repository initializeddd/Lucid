#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
enum class MarkdownBlockKind { Paragraph, Heading, Code, Quote, ListItem, Rule };
struct MarkdownBlock { MarkdownBlockKind kind = MarkdownBlockKind::Paragraph; int level = 0; std::string text; };
struct MarkdownPreview { std::vector<MarkdownBlock> blocks; bool truncated = false; };
// Parses Markdown into non-executable display blocks. Raw HTML is omitted.
[[nodiscard]] bool parseMarkdownPreview(std::string_view source, MarkdownPreview& result, std::string& error);
}
