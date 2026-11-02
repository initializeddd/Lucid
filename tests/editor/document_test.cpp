#include "editor/document/Document.hpp"

int main() {
    using lucid::editor::Document;

    const Document document = Document::fromUtf8("hello\nworld\n");
    if (document.lineCount() != 3) {
        return 1;
    }
    if (document.line(0) != U"hello" || document.line(1) != U"world" ||
        document.line(2) != U"") {
        return 2;
    }
    if (document.toUtf8() != "hello\nworld\n") {
        return 3;
    }

    const Document crlf = Document::fromUtf8("a\r\nb\rc\n");
    if (crlf.lineCount() != 4 || crlf.line(0) != U"a" || crlf.line(1) != U"b" ||
        crlf.line(2) != U"c" || crlf.line(3) != U"") {
        return 4;
    }
    if (crlf.toUtf8() != "a\nb\nc\n") {
        return 5;
    }

    Document unicode = Document::fromUtf8("héllo 😀");
    if (unicode.toUtf8() != "héllo 😀") {
        return 6;
    }
    unicode.setCursor(0, 2);
    unicode.insert(U'!');
    if (unicode.line(0) != U"hé!llo 😀") {
        return 7;
    }
    if (!unicode.dirty()) {
        return 8;
    }
    unicode.markSaved();
    if (unicode.dirty()) {
        return 9;
    }

    Document editing = Document::fromUtf8("");
    if (editing.lineCount() != 1) {
        return 10;
    }
    editing.insertSpaces(4);
    editing.insert(U'a');
    if (editing.line(0) != U"    a") {
        return 11;
    }
    editing.newline();
    if (editing.lineCount() != 2 || editing.line(1) != U"    " ||
        editing.cursorLine() != 1 || editing.cursorColumn() != 4) {
        return 12;
    }
    editing.insert(U'b');
    if (editing.line(1) != U"    b") {
        return 13;
    }
    editing.backspace();
    if (editing.line(1) != U"    " || editing.cursorColumn() != 4) {
        return 14;
    }
    editing.backspace();
    if (editing.line(1) != U"   " || editing.cursorColumn() != 3) {
        return 15;
    }
    editing.setCursor(1, 0);
    editing.backspace();
    if (editing.lineCount() != 1 || editing.line(0) != U"    a   " ||
        editing.cursorLine() != 0 || editing.cursorColumn() != 5) {
        return 16;
    }
    editing.setCursor(0, 0);
    editing.deleteForward();
    if (editing.line(0) != U"   a   ") {
        return 17;
    }

    editing.setCursor(0, 3);
    editing.moveLeft();
    if (editing.cursorColumn() != 2) {
        return 18;
    }
    editing.moveRight();
    if (editing.cursorColumn() != 3) {
        return 19;
    }
    editing.moveHome();
    if (editing.cursorColumn() != 0) {
        return 20;
    }
    editing.moveEnd();
    if (editing.cursorColumn() != 7) {
        return 21;
    }

    Document lines = Document::fromUtf8("longer\nx\nthird");
    lines.setCursor(0, 5);
    lines.moveDown();
    if (lines.cursorLine() != 1 || lines.cursorColumn() != 1) {
        return 22;
    }
    lines.moveDown();
    if (lines.cursorLine() != 2 || lines.cursorColumn() != 5) {
        return 23;
    }
    lines.moveUp();
    if (lines.cursorLine() != 1 || lines.cursorColumn() != 1) {
        return 24;
    }

    Document control = Document::fromUtf8("a");
    control.insert(U'\n');
    control.insert(0xD800);
    if (control.lineCount() != 1 || control.toUtf8() != "a") {
        return 25;
    }

    Document revisioned = Document::fromUtf8("a");
    const std::uint64_t initialRevision = revisioned.revision();
    revisioned.insert(U'b');
    if (revisioned.revision() == initialRevision) {
        return 26;
    }
    const std::uint64_t editedRevision = revisioned.revision();
    revisioned.markSaved();
    if (revisioned.revision() != editedRevision) {
        return 27;
    }

    Document selection = Document::fromUtf8("hello\nworld");
    selection.setCursor(0, 2);
    selection.moveRight(true);
    selection.moveRight(true);
    selection.moveDown(true);
    if (!selection.hasSelection()) {
        return 28;
    }
    const auto selectionStart = selection.selectionStart();
    const auto selectionEnd = selection.selectionEnd();
    if (selectionStart.line != 0 || selectionStart.column != 2 ||
        selectionEnd.line != 1 || selectionEnd.column != 4) {
        return 29;
    }
    if (selection.selectionUtf8() != "llo\nworl") {
        return 30;
    }
    selection.moveUp(true);
    selection.moveLeft(true);
    if (!selection.hasSelection() || selection.selectionUtf8() != "l") {
        return 31;
    }
    selection.selectAll();
    if (selection.selectionUtf8() != "hello\nworld") {
        return 32;
    }
    selection.insert(U'!');
    if (selection.toUtf8() != "!" || selection.hasSelection()) {
        return 33;
    }

    Document replaced = Document::fromUtf8("abc\ndef");
    replaced.setCursor(0, 1);
    replaced.extendSelectionTo(1, 2);
    replaced.insert(U'X');
    if (replaced.toUtf8() != "aXf" || replaced.cursorLine() != 0 ||
        replaced.cursorColumn() != 2) {
        return 34;
    }

    Document removed = Document::fromUtf8("abc\ndef");
    removed.setCursor(0, 1);
    removed.extendSelectionTo(1, 2);
    removed.backspace();
    if (removed.toUtf8() != "af") {
        return 35;
    }

    Document duplicated = Document::fromUtf8("one\ntwo");
    duplicated.setCursor(0, 1);
    duplicated.duplicateLine();
    if (duplicated.toUtf8() != "one\none\ntwo" ||
        duplicated.cursorLine() != 1 || duplicated.cursorColumn() != 1) {
        return 36;
    }

    Document pasted = Document::fromUtf8("ab");
    pasted.setCursor(0, 1);
    pasted.pasteUtf8("X\nY\r\nZ");
    if (pasted.toUtf8() != "aX\nY\nZb" || pasted.cursorLine() != 2 ||
        pasted.cursorColumn() != 1) {
        return 37;
    }

    Document trailingPaste = Document::fromUtf8("ab");
    trailingPaste.setCursor(0, 1);
    trailingPaste.pasteUtf8("X\n");
    if (trailingPaste.toUtf8() != "aX\nb" || trailingPaste.cursorLine() != 1 ||
        trailingPaste.cursorColumn() != 0) {
        return 38;
    }

    Document multiline = Document::fromUtf8("héllo\n😀");
    multiline.setCursor(0, 1);
    multiline.extendSelectionTo(1, 0);
    if (multiline.selectionUtf8() != "éllo\n" || multiline.lineUtf8(1) != "😀") {
        return 39;
    }

    Document reversed = Document::fromUtf8("hello");
    reversed.setCursor(0, 5);
    reversed.extendSelectionTo(0, 1);
    const auto reversedStart = reversed.selectionStart();
    const auto reversedEnd = reversed.selectionEnd();
    if (reversedStart.column != 1 || reversedEnd.column != 5 ||
        reversed.selectionUtf8() != "ello") {
        return 40;
    }

    Document deletedLine = Document::fromUtf8("one\ntwo\nthree");
    deletedLine.setCursor(1, 2);
    deletedLine.deleteLine();
    if (deletedLine.toUtf8() != "one\nthree" || deletedLine.cursorLine() != 1 ||
        deletedLine.cursorColumn() != 0) {
        return 41;
    }

    Document below = Document::fromUtf8("    first");
    below.setCursor(0, 5);
    below.insertLineBelow();
    if (below.toUtf8() != "    first\n    " || below.cursorLine() != 1 ||
        below.cursorColumn() != 4) {
        return 42;
    }

    Document above = Document::fromUtf8("    second");
    above.setCursor(0, 6);
    above.insertLineAbove();
    if (above.toUtf8() != "    \n    second" || above.cursorLine() != 0 ||
        above.cursorColumn() != 4) {
        return 43;
    }

    Document comment = Document::fromUtf8("    value();");
    comment.setCursor(0, 6);
    comment.toggleLineComment();
    if (comment.toUtf8() != "    // value();" || comment.cursorColumn() != 9) {
        return 44;
    }
    comment.toggleLineComment();
    if (comment.toUtf8() != "    value();" || comment.cursorColumn() != 6) {
        return 45;
    }

    Document commentSelection = Document::fromUtf8("one\ntwo");
    commentSelection.selectAll();
    commentSelection.toggleLineComment();
    if (commentSelection.toUtf8() != "// one\n// two") {
        return 46;
    }

    Document indented = Document::fromUtf8("one\ntwo");
    indented.selectAll();
    indented.indentLines();
    if (indented.toUtf8() != "    one\n    two" ||
        indented.selectionUtf8() != "one\n    two") {
        return 47;
    }
    indented.outdentLines();
    if (indented.toUtf8() != "one\ntwo") {
        return 48;
    }

    Document moved = Document::fromUtf8("one\ntwo\nthree");
    moved.setCursor(1, 1);
    moved.moveLineUp();
    if (moved.toUtf8() != "two\none\nthree" || moved.cursorLine() != 0) {
        return 49;
    }
    moved.moveLineDown();
    moved.moveLineDown();
    if (moved.toUtf8() != "one\nthree\ntwo" || moved.cursorLine() != 2) {
        return 50;
    }

    Document history = Document::fromUtf8("a");
    history.setCursor(0, 1);
    history.insert(U'b');
    history.insert(U'c');
    history.undo();
    if (history.toUtf8() != "ab" || history.cursorColumn() != 2) {
        return 51;
    }
    history.undo();
    if (history.toUtf8() != "a" || history.cursorColumn() != 1) {
        return 52;
    }
    history.redo();
    if (history.toUtf8() != "ab" || history.cursorColumn() != 2) {
        return 53;
    }

    Document blankIndent = Document::fromUtf8("    \nnext");
    blankIndent.setCursor(0, 4);
    blankIndent.insertLineBelow();
    if (blankIndent.toUtf8() != "    \n    \nnext" ||
        blankIndent.cursorLine() != 1 || blankIndent.cursorColumn() != 4) {
        return 55;
    }

    Document cut = Document::fromUtf8("one\ntwo");
    cut.setCursor(0, 1);
    cut.extendSelectionTo(0, 2);
    cut.deleteSelection();
    if (cut.toUtf8() != "oe\ntwo" || cut.cursorColumn() != 1) {
        return 54;
    }

    return 0;
}
