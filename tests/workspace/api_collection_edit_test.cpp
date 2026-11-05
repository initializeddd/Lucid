#include "modules/ApiRequests.hpp"
#include "editor/json/Json.hpp"
#include <string>
using namespace lucid::modules;
namespace {
std::string applyEdit(std::string text, const ApiCollectionEdit& edit) {
    text.replace(edit.offset, edit.length, edit.replacement); return text;
}
bool valid(std::string_view text, std::size_t count) {
    lucid::editor::JsonError error; const auto json = lucid::editor::parseJson(text, error);
    const auto* requests = json ? json->find("requests") : nullptr;
    return requests && requests->type == lucid::editor::JsonType::Array && requests->children.size() == count;
}
}
int main() {
    const std::string original = R"({"extra":{"keep":true},"requests": [
 {"name":"one","url":"https://x/?token={{TOKEN}}","nested":{"literal":"}\\\"x"}},
 {"name":"two","body":"{json}"},
 {"name":"three","headers":{"X":"{{SECRET}}"}}
],"variables":{"SECRET":"private"}})";
    ApiCollectionEdit edit; std::string error;
    if (!valid(original, 3)) return 1;
    std::vector<std::string> names;
    if (!listApiRequestTemplates(original, names, error) || names != std::vector<std::string>{"one", "two", "three"}) return 14;
    if (listApiRequestTemplates(R"({"requests":[{}]})", names, error) || !names.empty()) return 15;
    if (!renameApiRequest(original, 1, "new \"name\"", edit, error)) return 2;
    auto renamed = applyEdit(original, edit);
    if (!valid(renamed, 3) || renamed.find("\"name\":\"new \\\"name\\\"\"") == std::string::npos ||
        renamed.find("{{TOKEN}}") == std::string::npos || renamed.find("\"SECRET\":\"private\"") == std::string::npos || edit.replacement.find("private") != std::string::npos) return 3;
    for (std::size_t index = 0; index < 3; ++index) {
        if (!removeApiRequest(original, index, edit, error)) return 4;
        const auto updated = applyEdit(original, edit);
        if (!valid(updated, 2) || updated.find("\"extra\":{\"keep\":true}") == std::string::npos || updated.find("\"SECRET\":\"private\"") == std::string::npos || !edit.replacement.empty()) return 5;
    }
    const std::string single = R"({"requests":[ {"name":"only"} ]})";
    if (!removeApiRequest(single, 0, edit, error) || !valid(applyEdit(single, edit), 0)) return 6;
    if (!appendApiRequestTemplate(original, "new", edit, error)) return 11;
    auto appended = applyEdit(original, edit);
    if (!valid(appended, 4) || appended.find("\"SECRET\":\"private\"") == std::string::npos || edit.replacement.find("private") != std::string::npos || edit.replacement.find("{{TOKEN}}") != std::string::npos) return 12;
    if (!appendApiRequestTemplate(R"({"requests":[]})", "first", edit, error) || !valid(applyEdit(R"({"requests":[]})", edit), 1)) return 13;
    if (renameApiRequest(original, 3, "x", edit, error) || !edit.replacement.empty()) return 7;
    if (renameApiRequest(original, 0, std::string("\xff", 1), edit, error) || error.empty()) return 8;
    if (renameApiRequest(R"({"requests":[{"name":"x","name":"y"}]})", 0, "z", edit, error)) return 9;
    if (removeApiRequest(std::string(1024U * 1024U + 1, 'x'), 0, edit, error)) return 10;
    return 0;
}
