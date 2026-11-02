#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lucid::editor {

struct Diagnostic {
    std::uint32_t line = 0;
    std::uint32_t column = 0;
    std::uint32_t length = 1;
    std::string message;
    std::string code;

    Diagnostic() = default;
    Diagnostic(std::uint32_t lineValue, std::uint32_t columnValue,
               std::uint32_t lengthValue, std::string messageValue,
               std::string codeValue = {})
        : line(lineValue), column(columnValue), length(lengthValue),
          message(std::move(messageValue)), code(std::move(codeValue)) {}

    [[nodiscard]] std::string key() const;
};

[[nodiscard]] std::vector<Diagnostic>
diagnoseCpp(const std::vector<std::u32string>& lines);

} // namespace lucid::editor
