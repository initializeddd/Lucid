#include "editor/document/Document.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace lucid::editor {
namespace {

constexpr char32_t kReplacementCharacter = U'\uFFFD';

[[nodiscard]] bool isContinuationByte(unsigned char byte) noexcept {
    return (byte & 0xC0U) == 0x80U;
}

void appendUtf8(std::string& out, char32_t codepoint) {
    if (codepoint > 0x10FFFFU || (codepoint >= 0xD800U && codepoint <= 0xDFFFU)) {
        codepoint = kReplacementCharacter;
    }

    if (codepoint <= 0x7FU) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FFU) {
        out.push_back(static_cast<char>(0xC0U | (codepoint >> 6U)));
        out.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    } else if (codepoint <= 0xFFFFU) {
        out.push_back(static_cast<char>(0xE0U | (codepoint >> 12U)));
        out.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    } else {
        out.push_back(static_cast<char>(0xF0U | (codepoint >> 18U)));
        out.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
}

[[nodiscard]] std::string encodeUtf8(const std::u32string& text) {
    std::string result;
    result.reserve(text.size());
    for (const char32_t codepoint : text) {
        appendUtf8(result, codepoint);
    }
    return result;
}

[[nodiscard]] char32_t decodeUtf8(std::string_view text, std::size_t& index) {
    const auto first = static_cast<unsigned char>(text[index]);
    if (first < 0x80U) {
        ++index;
        return static_cast<char32_t>(first);
    }

    int length = 0;
    char32_t codepoint = 0;
    char32_t minimum = 0;
    if ((first & 0xE0U) == 0xC0U) {
        length = 2;
        codepoint = static_cast<char32_t>(first & 0x1FU);
        minimum = 0x80U;
    } else if ((first & 0xF0U) == 0xE0U) {
        length = 3;
        codepoint = static_cast<char32_t>(first & 0x0FU);
        minimum = 0x800U;
    } else if ((first & 0xF8U) == 0xF0U) {
        length = 4;
        codepoint = static_cast<char32_t>(first & 0x07U);
        minimum = 0x10000U;
    } else {
        ++index;
        return kReplacementCharacter;
    }

    if (index + static_cast<std::size_t>(length) > text.size()) {
        ++index;
        return kReplacementCharacter;
    }

    for (int byteIndex = 1; byteIndex < length; ++byteIndex) {
        const auto byte = static_cast<unsigned char>(text[index + static_cast<std::size_t>(byteIndex)]);
        if (!isContinuationByte(byte)) {
            ++index;
            return kReplacementCharacter;
        }
        codepoint = static_cast<char32_t>((codepoint << 6U) | static_cast<char32_t>(byte & 0x3FU));
    }

    if (codepoint < minimum || codepoint > 0x10FFFFU ||
        (codepoint >= 0xD800U && codepoint <= 0xDFFFU)) {
        ++index;
        return kReplacementCharacter;
    }

    index += static_cast<std::size_t>(length);
    return codepoint;
}

[[nodiscard]] std::u32string decodeUtf8(std::string_view text) {
    std::u32string result;
    result.reserve(text.size());

    std::size_t index = 0;
    while (index < text.size()) {
        result.push_back(decodeUtf8(text, index));
    }
    return result;
}

[[nodiscard]] std::vector<std::u32string> splitLines(const std::u32string& text) {
    std::vector<std::u32string> lines;
    std::u32string current;

    for (std::size_t index = 0; index < text.size(); ++index) {
        char32_t character = text[index];
        if (character == U'\r') {
            if (index + 1 < text.size() && text[index + 1] == U'\n') {
                ++index;
            }
            character = U'\n';
        }

        if (character == U'\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(character);
        }
    }

    lines.push_back(current);
    return lines;
}

} // namespace

Document Document::fromUtf8(std::string_view text) {
    Document document;
    document.m_lines = splitLines(decodeUtf8(text));
    return document;
}

std::string Document::toUtf8() const {
    std::size_t size = m_lines.empty() ? 0 : m_lines.size() - 1;
    for (const std::u32string& line : m_lines) {
        size += line.size();
    }

    std::string result;
    result.reserve(size);

    for (std::size_t index = 0; index < m_lines.size(); ++index) {
        for (char32_t codepoint : m_lines[index]) {
            appendUtf8(result, codepoint);
        }
        if (index + 1 < m_lines.size()) {
            result.push_back('\n');
        }
    }

    return result;
}

bool Document::canEditLosslessly(std::string_view text, std::string& error) {
    std::string normalized;
    normalized.reserve(text.size());
    int ending = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r' || text[i] == '\n') {
            int current = text[i] == '\n' ? 1 : 3;
            if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') { current = 2; ++i; }
            if (ending != 0 && ending != current) { error = "Mixed line endings cannot yet be edited safely"; return false; }
            ending = current; normalized.push_back('\n');
        } else normalized.push_back(text[i]);
    }
    if (Document::fromUtf8(text).toUtf8() != normalized) { error = "File is not valid UTF-8 and cannot be edited safely"; return false; }
    error.clear(); return true;
}

bool Document::serializeForFile(std::string_view original, std::string& output, std::string& error) const {
    if (!canEditLosslessly(original, error)) return false;
    const auto first = original.find_first_of("\r\n");
    const std::string_view ending = first == std::string_view::npos || original[first] == '\n' ? "\n" :
        (first + 1 < original.size() && original[first + 1] == '\n' ? "\r\n" : "\r");
    const auto normalized = toUtf8();
    output.clear(); output.reserve(normalized.size());
    for (const char c : normalized) { if (c == '\n') output.append(ending); else output.push_back(c); }
    error.clear(); return true;
}

const std::u32string& Document::line(std::size_t index) const {
    if (index >= m_lines.size()) {
        throw std::out_of_range("Document line index is out of range");
    }
    return m_lines[index];
}

bool Document::hasSelection() const noexcept {
    return m_selectionActive &&
           (m_selectionAnchorLine != m_cursorLine ||
            m_selectionAnchorColumn != m_cursorColumn);
}

TextPosition Document::selectionStart() const noexcept {
    if (m_selectionAnchorLine < m_cursorLine ||
        (m_selectionAnchorLine == m_cursorLine &&
         m_selectionAnchorColumn <= m_cursorColumn)) {
        return TextPosition{m_selectionAnchorLine, m_selectionAnchorColumn};
    }
    return TextPosition{m_cursorLine, m_cursorColumn};
}

TextPosition Document::selectionEnd() const noexcept {
    if (m_selectionAnchorLine < m_cursorLine ||
        (m_selectionAnchorLine == m_cursorLine &&
         m_selectionAnchorColumn <= m_cursorColumn)) {
        return TextPosition{m_cursorLine, m_cursorColumn};
    }
    return TextPosition{m_selectionAnchorLine, m_selectionAnchorColumn};
}

std::string Document::selectionUtf8() const {
    if (!hasSelection()) {
        return {};
    }

    const TextPosition start = selectionStart();
    const TextPosition end = selectionEnd();
    std::string result;

    if (start.line == end.line) {
        return encodeUtf8(m_lines[start.line].substr(start.column,
                                                     end.column - start.column));
    }

    result += encodeUtf8(m_lines[start.line].substr(start.column));
    for (std::size_t lineIndex = start.line + 1; lineIndex < end.line; ++lineIndex) {
        result.push_back('\n');
        result += encodeUtf8(m_lines[lineIndex]);
    }
    result.push_back('\n');
    result += encodeUtf8(m_lines[end.line].substr(0, end.column));
    return result;
}

std::string Document::lineUtf8(std::size_t index) const {
    return encodeUtf8(line(index));
}

void Document::selectWord() {
    const auto lineIndex = m_cursorLine;
    const auto& text = m_lines[lineIndex];
    if (text.empty()) return;
    auto category = [](char32_t c) {
        if (c == U' ' || c == U'\t') return 0;
        if ((c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') ||
            (c >= U'0' && c <= U'9') || c == U'_' || c >= 128) return 1;
        return 2;
    };
    auto index = std::min(m_cursorColumn, text.size() - 1);
    const int kind = category(text[index]);
    auto start = index;
    auto end = index + 1;
    while (start > 0 && category(text[start - 1]) == kind) --start;
    while (end < text.size() && category(text[end]) == kind) ++end;
    setCursor(lineIndex, start);
    extendSelectionTo(lineIndex, end);
}

void Document::selectLine() {
    const auto lineIndex = m_cursorLine;
    setCursor(lineIndex, 0);
    if (lineIndex + 1 < m_lines.size()) extendSelectionTo(lineIndex + 1, 0);
    else extendSelectionTo(lineIndex, m_lines[lineIndex].size());
}

void Document::setCursor(std::size_t lineIndex, std::size_t column) {
    m_selectionActive = false;
    m_cursorLine = lineIndex;
    m_cursorColumn = column;
    clampCursor();
    m_preferredColumn = m_cursorColumn;
}

void Document::selectAll() {
    m_selectionActive = true;
    m_selectionAnchorLine = 0;
    m_selectionAnchorColumn = 0;
    m_cursorLine = m_lines.size() - 1;
    m_cursorColumn = m_lines[m_cursorLine].size();
    m_preferredColumn = m_cursorColumn;
}

void Document::extendSelectionTo(std::size_t lineIndex, std::size_t column) {
    if (!m_selectionActive) {
        m_selectionAnchorLine = m_cursorLine;
        m_selectionAnchorColumn = m_cursorColumn;
        m_selectionActive = true;
    }
    m_cursorLine = lineIndex;
    m_cursorColumn = column;
    clampCursor();
    m_preferredColumn = m_cursorColumn;
}

void Document::insert(char32_t codepoint) {
    if (codepoint < 0x20U || codepoint == 0x7FU ||
        (codepoint >= 0xD800U && codepoint <= 0xDFFFU) || codepoint > 0x10FFFFU) {
        return;
    }

    beforeEdit();
    (void)eraseSelection();
    m_lines[m_cursorLine].insert(m_cursorColumn, 1, codepoint);
    ++m_cursorColumn;
    markEdited();
}

void Document::insertSpaces(std::size_t count) {
    if (count == 0) {
        return;
    }

    beforeEdit();
    (void)eraseSelection();
    m_lines[m_cursorLine].insert(m_cursorColumn, count, U' ');
    m_cursorColumn += count;
    markEdited();
}

void Document::newline() {
    beforeEdit();
    (void)eraseSelection();
    std::u32string& line = m_lines[m_cursorLine];

    std::size_t indentLength = 0;
    while (indentLength < line.size() && indentLength < m_cursorColumn &&
           (line[indentLength] == U' ' || line[indentLength] == U'\t')) {
        ++indentLength;
    }

    std::u32string indentation = line.substr(0, indentLength);
    std::u32string tail = line.substr(m_cursorColumn);
    line.erase(m_cursorColumn);

    indentation += tail;
    const auto insertPosition =
        m_lines.begin() + static_cast<std::ptrdiff_t>(m_cursorLine + 1);
    m_lines.insert(insertPosition, std::move(indentation));

    ++m_cursorLine;
    m_cursorColumn = indentLength;
    markEdited();
}

void Document::pasteUtf8(std::string_view text) {
    std::u32string pasted = decodeUtf8(text);
    if (pasted.empty()) {
        return;
    }

    std::u32string normalized;
    normalized.reserve(pasted.size());
    for (std::size_t index = 0; index < pasted.size(); ++index) {
        const char32_t character = pasted[index];
        if (character == U'\r') {
            if (index + 1 < pasted.size() && pasted[index + 1] == U'\n') {
                ++index;
            }
            normalized.push_back(U'\n');
        } else if (character == U'\t' || character == U'\n' ||
                   (character >= 0x20U && character != 0x7FU)) {
            normalized.push_back(character);
        }
    }

    beforeEdit();
    (void)eraseSelection();

    const std::size_t firstNewline = normalized.find(U'\n');
    std::u32string& line = m_lines[m_cursorLine];
    if (firstNewline == std::u32string::npos) {
        line.insert(m_cursorColumn, normalized);
        m_cursorColumn += normalized.size();
        markEdited();
        return;
    }

    std::u32string tail = line.substr(m_cursorColumn);
    line.erase(m_cursorColumn);
    line += normalized.substr(0, firstNewline);

    const std::size_t insertionLine = m_cursorLine;
    std::size_t partStart = firstNewline + 1;
    std::size_t insertedLines = 0;
    while (partStart <= normalized.size()) {
        const std::size_t nextNewline = normalized.find(U'\n', partStart);
        const std::size_t partEnd =
            nextNewline == std::u32string::npos ? normalized.size() : nextNewline;
        std::u32string part = normalized.substr(partStart, partEnd - partStart);
        m_lines.insert(m_lines.begin() +
                           static_cast<std::ptrdiff_t>(insertionLine + insertedLines + 1),
                       std::move(part));
        ++insertedLines;
        if (nextNewline == std::u32string::npos) {
            break;
        }
        partStart = nextNewline + 1;
    }

    m_cursorLine = insertionLine + insertedLines;
    m_lines[m_cursorLine] += tail;
    m_cursorColumn = m_lines[m_cursorLine].size() - tail.size();
    markEdited();
}

void Document::insertJsonBlock(char32_t open, char32_t close,
                               std::size_t spaces) {
    beforeEdit();
    (void)eraseSelection();

    std::u32string& line = m_lines[m_cursorLine];
    std::size_t indentLength = 0;
    while (indentLength < line.size() &&
           (line[indentLength] == U' ' || line[indentLength] == U'\t')) {
        ++indentLength;
    }

    const std::u32string indentation = line.substr(0, indentLength);
    std::u32string tail = line.substr(m_cursorColumn);
    line.erase(m_cursorColumn);
    line.push_back(open);

    const std::u32string innerIndent = indentation + std::u32string(spaces, U' ');
    const std::u32string closing = indentation + close + tail;

    const auto insertionLine =
        m_lines.begin() + static_cast<std::ptrdiff_t>(m_cursorLine + 1);
    m_lines.insert(insertionLine, innerIndent);
    m_lines.insert(m_lines.begin() +
                       static_cast<std::ptrdiff_t>(m_cursorLine + 2),
                   closing);

    ++m_cursorLine;
    m_cursorColumn = innerIndent.size();
    m_preferredColumn = m_cursorColumn;
    clearSelection();
    markEdited();
}

bool Document::insertJsonQuote() {
    std::u32string& line = m_lines[m_cursorLine];
    if (!hasSelection() && m_cursorColumn < line.size() &&
        line[m_cursorColumn] == U'"') {
        ++m_cursorColumn;
        m_preferredColumn = m_cursorColumn;
        return true;
    }

    if (!hasSelection()) {
        std::size_t previous = m_cursorColumn;
        while (previous > 0 && (line[previous - 1] == U' ' ||
                                line[previous - 1] == U'\t')) {
            --previous;
        }
        const bool valueStart =
            previous == 0 || line[previous - 1] == U'{' ||
            line[previous - 1] == U'[' || line[previous - 1] == U',' ||
            line[previous - 1] == U':';
        if (!valueStart) {
            return false;
        }
    }

    beforeEdit();
    (void)eraseSelection();
    m_lines[m_cursorLine].insert(m_cursorColumn, 2, U'"');
    ++m_cursorColumn;
    m_preferredColumn = m_cursorColumn;
    markEdited();
    return true;
}

bool Document::movePast(char32_t codepoint) {
    if (hasSelection()) {
        return false;
    }
    const std::u32string& line = m_lines[m_cursorLine];
    if (m_cursorColumn < line.size() && line[m_cursorColumn] == codepoint) {
        ++m_cursorColumn;
        m_preferredColumn = m_cursorColumn;
        return true;
    }
    return false;
}

void Document::duplicateLine() {
    if (hasSelection()) {
        const auto [first, last] = selectedLineRange();
        const auto count = last - first + 1;
        const std::vector<std::u32string> copy(m_lines.begin() + static_cast<std::ptrdiff_t>(first),
                                               m_lines.begin() + static_cast<std::ptrdiff_t>(last + 1));
        beforeEdit();
        m_lines.insert(m_lines.begin() + static_cast<std::ptrdiff_t>(last + 1), copy.begin(), copy.end());
        m_cursorLine += count;
        m_selectionAnchorLine += count;
        markEdited();
        return;
    }
    beforeEdit();
    m_selectionActive = false;
    const std::u32string duplicate = m_lines[m_cursorLine];
    m_lines.insert(m_lines.begin() + static_cast<std::ptrdiff_t>(m_cursorLine + 1),
                   duplicate);
    ++m_cursorLine;
    clampCursor();
    markEdited();
}

void Document::deleteLine() {
    if (m_lines.size() == 1 && m_lines[0].empty() && !hasSelection()) return;
    beforeEdit();
    m_selectionActive = false;

    if (m_lines.size() == 1) {
        m_lines[0].clear();
        m_cursorColumn = 0;
        m_preferredColumn = 0;
        markEdited();
        return;
    }

    m_lines.erase(m_lines.begin() + static_cast<std::ptrdiff_t>(m_cursorLine));
    if (m_cursorLine >= m_lines.size()) {
        m_cursorLine = m_lines.size() - 1;
        m_cursorColumn = m_lines[m_cursorLine].size();
    } else {
        m_cursorColumn = 0;
    }
    m_preferredColumn = m_cursorColumn;
    markEdited();
}

void Document::insertLineBelow() {
    beforeEdit();
    m_selectionActive = false;
    const std::u32string indentation = m_lines[m_cursorLine].substr(
        0, m_lines[m_cursorLine].find_first_not_of(U" \t"));
    m_lines.insert(
        m_lines.begin() + static_cast<std::ptrdiff_t>(m_cursorLine + 1), indentation);
    ++m_cursorLine;
    m_cursorColumn = indentation.size();
    m_preferredColumn = m_cursorColumn;
    markEdited();
}

void Document::insertLineAbove() {
    beforeEdit();
    m_selectionActive = false;
    const std::u32string indentation = m_lines[m_cursorLine].substr(
        0, m_lines[m_cursorLine].find_first_not_of(U" \t"));
    m_lines.insert(m_lines.begin() + static_cast<std::ptrdiff_t>(m_cursorLine), indentation);
    m_cursorColumn = indentation.size();
    m_preferredColumn = m_cursorColumn;
    markEdited();
}

void Document::toggleLineComment() {
    const auto [first, last] = selectedLineRange();
    bool anyText = false;
    bool allCommented = true;

    for (std::size_t lineIndex = first; lineIndex <= last; ++lineIndex) {
        const std::u32string& line = m_lines[lineIndex];
        const std::size_t start = line.find_first_not_of(U" \t");
        if (start == std::u32string::npos) {
            continue;
        }
        anyText = true;
        if (line.substr(start, 2) != U"//") {
            allCommented = false;
        }
    }

    if (!anyText) {
        return;
    }

    beforeEdit();
    const bool comment = !allCommented;
    const auto adjustInsert = [this](std::size_t lineIndex, std::size_t position,
                                     std::size_t count) {
        const auto adjust = [lineIndex, position, count](std::size_t& line,
                                                         std::size_t& column) {
            if (line == lineIndex && column >= position) {
                column += count;
            }
        };
        adjust(m_cursorLine, m_cursorColumn);
        adjust(m_selectionAnchorLine, m_selectionAnchorColumn);
        if (m_cursorLine == lineIndex) {
            m_preferredColumn = m_cursorColumn;
        }
    };
    const auto adjustErase = [this](std::size_t lineIndex, std::size_t position,
                                    std::size_t count) {
        const auto adjust = [lineIndex, position, count](std::size_t& line,
                                                         std::size_t& column) {
            if (line != lineIndex) {
                return;
            }
            if (column >= position + count) {
                column -= count;
            } else if (column > position) {
                column = position;
            }
        };
        adjust(m_cursorLine, m_cursorColumn);
        adjust(m_selectionAnchorLine, m_selectionAnchorColumn);
        if (m_cursorLine == lineIndex) {
            m_preferredColumn = m_cursorColumn;
        }
    };

    for (std::size_t lineIndex = first; lineIndex <= last; ++lineIndex) {
        std::u32string& line = m_lines[lineIndex];
        const std::size_t start = line.find_first_not_of(U" \t");
        if (start == std::u32string::npos) {
            continue;
        }

        if (comment) {
            line.insert(start, U"// ");
            adjustInsert(lineIndex, start, 3);
            continue;
        }

        std::size_t count = 2;
        if (start + count < line.size() && line[start + count] == U' ') {
            ++count;
        }
        line.erase(start, count);
        adjustErase(lineIndex, start, count);
    }

    markEdited();
}

void Document::indentLines(std::size_t spaces) {
    if (spaces == 0) {
        return;
    }

    const auto [first, last] = selectedLineRange();
    beforeEdit();
    const auto adjust = [first, last, spaces](std::size_t line, std::size_t& column) {
        if (line >= first && line <= last) {
            column += spaces;
        }
    };
    adjust(m_cursorLine, m_cursorColumn);
    adjust(m_selectionAnchorLine, m_selectionAnchorColumn);
    m_preferredColumn = m_cursorColumn;

    for (std::size_t lineIndex = first; lineIndex <= last; ++lineIndex) {
        m_lines[lineIndex].insert(0, spaces, U' ');
    }
    markEdited();
}

void Document::outdentLines(std::size_t spaces) {
    if (spaces == 0) {
        return;
    }

    const auto [first, last] = selectedLineRange();
    bool hasIndent = false;
    for (std::size_t i = first; i <= last; ++i)
        if (!m_lines[i].empty() && (m_lines[i][0] == U' ' || m_lines[i][0] == U'\t')) { hasIndent = true; break; }
    if (!hasIndent) return;
    beforeEdit();
    const auto adjust = [](std::size_t lineIndex, std::size_t& line,
                           std::size_t& column, std::size_t removed) {
        if (line != lineIndex) {
            return;
        }
        if (column > removed) {
            column -= removed;
        } else {
            column = 0;
        }
    };

    for (std::size_t lineIndex = first; lineIndex <= last; ++lineIndex) {
        std::u32string& line = m_lines[lineIndex];
        std::size_t removed = 0;
        if (!line.empty() && line[0] == U'\t') {
            removed = 1;
        } else {
            while (removed < spaces && removed < line.size() &&
                   line[removed] == U' ') {
                ++removed;
            }
        }
        if (removed == 0) {
            continue;
        }
        line.erase(0, removed);
        adjust(lineIndex, m_cursorLine, m_cursorColumn, removed);
        adjust(lineIndex, m_selectionAnchorLine, m_selectionAnchorColumn, removed);
    }
    m_preferredColumn = m_cursorColumn;
    markEdited();
}

void Document::moveLineUp() {
    const auto [first, last] = selectedLineRange();
    if (first == 0) {
        return;
    }
    beforeEdit();
    std::rotate(m_lines.begin() + static_cast<std::ptrdiff_t>(first - 1),
                m_lines.begin() + static_cast<std::ptrdiff_t>(first),
                m_lines.begin() + static_cast<std::ptrdiff_t>(last + 1));
    --m_cursorLine;
    if (m_selectionActive) --m_selectionAnchorLine;
    clampCursor();
    m_preferredColumn = m_cursorColumn;
    markEdited();
}

void Document::moveLineDown() {
    const auto [first, last] = selectedLineRange();
    if (last + 1 >= m_lines.size()) {
        return;
    }
    beforeEdit();
    std::rotate(m_lines.begin() + static_cast<std::ptrdiff_t>(first),
                m_lines.begin() + static_cast<std::ptrdiff_t>(last + 1),
                m_lines.begin() + static_cast<std::ptrdiff_t>(last + 2));
    ++m_cursorLine;
    if (m_selectionActive) ++m_selectionAnchorLine;
    if (m_cursorLine >= m_lines.size()) { m_cursorLine = m_lines.size() - 1; m_cursorColumn = m_lines.back().size(); }
    if (m_selectionActive && m_selectionAnchorLine >= m_lines.size()) { m_selectionAnchorLine = m_lines.size() - 1; m_selectionAnchorColumn = m_lines.back().size(); }
    clampCursor();
    m_preferredColumn = m_cursorColumn;
    markEdited();
}

std::pair<std::size_t, std::size_t> Document::selectedLineRange() const {
    if (!hasSelection()) {
        return {m_cursorLine, m_cursorLine};
    }
    const TextPosition start = selectionStart();
    const TextPosition end = selectionEnd();
    std::size_t last = end.line;
    if (last > start.line && end.column == 0) {
        --last;
    }
    return {start.line, last};
}

void Document::backspace() {
    if (!hasSelection() && m_cursorLine == 0 && m_cursorColumn == 0) return;
    beforeEdit();
    if (eraseSelection()) {
        return;
    }

    if (m_cursorColumn > 0) {
        m_lines[m_cursorLine].erase(m_cursorColumn - 1, 1);
        --m_cursorColumn;
        markEdited();
        return;
    }

    if (m_cursorLine == 0) {
        return;
    }

    const std::size_t previousLength = m_lines[m_cursorLine - 1].size();
    m_lines[m_cursorLine - 1] += m_lines[m_cursorLine];
    m_lines.erase(m_lines.begin() + static_cast<std::ptrdiff_t>(m_cursorLine));
    --m_cursorLine;
    m_cursorColumn = previousLength;
    markEdited();
}

void Document::deleteForward() {
    if (!hasSelection() && m_cursorLine + 1 == m_lines.size() &&
        m_cursorColumn == m_lines[m_cursorLine].size()) return;
    beforeEdit();
    if (eraseSelection()) {
        return;
    }

    if (m_cursorColumn < m_lines[m_cursorLine].size()) {
        m_lines[m_cursorLine].erase(m_cursorColumn, 1);
        markEdited();
        return;
    }

    if (m_cursorLine + 1 >= m_lines.size()) {
        return;
    }

    m_lines[m_cursorLine] += m_lines[m_cursorLine + 1];
    m_lines.erase(m_lines.begin() + static_cast<std::ptrdiff_t>(m_cursorLine + 1));
    markEdited();
}

void Document::moveWordLeft(bool extendSelection) {
    if (!extendSelection && hasSelection()) { const auto start = selectionStart(); setCursor(start.line, start.column); return; }
    if (extendSelection) beginExtend(); else clearSelection();
    if (m_cursorColumn == 0 && m_cursorLine > 0) {
        --m_cursorLine;
        m_cursorColumn = m_lines[m_cursorLine].size();
    } else {
        const auto& text = m_lines[m_cursorLine];
        while (m_cursorColumn > 0 && (text[m_cursorColumn - 1] == U' ' || text[m_cursorColumn - 1] == U'\t')) --m_cursorColumn;
        if (m_cursorColumn > 0) {
            const auto word = [](char32_t c) { return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') || (c >= U'0' && c <= U'9') || c == U'_' || c >= 128; };
            const bool kind = word(text[m_cursorColumn - 1]);
            while (m_cursorColumn > 0 && text[m_cursorColumn - 1] != U' ' && text[m_cursorColumn - 1] != U'\t' && word(text[m_cursorColumn - 1]) == kind) --m_cursorColumn;
        }
    }
    m_preferredColumn = m_cursorColumn;
}

void Document::moveWordRight(bool extendSelection) {
    if (!extendSelection && hasSelection()) { const auto end = selectionEnd(); setCursor(end.line, end.column); return; }
    if (extendSelection) beginExtend(); else clearSelection();
    const auto& text = m_lines[m_cursorLine];
    if (m_cursorColumn == text.size() && m_cursorLine + 1 < m_lines.size()) {
        ++m_cursorLine; m_cursorColumn = 0;
    } else {
        const auto word = [](char32_t c) { return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') || (c >= U'0' && c <= U'9') || c == U'_' || c >= 128; };
        if (m_cursorColumn < text.size() && text[m_cursorColumn] != U' ' && text[m_cursorColumn] != U'\t') {
            const bool kind = word(text[m_cursorColumn]);
            while (m_cursorColumn < text.size() && text[m_cursorColumn] != U' ' && text[m_cursorColumn] != U'\t' && word(text[m_cursorColumn]) == kind) ++m_cursorColumn;
        }
        while (m_cursorColumn < text.size() && (text[m_cursorColumn] == U' ' || text[m_cursorColumn] == U'\t')) ++m_cursorColumn;
    }
    m_preferredColumn = m_cursorColumn;
}

void Document::moveLeft(bool extendSelection) {
    if (!extendSelection && hasSelection()) { const auto start = selectionStart(); setCursor(start.line, start.column); return; }
    if (extendSelection) {
        beginExtend();
    } else {
        clearSelection();
    }

    if (m_cursorColumn > 0) {
        --m_cursorColumn;
    } else if (m_cursorLine > 0) {
        --m_cursorLine;
        m_cursorColumn = m_lines[m_cursorLine].size();
    }
    m_preferredColumn = m_cursorColumn;
}

void Document::moveRight(bool extendSelection) {
    if (!extendSelection && hasSelection()) { const auto end = selectionEnd(); setCursor(end.line, end.column); return; }
    if (extendSelection) {
        beginExtend();
    } else {
        clearSelection();
    }

    if (m_cursorColumn < m_lines[m_cursorLine].size()) {
        ++m_cursorColumn;
    } else if (m_cursorLine + 1 < m_lines.size()) {
        ++m_cursorLine;
        m_cursorColumn = 0;
    }
    m_preferredColumn = m_cursorColumn;
}

void Document::moveUp(bool extendSelection) {
    if (extendSelection) {
        beginExtend();
    } else {
        clearSelection();
    }

    if (m_cursorLine == 0) {
        return;
    }

    --m_cursorLine;
    m_cursorColumn = std::min(m_preferredColumn, m_lines[m_cursorLine].size());
}

void Document::moveDown(bool extendSelection) {
    if (extendSelection) {
        beginExtend();
    } else {
        clearSelection();
    }

    if (m_cursorLine + 1 >= m_lines.size()) {
        return;
    }

    ++m_cursorLine;
    m_cursorColumn = std::min(m_preferredColumn, m_lines[m_cursorLine].size());
}

void Document::moveHome(bool extendSelection) {
    if (extendSelection) {
        beginExtend();
    } else {
        clearSelection();
    }
    m_cursorColumn = 0;
    m_preferredColumn = 0;
}

void Document::moveSmartHome(bool extendSelection) {
    if (extendSelection) beginExtend(); else clearSelection();
    const auto& text = m_lines[m_cursorLine];
    const auto first = text.find_first_not_of(U" \t");
    const auto indentation = first == std::u32string::npos ? text.size() : first;
    m_cursorColumn = m_cursorColumn == indentation ? 0 : indentation;
    m_preferredColumn = m_cursorColumn;
}

void Document::moveEnd(bool extendSelection) {
    if (extendSelection) {
        beginExtend();
    } else {
        clearSelection();
    }
    m_cursorColumn = m_lines[m_cursorLine].size();
    m_preferredColumn = m_cursorColumn;
}

Document::Snapshot Document::snapshot() const {
    Snapshot value;
    value.lines = m_lines;
    value.cursorLine = m_cursorLine;
    value.cursorColumn = m_cursorColumn;
    value.preferredColumn = m_preferredColumn;
    value.anchorLine = m_selectionAnchorLine;
    value.anchorColumn = m_selectionAnchorColumn;
    value.selectionActive = m_selectionActive;
    return value;
}

void Document::restore(const Snapshot& value) {
    m_lines = value.lines;
    m_cursorLine = value.cursorLine;
    m_cursorColumn = value.cursorColumn;
    m_preferredColumn = value.preferredColumn;
    m_selectionAnchorLine = value.anchorLine;
    m_selectionAnchorColumn = value.anchorColumn;
    m_selectionActive = value.selectionActive;
    clampCursor();
}

void Document::beforeEdit() {
    constexpr std::size_t kMaxUndo = 200;
    if (m_undo.size() >= kMaxUndo) {
        m_undo.erase(m_undo.begin());
    }
    m_undo.push_back(snapshot());
    m_redo.clear();
}

void Document::undo() {
    if (m_undo.empty()) {
        return;
    }
    m_redo.push_back(snapshot());
    restore(m_undo.back());
    m_undo.pop_back();
    ++m_revision;
    m_dirty = true;
}

void Document::redo() {
    if (m_redo.empty()) {
        return;
    }
    m_undo.push_back(snapshot());
    restore(m_redo.back());
    m_redo.pop_back();
    ++m_revision;
    m_dirty = true;
}

void Document::deleteSelection() {
    if (!hasSelection()) return;
    beforeEdit();
    (void)eraseSelection();
}

void Document::beginExtend() {
    if (!m_selectionActive) {
        m_selectionAnchorLine = m_cursorLine;
        m_selectionAnchorColumn = m_cursorColumn;
        m_selectionActive = true;
    }
}

bool Document::eraseSelection() {
    if (!hasSelection()) {
        m_selectionActive = false;
        return false;
    }

    const TextPosition start = selectionStart();
    const TextPosition end = selectionEnd();
    if (start.line == end.line) {
        m_lines[start.line].erase(start.column, end.column - start.column);
    } else {
        m_lines[start.line].erase(start.column);
        m_lines[start.line] += m_lines[end.line].substr(end.column);
        m_lines.erase(m_lines.begin() + static_cast<std::ptrdiff_t>(start.line + 1),
                      m_lines.begin() + static_cast<std::ptrdiff_t>(end.line + 1));
    }

    m_cursorLine = start.line;
    m_cursorColumn = start.column;
    m_selectionActive = false;
    markEdited();
    return true;
}

void Document::markEdited() {
    ++m_revision;
    m_dirty = true;
    m_preferredColumn = m_cursorColumn;
}

void Document::clampCursor() {
    if (m_lines.empty()) {
        m_lines.emplace_back();
    }
    if (m_cursorLine >= m_lines.size()) {
        m_cursorLine = m_lines.size() - 1;
    }
    m_cursorColumn = std::min(m_cursorColumn, m_lines[m_cursorLine].size());
    if (m_selectionActive) {
        m_selectionAnchorLine = std::min(m_selectionAnchorLine, m_lines.size() - 1);
        m_selectionAnchorColumn =
            std::min(m_selectionAnchorColumn, m_lines[m_selectionAnchorLine].size());
    }
}

} // namespace lucid::editor
