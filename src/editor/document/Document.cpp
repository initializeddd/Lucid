#include "editor/document/Document.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace lucid::editor {
namespace {

constexpr char32_t kReplacementCharacter = U'\uFFFD';

// The renderer displays each tab as four character cells.
std::size_t visualColumn(std::u32string_view line, std::size_t column) {
    std::size_t width = 0;
    for (std::size_t i = 0; i < std::min(column, line.size()); ++i)
        width += line[i] == U'\t' ? 4U : 1U;
    return width;
}

std::size_t columnAtVisual(std::u32string_view line, std::size_t target) {
    std::size_t width = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const std::size_t advance = line[i] == U'\t' ? 4U : 1U;
        if (target < width + (advance + 1U) / 2U) return i;
        width += advance;
    }
    return line.size();
}

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

bool Document::canEditLosslessly(std::string_view text, std::string& error, const std::atomic_bool* cancel) {
    if (cancel && cancel->load(std::memory_order_relaxed)) { error = "Text validation cancelled"; return false; }
    int ending = 0;
    std::size_t nextCancelCheck = 0;
    for (std::size_t i = 0; i < text.size();) {
        if (cancel && i >= nextCancelCheck) {
            if (cancel->load(std::memory_order_relaxed)) { error = "Text validation cancelled"; return false; }
            nextCancelCheck = i + 4096;
        }
        if (text[i] == '\r' || text[i] == '\n') {
            int current = text[i] == '\n' ? 1 : 3;
            if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') { current = 2; ++i; }
            if (ending != 0 && ending != current) { error = "Mixed line endings cannot yet be edited safely"; return false; }
            ending = current; ++i;
        } else {
            const auto start = i;
            const auto codepoint = decodeUtf8(text, i);
            if (codepoint == kReplacementCharacter && i == start + 1 && static_cast<unsigned char>(text[start]) >= 0x80U) {
                error = "File is not valid UTF-8 and cannot be edited safely"; return false;
            }
        }
    }
    if (cancel && cancel->load(std::memory_order_relaxed)) { error = "Text validation cancelled"; return false; }
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
    resetPreferredColumn();
}

void Document::selectAll() {
    m_selectionActive = true;
    m_selectionAnchorLine = 0;
    m_selectionAnchorColumn = 0;
    m_cursorLine = m_lines.size() - 1;
    m_cursorColumn = m_lines[m_cursorLine].size();
    resetPreferredColumn();
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
    resetPreferredColumn();
}

bool Document::surroundSelection(char32_t open, char32_t close) {
    if (!hasSelection()) return false;
    const auto start = selectionStart(), end = selectionEnd();
    const bool reverse = m_cursorLine == start.line && m_cursorColumn == start.column;
    beforeEdit();
    m_lines[end.line].insert(end.column, 1, close);
    m_lines[start.line].insert(start.column, 1, open);
    const TextPosition innerStart{start.line, start.column + 1};
    const TextPosition innerEnd{end.line, end.column + (start.line == end.line ? 1U : 0U)};
    const auto cursor = reverse ? innerStart : innerEnd;
    const auto anchor = reverse ? innerEnd : innerStart;
    m_cursorLine = cursor.line; m_cursorColumn = cursor.column;
    m_selectionAnchorLine = anchor.line; m_selectionAnchorColumn = anchor.column;
    m_selectionActive = true; resetPreferredColumn(); markEdited();
    return true;
}

void Document::insert(char32_t codepoint) {
    if (codepoint < 0x20U || codepoint == 0x7FU ||
        (codepoint >= 0xD800U && codepoint <= 0xDFFFU) || codepoint > 0x10FFFFU) {
        return;
    }

    if (hasSelection()) {
        const auto start = selectionStart();
        const auto end = selectionEnd();
        if (start.line == end.line && end.column == start.column + 1 &&
            m_lines[start.line][start.column] == codepoint) {
            setCursor(end.line, end.column);
            return;
        }
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

    if (hasSelection()) {
        const auto start = selectionStart();
        const auto end = selectionEnd();
        if (start.line == end.line && end.column - start.column == count &&
            std::all_of(m_lines[start.line].begin() + static_cast<std::ptrdiff_t>(start.column),
                        m_lines[start.line].begin() + static_cast<std::ptrdiff_t>(end.column),
                        [](char32_t character) { return character == U' '; })) {
            setCursor(end.line, end.column);
            return;
        }
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

bool Document::replaceAllUtf8(std::string_view text) {
    auto replacement = Document::fromUtf8(text);
    if (replacement.toUtf8() != text) return false;
    if (toUtf8() == text) return true;
    beforeEdit();
    m_lines = std::move(replacement.m_lines);
    m_selectionActive = false;
    clampCursor(); resetPreferredColumn(); markEdited();
    return true;
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

    if (normalized.empty()) return;

    // Replacing a selection with the same text only moves the caret. In
    // particular, it must not mark a saved file dirty or discard redo history.
    if (hasSelection() && selectionUtf8() == encodeUtf8(normalized)) {
        const TextPosition end = selectionEnd();
        setCursor(end.line, end.column);
        return;
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
    std::vector<std::u32string> inserted;
    while (partStart <= normalized.size()) {
        const std::size_t nextNewline = normalized.find(U'\n', partStart);
        const std::size_t partEnd =
            nextNewline == std::u32string::npos ? normalized.size() : nextNewline;
        inserted.emplace_back(normalized.substr(partStart, partEnd - partStart));
        if (nextNewline == std::u32string::npos) {
            break;
        }
        partStart = nextNewline + 1;
    }

    m_cursorLine = insertionLine + inserted.size();
    m_lines.insert(m_lines.begin() + static_cast<std::ptrdiff_t>(insertionLine + 1),
                   std::make_move_iterator(inserted.begin()),
                   std::make_move_iterator(inserted.end()));
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
    resetPreferredColumn();
    clearSelection();
    markEdited();
}

bool Document::insertJsonQuote() {
    std::u32string& line = m_lines[m_cursorLine];
    if (!hasSelection() && m_cursorColumn < line.size() &&
        line[m_cursorColumn] == U'"') {
        ++m_cursorColumn;
        resetPreferredColumn();
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
    resetPreferredColumn();
    markEdited();
    m_autoPair = AutoPair{m_cursorLine, m_cursorColumn, m_revision};
    return true;
}

bool Document::movePast(char32_t codepoint) {
    if (hasSelection()) {
        return false;
    }
    const std::u32string& line = m_lines[m_cursorLine];
    if (m_cursorColumn < line.size() && line[m_cursorColumn] == codepoint) {
        ++m_cursorColumn;
        resetPreferredColumn();
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
    const auto [first, last] = selectedLineRange();
    if (m_lines.size() == 1 && m_lines[0].empty()) return;
    beforeEdit();
    m_selectionActive = false;

    if (first == 0 && last + 1 == m_lines.size()) {
        m_lines.assign(1, std::u32string{});
        m_cursorLine = 0;
        m_cursorColumn = 0;
        resetPreferredColumn();
        markEdited();
        return;
    }

    m_lines.erase(m_lines.begin() + static_cast<std::ptrdiff_t>(first),
                  m_lines.begin() + static_cast<std::ptrdiff_t>(last + 1));
    if (first >= m_lines.size()) {
        m_cursorLine = m_lines.size() - 1;
        m_cursorColumn = m_lines[m_cursorLine].size();
    } else {
        m_cursorLine = first;
        m_cursorColumn = 0;
    }
    resetPreferredColumn();
    markEdited();
}

void Document::joinLines() {
    const auto [first, selectedLast] = selectedLineRange();
    const auto last = selectedLast == first ? std::min(first + 1, m_lines.size() - 1) : selectedLast;
    if (first == last) return;
    beforeEdit();
    auto& target = m_lines[first];
    const auto caret = target.size();
    for (std::size_t i = first + 1; i <= last; ++i) {
        const auto start = m_lines[i].find_first_not_of(U" \t");
        if (start == std::u32string::npos) continue;
        if (!target.empty() && target.back() != U' ' && target.back() != U'\t') target.push_back(U' ');
        target.append(m_lines[i], start, std::u32string::npos);
    }
    m_lines.erase(m_lines.begin() + static_cast<std::ptrdiff_t>(first + 1),
                  m_lines.begin() + static_cast<std::ptrdiff_t>(last + 1));
    m_selectionActive = false; m_cursorLine = first; m_cursorColumn = caret;
    resetPreferredColumn(); markEdited();
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
    resetPreferredColumn();
    markEdited();
}

void Document::insertLineAbove() {
    beforeEdit();
    m_selectionActive = false;
    const std::u32string indentation = m_lines[m_cursorLine].substr(
        0, m_lines[m_cursorLine].find_first_not_of(U" \t"));
    m_lines.insert(m_lines.begin() + static_cast<std::ptrdiff_t>(m_cursorLine), indentation);
    m_cursorColumn = indentation.size();
    resetPreferredColumn();
    markEdited();
}

bool Document::toggleBlockComment(std::u32string_view open, std::u32string_view close) {
    if (open.empty() || close.empty()) return false;
    const bool selected = hasSelection();
    const bool reversed = selected && (m_cursorLine < m_selectionAnchorLine ||
        (m_cursorLine == m_selectionAnchorLine && m_cursorColumn < m_selectionAnchorColumn));
    TextPosition start = selected ? selectionStart() : TextPosition{m_cursorLine, 0};
    TextPosition end = selected ? selectionEnd() : TextPosition{m_cursorLine, m_lines[m_cursorLine].size()};
    const bool wrapped = m_lines[start.line].compare(start.column, open.size(), open) == 0 &&
        end.column >= close.size() && m_lines[end.line].compare(end.column - close.size(), close.size(), close) == 0 &&
        (start.line != end.line || end.column >= start.column + open.size() + close.size());
    if (!wrapped && open == U"<!--" && end.column > 0 && m_lines[end.line][end.column - 1] == U'-') return false;
    // Even when the selection starts and ends with delimiters, it may contain
    // several separate comments. Removing only the outermost markers in that
    // case would turn intervening source into a comment (or executable code).
    for (std::size_t i = start.line; i <= end.line; ++i) {
        const auto begin = i == start.line ? start.column + (wrapped ? open.size() : 0) : 0;
        const auto limit = i == end.line ? end.column - (wrapped ? close.size() : 0) : m_lines[i].size();
        const auto part = std::u32string_view(m_lines[i]).substr(begin, limit - begin);
        if (part.find(open) != std::u32string_view::npos || part.find(close) != std::u32string_view::npos ||
            (!wrapped && open == U"<!--" && part.find(U"--") != std::u32string_view::npos)) return false;
    }
    beforeEdit();
    if (wrapped) {
        m_lines[end.line].erase(end.column - close.size(), close.size());
        m_lines[start.line].erase(start.column, open.size());
        end.column -= close.size();
        if (start.line == end.line) end.column -= open.size();
    } else {
        m_lines[end.line].insert(end.column, close);
        m_lines[start.line].insert(start.column, open);
        end.column += close.size();
        if (start.line == end.line) end.column += open.size();
    }
    m_selectionAnchorLine = reversed ? end.line : start.line;
    m_selectionAnchorColumn = reversed ? end.column : start.column;
    m_cursorLine = reversed ? start.line : end.line;
    m_cursorColumn = reversed ? start.column : end.column;
    m_selectionActive = start.line != end.line || start.column != end.column;
    resetPreferredColumn(); markEdited(); return true;
}

void Document::toggleLineComment(std::u32string_view prefix) {
    if (prefix.empty()) return;
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
        if (line.compare(start, prefix.size(), prefix) != 0) {
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
            resetPreferredColumn();
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
            resetPreferredColumn();
        }
    };

    for (std::size_t lineIndex = first; lineIndex <= last; ++lineIndex) {
        std::u32string& line = m_lines[lineIndex];
        const std::size_t start = line.find_first_not_of(U" \t");
        if (start == std::u32string::npos) {
            continue;
        }

        if (comment) {
            line.insert(start, prefix);
            line.insert(start + prefix.size(), 1, U' ');
            adjustInsert(lineIndex, start, prefix.size() + 1);
            continue;
        }

        std::size_t count = prefix.size();
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
    resetPreferredColumn();

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
    resetPreferredColumn();
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
    resetPreferredColumn();
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
    resetPreferredColumn();
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

void Document::backspace(std::size_t indentWidth) {
    if (!hasSelection() && m_cursorLine == 0 && m_cursorColumn == 0) return;
    if (!hasSelection() && m_autoPair && m_autoPair->revision == m_revision &&
        m_autoPair->line == m_cursorLine && m_autoPair->column == m_cursorColumn &&
        m_cursorColumn > 0 && m_cursorColumn < m_lines[m_cursorLine].size() &&
        m_lines[m_cursorLine][m_cursorColumn - 1] == U'"' && m_lines[m_cursorLine][m_cursorColumn] == U'"') {
        beforeEdit();
        m_lines[m_cursorLine].erase(m_cursorColumn - 1, 2); --m_cursorColumn;
        m_autoPair.reset(); markEdited(); return;
    }
    beforeEdit();
    if (eraseSelection()) {
        return;
    }

    if (m_cursorColumn > 0) {
        auto& line = m_lines[m_cursorLine];
        std::size_t remove = 1;
        if (indentWidth > 1 && line[m_cursorColumn - 1] == U' ' &&
            std::all_of(line.begin(), line.begin() + static_cast<std::ptrdiff_t>(m_cursorColumn),
                        [](char32_t c) { return c == U' ' || c == U'\t'; })) {
            const auto visual = visualColumn(line, m_cursorColumn);
            const auto previousStop = ((visual - 1) / indentWidth) * indentWidth;
            while (remove < m_cursorColumn && visual - remove > previousStop &&
                   line[m_cursorColumn - remove - 1] == U' ') ++remove;
        }
        line.erase(m_cursorColumn - remove, remove);
        m_cursorColumn -= remove;
        resetPreferredColumn();
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
    resetPreferredColumn();
}

void Document::moveDocumentStart(bool extendSelection) {
    if (extendSelection) extendSelectionTo(0, 0); else setCursor(0, 0);
}

void Document::moveDocumentEnd(bool extendSelection) {
    const auto line = m_lines.size() - 1, column = m_lines.back().size();
    if (extendSelection) extendSelectionTo(line, column); else setCursor(line, column);
}

void Document::moveParagraphUp(bool extendSelection) {
    const auto blank = [this](std::size_t line) { return m_lines[line].find_first_not_of(U" \t") == std::u32string::npos; };
    std::size_t line = m_cursorLine;
    if (!blank(line)) {
        while (line > 0 && !blank(line - 1)) --line;
        if (line != m_cursorLine || m_cursorColumn != 0) {
            if (extendSelection) extendSelectionTo(line, 0); else setCursor(line, 0);
            return;
        }
    }
    while (line > 0 && blank(line - 1)) --line;
    if (line > 0) --line;
    while (line > 0 && !blank(line - 1)) --line;
    if (extendSelection) extendSelectionTo(line, 0); else setCursor(line, 0);
}

void Document::moveParagraphDown(bool extendSelection) {
    const auto blank = [this](std::size_t line) { return m_lines[line].find_first_not_of(U" \t") == std::u32string::npos; };
    std::size_t line = m_cursorLine;
    while (line + 1 < m_lines.size() && !blank(line)) ++line;
    if (line + 1 == m_lines.size() && !blank(line)) {
        if (extendSelection) extendSelectionTo(line, m_lines[line].size()); else setCursor(line, m_lines[line].size());
        return;
    }
    while (line + 1 < m_lines.size() && blank(line)) ++line;
    const auto column = blank(line) ? m_lines[line].size() : 0;
    if (extendSelection) extendSelectionTo(line, column); else setCursor(line, column);
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
    resetPreferredColumn();
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
    resetPreferredColumn();
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
    resetPreferredColumn();
}

void Document::resetPreferredColumn() {
    m_preferredColumn = m_cursorColumn;
    m_preferredColumnIsVisual = false;
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

    if (!m_preferredColumnIsVisual) {
        m_preferredColumn = visualColumn(m_lines[m_cursorLine], m_cursorColumn);
        m_preferredColumnIsVisual = true;
    }
    --m_cursorLine;
    m_cursorColumn = columnAtVisual(m_lines[m_cursorLine], m_preferredColumn);
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

    if (!m_preferredColumnIsVisual) {
        m_preferredColumn = visualColumn(m_lines[m_cursorLine], m_cursorColumn);
        m_preferredColumnIsVisual = true;
    }
    ++m_cursorLine;
    m_cursorColumn = columnAtVisual(m_lines[m_cursorLine], m_preferredColumn);
}

void Document::moveHome(bool extendSelection) {
    if (extendSelection) {
        beginExtend();
    } else {
        clearSelection();
    }
    m_cursorColumn = 0;
    m_preferredColumn = 0; m_preferredColumnIsVisual = false;
}

void Document::moveSmartHome(bool extendSelection) {
    if (extendSelection) beginExtend(); else clearSelection();
    const auto& text = m_lines[m_cursorLine];
    const auto first = text.find_first_not_of(U" \t");
    const auto indentation = first == std::u32string::npos ? text.size() : first;
    m_cursorColumn = m_cursorColumn == indentation ? 0 : indentation;
    resetPreferredColumn();
}

void Document::moveEnd(bool extendSelection) {
    if (extendSelection) {
        beginExtend();
    } else {
        clearSelection();
    }
    m_cursorColumn = m_lines[m_cursorLine].size();
    resetPreferredColumn();
}

Document::Snapshot Document::snapshot() const {
    Snapshot value;
    value.lines = m_lines;
    value.cursorLine = m_cursorLine;
    value.cursorColumn = m_cursorColumn;
    value.preferredColumn = m_preferredColumn;
    value.preferredColumnIsVisual = m_preferredColumnIsVisual;
    value.contentState = m_contentState;
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
    m_preferredColumnIsVisual = value.preferredColumnIsVisual;
    m_contentState = value.contentState;
    m_selectionAnchorLine = value.anchorLine;
    m_selectionAnchorColumn = value.anchorColumn;
    m_selectionActive = value.selectionActive;
    clampCursor();
}

std::size_t Document::snapshotBytes(const Snapshot& value) noexcept {
    // Use logical sizes so the accounting survives copying a Document, whose
    // string and vector capacities may differ from the original's capacities.
    std::size_t bytes = sizeof(Snapshot) + value.lines.size() * sizeof(std::u32string);
    for (const auto& line : value.lines) bytes += line.size() * sizeof(char32_t);
    return bytes;
}

void Document::pushHistory(std::vector<Snapshot>& history, std::size_t& bytes, Snapshot value) {
    constexpr std::size_t kMaxUndo = 200;
    constexpr std::size_t kMaxHistoryBytes = 64U * 1024U * 1024U;
    const auto size = snapshotBytes(value);
    while (!history.empty() && (history.size() >= kMaxUndo || size > kMaxHistoryBytes || bytes > kMaxHistoryBytes - size)) {
        bytes -= snapshotBytes(history.front());
        history.erase(history.begin());
    }
    bytes += size; history.push_back(std::move(value));
}

void Document::beforeEdit() {
    pushHistory(m_undo, m_undoBytes, snapshot());
    m_redo.clear(); m_redoBytes = 0;
}

void Document::undo() {
    if (m_undo.empty()) {
        return;
    }
    pushHistory(m_redo, m_redoBytes, snapshot());
    restore(m_undo.back());
    m_undoBytes -= snapshotBytes(m_undo.back());
    m_undo.pop_back();
    ++m_revision;
    m_dirty = !m_hasSavedState || m_contentState != m_savedState;
}

void Document::redo() {
    if (m_redo.empty()) {
        return;
    }
    pushHistory(m_undo, m_undoBytes, snapshot());
    restore(m_redo.back());
    m_redoBytes -= snapshotBytes(m_redo.back());
    m_redo.pop_back();
    ++m_revision;
    m_dirty = !m_hasSavedState || m_contentState != m_savedState;
}

void Document::deleteSelection() {
    if (!hasSelection()) return;
    beforeEdit();
    (void)eraseSelection();
}

void Document::deleteWord(bool forward) { deleteWithMovement(forward, true); }
void Document::deleteToLineBoundary(bool forward) { deleteWithMovement(forward, false); }

void Document::deleteWithMovement(bool forward, bool word) {
    if (hasSelection()) { deleteSelection(); return; }
    const auto originalLine = m_cursorLine, originalColumn = m_cursorColumn;
    const auto preferred = m_preferredColumn;
    const bool preferredVisual = m_preferredColumnIsVisual;
    if (word) {
        if (forward) moveWordRight(true); else moveWordLeft(true);
    } else {
        if (forward) moveEnd(true); else moveHome(true);
    }
    const auto endLine = m_cursorLine, endColumn = m_cursorColumn;
    setCursor(originalLine, originalColumn);
    m_preferredColumn = preferred; m_preferredColumnIsVisual = preferredVisual;
    if (endLine == originalLine && endColumn == originalColumn) return;
    beforeEdit();
    extendSelectionTo(endLine, endColumn);
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
    m_contentState = m_nextContentState++;
    m_dirty = true;
    resetPreferredColumn();
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
