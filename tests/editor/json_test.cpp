#include "editor/json/Json.hpp"
#include "editor/syntax/Syntax.hpp"

#include <string>
#include <vector>

namespace {

bool hasRole(const std::vector<lucid::editor::TokenSpan>& tokens,
             lucid::editor::TokenRole role) {
    for (const auto& token : tokens) {
        if (token.role == role) {
            return true;
        }
    }
    return false;
}

} // namespace

int main() {
    using namespace lucid::editor;

    JsonError error;
    const auto root = parseJson(
        R"({"animationSpeed": 1.5, "nested": {"enabled": true}, "list": [1, 2]})",
        error);
    if (!root.has_value() || root->type != JsonType::Object) {
        return 1;
    }
    const JsonValue* speed = root->find("animationSpeed");
    const JsonValue* nested = root->find("nested");
    if (speed == nullptr || speed->type != JsonType::Number ||
        speed->numberValue != 1.5 || nested == nullptr ||
        nested->type != JsonType::Object) {
        return 2;
    }
    const JsonValue* enabled = nested->find("enabled");
    if (enabled == nullptr || enabled->type != JsonType::Bool ||
        !enabled->boolValue) {
        return 3;
    }

    const auto invalid = parseJson("{\n  \"a\": ,\n}", error);
    if (invalid.has_value() || error.line != 1 || error.message.empty()) {
        return 4;
    }

    const auto tokens = highlightJsonLine(U"\"name\": \"value\",");
    if (!hasRole(tokens, TokenRole::Key) || !hasRole(tokens, TokenRole::String) ||
        !hasRole(tokens, TokenRole::Punctuation)) {
        return 5;
    }

    const std::vector<std::u32string> lines = {U"{", U"  \"a\": \"unterminated"};
    const auto diagnostics = diagnoseJson(lines);
    if (diagnostics.empty() || diagnostics.front().code != "json" ||
        diagnostics.front().message.empty()) {
        return 6;
    }

    return 0;
}
