#include "modules/MarkdownOutline.hpp"
using namespace lucid::modules;
int main() {
    auto doc = lucid::editor::Document::fromUtf8("# First\n\nTitle\n=====\n\nTwo\nlines\n---\n\n```\nFake\n===\n# Hidden\n```\n\n---\n\n<div>\nHTML\n===\n</div>\n");
    auto outline = markdownOutline(doc);
    bool ok = outline.headings.size() == 3 && outline.headings[0].text == "First" && outline.headings[0].line == 0 && outline.headings[1].text == "Title" && outline.headings[1].line == 2 && outline.headings[1].level == 1 && outline.headings[2].text == "Two lines" && outline.headings[2].line == 5 && outline.headings[2].level == 2;
    auto limited = markdownOutline(doc, 2); ok = ok && limited.truncated && limited.headings.size() == 2;
    auto hr = lucid::editor::Document::fromUtf8("# Heading\n---\n");
    auto hrOutline = markdownOutline(hr); ok = ok && hrOutline.headings.size() == 1 && hrOutline.headings[0].line == 0;
    auto nested = lucid::editor::Document::fromUtf8("> # Quoted\n>\n> Nested title\n> ---\n\n- item\n  - ## List heading\n\n  ```\n  # Not a heading\n  ```\n\n# Outside\n");
    auto nestedOutline = markdownOutline(nested);
    ok = ok && nestedOutline.headings.size() == 4 && nestedOutline.headings[0].text == "Quoted" && nestedOutline.headings[0].line == 0 && nestedOutline.headings[0].level == 1 && nestedOutline.headings[1].text == "Nested title" && nestedOutline.headings[1].line == 2 && nestedOutline.headings[1].level == 2 && nestedOutline.headings[2].text == "List heading" && nestedOutline.headings[2].line == 6 && nestedOutline.headings[2].level == 2 && nestedOutline.headings[3].text == "Outside" && nestedOutline.headings[3].line == 12;
    auto nestedLimited = markdownOutline(nested, 2);
    ok = ok && nestedLimited.truncated && nestedLimited.headings.size() == 2 && nestedLimited.headings[0].line == 0 && nestedLimited.headings[1].line == 2;
    std::string mixed;
    for (int i = 0; i < 200; ++i) mixed += "Setext " + std::to_string(i) + "\n===\n# ATX\n";
    auto many = markdownOutline(lucid::editor::Document::fromUtf8(mixed), 3);
    ok = ok && many.truncated && many.headings.size() == 3 && many.headings[0].line == 0 && many.headings[1].line == 2 && many.headings[2].line == 3;
    std::string unicode; for (int i = 0; i < 1500; ++i) unicode += "\xf0\x9f\x98\x80";
    auto longHeading = markdownOutline(lucid::editor::Document::fromUtf8(unicode + "\n===\n"));
    ok = ok && longHeading.truncated && longHeading.headings.size() == 1 && longHeading.headings[0].text.size() == 4096 && lucid::editor::Document::fromUtf8(longHeading.headings[0].text).toUtf8() == longHeading.headings[0].text;
    return ok ? 0 : 1;
}
