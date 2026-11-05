#pragma once

#include "editor/diagnostics/Diagnostics.hpp"
#include "editor/syntax/Syntax.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::editor {

enum class JsonType {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object,
};

struct JsonValue {
    JsonType type = JsonType::Null;
    std::size_t byteOffset = 0; // Start of this value in the original JSON text.
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    std::string key;
    std::vector<JsonValue> children;

    [[nodiscard]] const JsonValue* find(std::string_view member) const noexcept;
};

struct JsonError {
    std::size_t line = 0;
    std::size_t column = 1;
    std::string message;
};

[[nodiscard]] std::optional<JsonValue> parseJson(std::string_view text,
                                                 JsonError& error);
[[nodiscard]] std::vector<TokenSpan> highlightJsonLine(std::u32string_view line);
[[nodiscard]] std::vector<Diagnostic>
diagnoseJson(const std::vector<std::u32string>& lines);

} // namespace lucid::editor
