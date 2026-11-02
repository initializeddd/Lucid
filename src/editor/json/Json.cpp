#include "editor/json/Json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lucid::editor {
namespace {

constexpr std::size_t kMaxDepth = 128;

[[nodiscard]] bool isDigit(char value) noexcept {
    return value >= '0' && value <= '9';
}

[[nodiscard]] bool isHexDigit(char value) noexcept {
    return isDigit(value) || (value >= 'a' && value <= 'f') ||
           (value >= 'A' && value <= 'F');
}

[[nodiscard]] unsigned hexValue(char value) noexcept {
    if (value >= '0' && value <= '9') {
        return static_cast<unsigned>(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return static_cast<unsigned>(value - 'a') + 10U;
    }
    return static_cast<unsigned>(value - 'A') + 10U;
}

void appendUtf8(std::string& output, std::uint32_t codepoint) {
    if (codepoint <= 0x7FU) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FFU) {
        output.push_back(static_cast<char>(0xC0U | (codepoint >> 6U)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    } else if (codepoint <= 0xFFFFU) {
        output.push_back(static_cast<char>(0xE0U | (codepoint >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    } else {
        output.push_back(static_cast<char>(0xF0U | (codepoint >> 18U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
}

class Parser {
public:
    Parser(std::string_view text, JsonError& error) : m_text(text), m_error(error) {}

    [[nodiscard]] std::optional<JsonValue> parse() {
        skipWhitespace();
        if (atEnd()) {
            fail("Expected a JSON value");
            return std::nullopt;
        }

        JsonValue value;
        if (!parseValue(value, 0)) {
            return std::nullopt;
        }

        skipWhitespace();
        if (!atEnd()) {
            fail("Unexpected trailing characters");
            return std::nullopt;
        }
        return value;
    }

private:
    [[nodiscard]] bool atEnd() const noexcept { return m_index >= m_text.size(); }
    [[nodiscard]] char peek() const noexcept { return atEnd() ? '\0' : m_text[m_index]; }

    char advance() {
        const char value = peek();
        if (atEnd()) {
            return value;
        }
        ++m_index;
        if (value == '\n') {
            ++m_line;
            m_column = 1;
        } else {
            ++m_column;
        }
        return value;
    }

    void skipWhitespace() {
        while (!atEnd()) {
            const char value = peek();
            if (value != ' ' && value != '\t' && value != '\r' && value != '\n') {
                break;
            }
            advance();
        }
    }

    bool fail(std::string message) {
        if (m_error.message.empty()) {
            m_error.line = m_line;
            m_error.column = m_column;
            m_error.message = std::move(message);
        }
        return false;
    }

    bool parseValue(JsonValue& value, std::size_t depth) {
        if (depth > kMaxDepth) {
            return fail("JSON is nested too deeply");
        }

        skipWhitespace();
        const char value_char = peek();
        if (value_char == '{') {
            return parseObject(value, depth + 1);
        }
        if (value_char == '[') {
            return parseArray(value, depth + 1);
        }
        if (value_char == '"') {
            value.type = JsonType::String;
            return parseString(value.stringValue);
        }
        if (value_char == '-' || isDigit(value_char)) {
            return parseNumber(value);
        }
        if (value_char == 't' || value_char == 'f' || value_char == 'n') {
            return parseLiteral(value);
        }
        if (atEnd()) {
            return fail("Expected a value");
        }
        return fail("Unexpected character in JSON value");
    }

    bool parseObject(JsonValue& value, std::size_t depth) {
        value.type = JsonType::Object;
        advance();
        skipWhitespace();
        if (peek() == '}') {
            advance();
            return true;
        }

        while (true) {
            if (peek() != '"') {
                return fail("Expected a string key");
            }

            JsonValue member;
            if (!parseString(member.key)) {
                return false;
            }

            skipWhitespace();
            if (peek() != ':') {
                return fail("Expected ':' after object key");
            }
            advance();
            skipWhitespace();
            if (!parseValue(member, depth)) {
                return false;
            }
            value.children.push_back(std::move(member));

            skipWhitespace();
            const char separator = peek();
            if (separator == '}') {
                advance();
                return true;
            }
            if (separator != ',') {
                return fail("Expected ',' or '}' in object");
            }
            advance();
            skipWhitespace();
            if (peek() == '}') {
                return fail("Trailing comma in object");
            }
        }
    }

    bool parseArray(JsonValue& value, std::size_t depth) {
        value.type = JsonType::Array;
        advance();
        skipWhitespace();
        if (peek() == ']') {
            advance();
            return true;
        }

        while (true) {
            JsonValue element;
            if (!parseValue(element, depth)) {
                return false;
            }
            value.children.push_back(std::move(element));

            skipWhitespace();
            const char separator = peek();
            if (separator == ']') {
                advance();
                return true;
            }
            if (separator != ',') {
                return fail("Expected ',' or ']' in array");
            }
            advance();
            skipWhitespace();
            if (peek() == ']') {
                return fail("Trailing comma in array");
            }
        }
    }

    bool parseString(std::string& output) {
        if (peek() != '"') {
            return fail("Expected a string");
        }
        advance();
        output.clear();

        while (!atEnd()) {
            const char value = advance();
            if (value == '"') {
                return true;
            }
            if (static_cast<unsigned char>(value) < 0x20U) {
                return fail("Control character in string");
            }
            if (value != '\\') {
                output.push_back(value);
                continue;
            }

            if (atEnd()) {
                return fail("Unterminated string");
            }
            const char escaped = advance();
            switch (escaped) {
            case '"':
            case '\\':
            case '/':
                output.push_back(escaped);
                break;
            case 'b':
                output.push_back('\b');
                break;
            case 'f':
                output.push_back('\f');
                break;
            case 'n':
                output.push_back('\n');
                break;
            case 'r':
                output.push_back('\r');
                break;
            case 't':
                output.push_back('\t');
                break;
            case 'u': {
                std::uint32_t codepoint = 0;
                if (!parseHex4(codepoint)) {
                    return false;
                }
                if (codepoint >= 0xD800U && codepoint <= 0xDBFFU) {
                    if (peek() != '\\') {
                        return fail("Invalid UTF-16 surrogate pair");
                    }
                    advance();
                    if (peek() != 'u') {
                        return fail("Invalid UTF-16 surrogate pair");
                    }
                    advance();
                    std::uint32_t low = 0;
                    if (!parseHex4(low)) {
                        return false;
                    }
                    if (low < 0xDC00U || low > 0xDFFFU) {
                        return fail("Invalid UTF-16 surrogate pair");
                    }
                    codepoint = 0x10000U +
                                ((codepoint - 0xD800U) << 10U) +
                                (low - 0xDC00U);
                } else if (codepoint >= 0xDC00U && codepoint <= 0xDFFFU) {
                    return fail("Invalid UTF-16 surrogate pair");
                }
                appendUtf8(output, codepoint);
                break;
            }
            default:
                return fail("Invalid escape sequence");
            }
        }

        return fail("Unterminated string");
    }

    bool parseHex4(std::uint32_t& value) {
        if (m_index + 4 > m_text.size()) {
            return fail("Incomplete unicode escape");
        }
        value = 0;
        for (int index = 0; index < 4; ++index) {
            const char character = peek();
            if (!isHexDigit(character)) {
                return fail("Invalid unicode escape");
            }
            value = (value << 4U) | hexValue(character);
            advance();
        }
        return true;
    }

    bool parseNumber(JsonValue& value) {
        const std::size_t start = m_index;
        if (peek() == '-') {
            advance();
        }
        if (atEnd() || !isDigit(peek())) {
            return fail("Expected a digit");
        }
        if (peek() == '0') {
            advance();
            if (isDigit(peek())) {
                return fail("Leading zero in number");
            }
        } else {
            while (isDigit(peek())) {
                advance();
            }
        }
        if (peek() == '.') {
            advance();
            if (!isDigit(peek())) {
                return fail("Expected a digit after decimal point");
            }
            while (isDigit(peek())) {
                advance();
            }
        }
        if (peek() == 'e' || peek() == 'E') {
            advance();
            if (peek() == '+' || peek() == '-') {
                advance();
            }
            if (!isDigit(peek())) {
                return fail("Expected a digit in exponent");
            }
            while (isDigit(peek())) {
                advance();
            }
        }

        const std::string number(m_text.substr(start, m_index - start));
        value.type = JsonType::Number;
        value.numberValue = std::strtod(number.c_str(), nullptr);
        return true;
    }

    bool parseLiteral(JsonValue& value) {
        const auto consume = [this](std::string_view literal) {
            if (m_text.substr(m_index, literal.size()) != literal) {
                return false;
            }
            for (std::size_t index = 0; index < literal.size(); ++index) {
                advance();
            }
            return true;
        };

        if (peek() == 't') {
            if (!consume("true")) {
                return fail("Invalid literal");
            }
            value.type = JsonType::Bool;
            value.boolValue = true;
            return true;
        }
        if (peek() == 'f') {
            if (!consume("false")) {
                return fail("Invalid literal");
            }
            value.type = JsonType::Bool;
            value.boolValue = false;
            return true;
        }
        if (!consume("null")) {
            return fail("Invalid literal");
        }
        value.type = JsonType::Null;
        return true;
    }

    std::string_view m_text;
    std::size_t m_index = 0;
    std::size_t m_line = 0;
    std::size_t m_column = 1;
    JsonError& m_error;
};

[[nodiscard]] bool isJsonSpace(char32_t value) noexcept {
    return value == U' ' || value == U'\t' || value == U'\r' || value == U'\n';
}

[[nodiscard]] bool isJsonDigit(char32_t value) noexcept {
    return value >= U'0' && value <= U'9';
}

[[nodiscard]] std::string encodeLine(const std::u32string& line) {
    std::string result;
    result.reserve(line.size());
    for (const char32_t character : line) {
        appendUtf8(result, character);
    }
    return result;
}

} // namespace

const JsonValue* JsonValue::find(std::string_view member) const noexcept {
    for (const JsonValue& child : children) {
        if (child.key == member) {
            return &child;
        }
    }
    return nullptr;
}

std::optional<JsonValue> parseJson(std::string_view text, JsonError& error) {
    error = {};
    Parser parser(text, error);
    return parser.parse();
}

std::vector<TokenSpan> highlightJsonLine(std::u32string_view line) {
    std::vector<TokenSpan> spans;
    spans.reserve(line.size() / 3 + 1);

    const auto add = [&spans](std::size_t start, std::size_t length,
                              TokenRole role) {
        if (length == 0) {
            return;
        }
        spans.push_back(TokenSpan{static_cast<std::uint32_t>(start),
                                  static_cast<std::uint32_t>(length), role});
    };

    std::size_t index = 0;
    while (index < line.size()) {
        const char32_t character = line[index];
        if (isJsonSpace(character)) {
            ++index;
            continue;
        }

        if (character == U'"') {
            std::size_t end = index + 1;
            while (end < line.size()) {
                if (line[end] == U'\\' && end + 1 < line.size()) {
                    end += 2;
                    continue;
                }
                if (line[end] == U'"') {
                    ++end;
                    break;
                }
                ++end;
            }

            std::size_t next = end;
            while (next < line.size() && isJsonSpace(line[next])) {
                ++next;
            }
            add(index, end - index,
                next < line.size() && line[next] == U':' ? TokenRole::Key
                                                         : TokenRole::String);
            index = end;
            continue;
        }

        if (isJsonDigit(character) ||
            (character == U'-' && index + 1 < line.size() &&
             isJsonDigit(line[index + 1]))) {
            std::size_t end = index + 1;
            while (end < line.size()) {
                const char32_t value = line[end];
                if (isJsonDigit(value) || value == U'.' || value == U'e' ||
                    value == U'E' || value == U'+' || value == U'-') {
                    ++end;
                    continue;
                }
                break;
            }
            add(index, end - index, TokenRole::Number);
            index = end;
            continue;
        }

        if (character == U't' || character == U'f' || character == U'n') {
            const std::u32string_view rest = line.substr(index);
            std::size_t length = 0;
            if (rest.substr(0, 4) == U"true" || rest.substr(0, 4) == U"null") {
                length = 4;
            } else if (rest.substr(0, 5) == U"false") {
                length = 5;
            }
            if (length > 0) {
                add(index, length, TokenRole::Keyword);
                index += length;
                continue;
            }
        }

        add(index, 1, TokenRole::Punctuation);
        ++index;
    }

    return spans;
}

std::vector<Diagnostic> diagnoseJson(const std::vector<std::u32string>& lines) {
    std::string text;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        text += encodeLine(lines[index]);
        if (index + 1 < lines.size()) {
            text.push_back('\n');
        }
    }

    JsonError error;
    if (parseJson(text, error).has_value()) {
        return {};
    }

    Diagnostic diagnostic;
    diagnostic.line = static_cast<std::uint32_t>(error.line);
    diagnostic.column = static_cast<std::uint32_t>(error.column);
    diagnostic.length = 1;
    diagnostic.message = std::move(error.message);
    diagnostic.code = "json";
    return {std::move(diagnostic)};
}

} // namespace lucid::editor
