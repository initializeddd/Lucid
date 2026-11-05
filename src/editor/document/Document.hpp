#pragma once

#include <cstddef>
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lucid::editor {

struct TextPosition {
    std::size_t line = 0;
    std::size_t column = 0;
};

class Document {
public:
    static Document fromUtf8(std::string_view text);

    [[nodiscard]] std::string toUtf8() const;
    [[nodiscard]] static bool canEditLosslessly(std::string_view text, std::string& error, const std::atomic_bool* cancel = nullptr);
    [[nodiscard]] bool serializeForFile(std::string_view original, std::string& output, std::string& error) const;

    [[nodiscard]] const std::vector<std::u32string>& lines() const noexcept { return m_lines; }
    [[nodiscard]] std::size_t lineCount() const noexcept { return m_lines.size(); }
    [[nodiscard]] const std::u32string& line(std::size_t index) const;
    [[nodiscard]] std::size_t cursorLine() const noexcept { return m_cursorLine; }
    [[nodiscard]] std::size_t cursorColumn() const noexcept { return m_cursorColumn; }
    [[nodiscard]] bool dirty() const noexcept { return m_dirty; }
    [[nodiscard]] std::uint64_t revision() const noexcept { return m_revision; }

    [[nodiscard]] bool hasSelection() const noexcept;
    [[nodiscard]] TextPosition selectionStart() const noexcept;
    [[nodiscard]] TextPosition selectionEnd() const noexcept;
    [[nodiscard]] std::string selectionUtf8() const;
    [[nodiscard]] std::string lineUtf8(std::size_t index) const;

    void markSaved() noexcept { m_savedState = m_contentState; m_hasSavedState = true; m_dirty = false; }
    void markRecovered() noexcept { m_hasSavedState = false; m_dirty = true; }
    void setCursor(std::size_t line, std::size_t column);
    void clearSelection() noexcept { m_selectionActive = false; }
    void selectAll();
    void selectWord();
    void selectLine();
    void extendSelectionTo(std::size_t line, std::size_t column);

    void insert(char32_t codepoint);
    [[nodiscard]] bool surroundSelection(char32_t open, char32_t close);
    void insertSpaces(std::size_t count);
    void newline();
    void pasteUtf8(std::string_view text);
    // Replace the document as one undoable edit, preserving the current cursor
    // as closely as possible. Reject text that cannot round-trip unchanged.
    [[nodiscard]] bool replaceAllUtf8(std::string_view text);
    void insertJsonBlock(char32_t open, char32_t close, std::size_t spaces = 4);
    [[nodiscard]] bool insertJsonQuote();
    [[nodiscard]] bool movePast(char32_t codepoint);
    void duplicateLine();
    void deleteLine();
    void joinLines();
    void insertLineBelow();
    void insertLineAbove();
    void toggleLineComment(std::u32string_view prefix = U"//");
    [[nodiscard]] bool toggleBlockComment(std::u32string_view open, std::u32string_view close);
    void indentLines(std::size_t spaces = 4);
    void outdentLines(std::size_t spaces = 4);
    void moveLineUp();
    void moveLineDown();
    void deleteSelection();
    void deleteWord(bool forward);
    void deleteToLineBoundary(bool forward);
    void undo();
    void redo();
    void backspace(std::size_t indentWidth = 4);
    void deleteForward();
    void moveWordLeft(bool extendSelection = false);
    void moveWordRight(bool extendSelection = false);
    void moveLeft(bool extendSelection = false);
    void moveRight(bool extendSelection = false);
    void moveUp(bool extendSelection = false);
    void moveDown(bool extendSelection = false);
    void moveHome(bool extendSelection = false);
    void moveSmartHome(bool extendSelection = false);
    void moveEnd(bool extendSelection = false);
    void moveDocumentStart(bool extendSelection = false);
    void moveDocumentEnd(bool extendSelection = false);
    void moveParagraphUp(bool extendSelection = false);
    void moveParagraphDown(bool extendSelection = false);

private:
    struct Snapshot {
        std::vector<std::u32string> lines;
        std::size_t cursorLine = 0;
        std::size_t cursorColumn = 0;
        std::size_t preferredColumn = 0;
        bool preferredColumnIsVisual = false;
        std::uint64_t contentState = 0;
        std::size_t anchorLine = 0;
        std::size_t anchorColumn = 0;
        bool selectionActive = false;
    };
    struct AutoPair { std::size_t line; std::size_t column; std::uint64_t revision; };
    std::optional<AutoPair> m_autoPair;

    void beginExtend();
    void deleteWithMovement(bool forward, bool word);
    void resetPreferredColumn();
    void beforeEdit();
    [[nodiscard]] Snapshot snapshot() const;
    static void pushHistory(std::vector<Snapshot>& history, std::size_t& bytes, Snapshot value);
    [[nodiscard]] static std::size_t snapshotBytes(const Snapshot& value) noexcept;
    void restore(const Snapshot& snapshot);
    [[nodiscard]] std::pair<std::size_t, std::size_t> selectedLineRange() const;
    [[nodiscard]] bool eraseSelection();
    void markEdited();
    void clampCursor();

    std::vector<std::u32string> m_lines{1};
    std::size_t m_cursorLine = 0;
    std::size_t m_cursorColumn = 0;
    std::size_t m_preferredColumn = 0;
    bool m_preferredColumnIsVisual = false;
    std::size_t m_selectionAnchorLine = 0;
    std::size_t m_selectionAnchorColumn = 0;
    std::uint64_t m_revision = 0;
    std::uint64_t m_contentState = 0;
    std::uint64_t m_nextContentState = 1;
    std::uint64_t m_savedState = 0;
    bool m_hasSavedState = true;
    bool m_selectionActive = false;
    bool m_dirty = false;
    std::vector<Snapshot> m_undo;
    std::vector<Snapshot> m_redo;
    std::size_t m_undoBytes = 0;
    std::size_t m_redoBytes = 0;
};

} // namespace lucid::editor
