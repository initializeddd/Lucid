#include "editor/document/Document.hpp"
#include <cstdint>

namespace {
bool originalValidation(std::string_view text) {
    std::string normalized;
    int ending = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r' || text[i] == '\n') {
            int current = text[i] == '\n' ? 1 : 3;
            if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') { current = 2; ++i; }
            if (ending && ending != current) return false;
            ending = current; normalized.push_back('\n');
        } else normalized.push_back(text[i]);
    }
    return lucid::editor::Document::fromUtf8(text).toUtf8() == normalized;
}
}

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
    if (editing.line(1) != U"" || editing.cursorColumn() != 0) {
        return 15;
    }
    editing.setCursor(1, 0);
    editing.backspace();
    if (editing.lineCount() != 1 || editing.line(0) != U"    a" ||
        editing.cursorLine() != 0 || editing.cursorColumn() != 5) {
        return 16;
    }
    editing.setCursor(0, 0);
    editing.deleteForward();
    if (editing.line(0) != U"   a") {
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
    if (editing.cursorColumn() != 4) {
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

    Document largePaste = Document::fromUtf8("before\nafter");
    largePaste.setCursor(0, 3);
    std::string manyLines;
    for (int i = 0; i < 5000; ++i) manyLines += "x\n";
    largePaste.pasteUtf8(manyLines);
    if (largePaste.lineCount() != 5002 || largePaste.cursorLine() != 5000 ||
        largePaste.cursorColumn() != 0 || largePaste.lineUtf8(5000) != "ore" ||
        largePaste.lineUtf8(5001) != "after") return 137;
    largePaste.undo();
    if (largePaste.toUtf8() != "before\nafter" || largePaste.dirty()) return 138;

    Document filteredPaste = Document::fromUtf8("keep");
    filteredPaste.selectAll();
    const auto beforeFilteredPaste = filteredPaste.revision();
    filteredPaste.pasteUtf8(std::string("\x01\x7f", 2));
    if (filteredPaste.toUtf8() != "keep" || filteredPaste.selectionUtf8() != "keep" ||
        filteredPaste.dirty() || filteredPaste.revision() != beforeFilteredPaste) return 96;
    filteredPaste.pasteUtf8(std::string("\x01X\x7f", 3));
    if (filteredPaste.toUtf8() != "X") return 97;
    filteredPaste.undo();
    if (filteredPaste.toUtf8() != "keep" || filteredPaste.selectionUtf8() != "keep") return 98;

    Document samePaste = Document::fromUtf8("alpha\n\xce\xb2" "eta");
    samePaste.selectAll();
    const auto beforeSamePaste = samePaste.revision();
    samePaste.pasteUtf8("alpha\r\n\xce\xb2" "eta");
    if (samePaste.toUtf8() != "alpha\n\xce\xb2" "eta" || samePaste.hasSelection() ||
        samePaste.cursorLine() != 1 || samePaste.cursorColumn() != 4 ||
        samePaste.dirty() || samePaste.revision() != beforeSamePaste) return 105;

    Document redoAfterSamePaste = Document::fromUtf8("a");
    redoAfterSamePaste.setCursor(0, 1);
    redoAfterSamePaste.insert(U'b');
    redoAfterSamePaste.undo();
    redoAfterSamePaste.selectAll();
    redoAfterSamePaste.pasteUtf8("a");
    redoAfterSamePaste.redo();
    if (redoAfterSamePaste.toUtf8() != "ab" || !redoAfterSamePaste.dirty()) return 106;

    Document sameTyping = Document::fromUtf8("a  b");
    sameTyping.setCursor(0, 4);
    sameTyping.insert(U'!');
    sameTyping.undo();
    const auto beforeSameTyping = sameTyping.revision();
    sameTyping.setCursor(0, 1);
    sameTyping.extendSelectionTo(0, 0);
    sameTyping.insert(U'a');
    if (sameTyping.dirty() || sameTyping.hasSelection() || sameTyping.cursorColumn() != 1 ||
        sameTyping.revision() != beforeSameTyping) return 111;
    sameTyping.setCursor(0, 3);
    sameTyping.extendSelectionTo(0, 1);
    sameTyping.insertSpaces(2);
    if (sameTyping.dirty() || sameTyping.hasSelection() || sameTyping.cursorColumn() != 3 ||
        sameTyping.revision() != beforeSameTyping) return 112;
    sameTyping.redo();
    if (sameTyping.toUtf8() != "a  b!" || !sameTyping.dirty()) return 113;

    Document savedHistory = Document::fromUtf8("a");
    savedHistory.setCursor(0, 1);
    savedHistory.insert(U'b');
    savedHistory.markSaved();
    savedHistory.insert(U'c');
    savedHistory.undo();
    if (savedHistory.toUtf8() != "ab" || savedHistory.dirty()) return 99;
    savedHistory.undo();
    if (savedHistory.toUtf8() != "a" || !savedHistory.dirty()) return 100;
    savedHistory.redo();
    if (savedHistory.toUtf8() != "ab" || savedHistory.dirty()) return 101;
    savedHistory.undo();
    savedHistory.insert(U'x');
    if (savedHistory.toUtf8() != "ax" || !savedHistory.dirty()) return 102;
    savedHistory.markRecovered();
    savedHistory.undo();
    if (!savedHistory.dirty()) return 103;
    savedHistory.markSaved();
    if (savedHistory.dirty()) return 104;

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

    Document deletedBlock = Document::fromUtf8("one\ntwo\nthree\nfour");
    deletedBlock.setCursor(3, 0);
    deletedBlock.extendSelectionTo(1, 1);
    deletedBlock.deleteLine();
    if (deletedBlock.toUtf8() != "one\nfour" || deletedBlock.cursorLine() != 1 ||
        deletedBlock.cursorColumn() != 0 || deletedBlock.hasSelection()) return 107;
    deletedBlock.undo();
    if (deletedBlock.toUtf8() != "one\ntwo\nthree\nfour" ||
        deletedBlock.selectionUtf8() != "wo\nthree\n" || deletedBlock.dirty()) return 108;
    deletedBlock.selectAll();
    deletedBlock.deleteLine();
    if (deletedBlock.toUtf8() != "" || deletedBlock.lineCount() != 1 ||
        deletedBlock.cursorLine() != 0 || deletedBlock.cursorColumn() != 0) return 109;
    deletedBlock.undo();
    if (deletedBlock.toUtf8() != "one\ntwo\nthree\nfour") return 110;

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
    Document pythonComment = Document::fromUtf8("  print(1)\n\n  # existing");
    pythonComment.selectAll();
    pythonComment.toggleLineComment(U"#");
    if (pythonComment.toUtf8() != "  # print(1)\n\n  # # existing") return 190;
    pythonComment.undo();
    if (pythonComment.toUtf8() != "  print(1)\n\n  # existing") return 191;
    pythonComment.toggleLineComment(U"#");
    pythonComment.toggleLineComment(U"#");
    if (pythonComment.toUtf8() != "  print(1)\n\n  # existing") return 192;
    Document blockComment = Document::fromUtf8("one\ntwo");
    blockComment.selectAll();
    if (!blockComment.toggleBlockComment(U"/*", U"*/") || blockComment.toUtf8() != "/*one\ntwo*/" || blockComment.selectionUtf8() != "/*one\ntwo*/") return 193;
    if (!blockComment.toggleBlockComment(U"/*", U"*/") || blockComment.toUtf8() != "one\ntwo") return 194;
    blockComment.undo();
    if (blockComment.toUtf8() != "/*one\ntwo*/") return 195;
    Document unsafeBlock = Document::fromUtf8("x */ y");
    if (unsafeBlock.toggleBlockComment(U"/*", U"*/") || unsafeBlock.toUtf8() != "x */ y" || unsafeBlock.dirty()) return 196;
    Document htmlBlock = Document::fromUtf8("a -- b");
    if (htmlBlock.toggleBlockComment(U"<!--", U"-->") || htmlBlock.dirty()) return 197;
    Document separateComments = Document::fromUtf8("/* first */ code /* second */");
    separateComments.selectAll();
    if (separateComments.toggleBlockComment(U"/*", U"*/") || separateComments.dirty() ||
        separateComments.toUtf8() != "/* first */ code /* second */") return 198;
    Document reverseBlock = Document::fromUtf8("one\ntwo");
    reverseBlock.setCursor(1, 3); reverseBlock.extendSelectionTo(0, 0);
    if (!reverseBlock.toggleBlockComment(U"/*", U"*/") || reverseBlock.cursorLine() != 0 ||
        reverseBlock.cursorColumn() != 0 || reverseBlock.selectionUtf8() != "/*one\ntwo*/") return 199;
    if (!reverseBlock.toggleBlockComment(U"/*", U"*/") || reverseBlock.selectionUtf8() != "one\ntwo") return 200;
    reverseBlock.undo(); reverseBlock.undo();
    if (reverseBlock.toUtf8() != "one\ntwo" || reverseBlock.cursorLine() != 0 || reverseBlock.cursorColumn() != 0) return 201;
    Document autoQuotes = Document::fromUtf8("");
    if (!autoQuotes.insertJsonQuote() || autoQuotes.toUtf8() != "\"\"") return 202;
    autoQuotes.backspace();
    if (!autoQuotes.toUtf8().empty() || autoQuotes.cursorColumn() != 0) return 203;
    autoQuotes.undo();
    if (autoQuotes.toUtf8() != "\"\"" || autoQuotes.cursorColumn() != 1) return 204;
    autoQuotes.backspace();
    if (autoQuotes.toUtf8() != "\"") return 205;
    Document manualQuotes = Document::fromUtf8("\"\"");
    manualQuotes.setCursor(0, 1); manualQuotes.backspace();
    if (manualQuotes.toUtf8() != "\"") return 206;
    Document changedQuotes = Document::fromUtf8("");
    if (!changedQuotes.insertJsonQuote()) return 207;
    changedQuotes.insert(U'x'); changedQuotes.backspace(); changedQuotes.backspace();
    if (changedQuotes.toUtf8() != "\"") return 208;

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

    Document clicks = Document::fromUtf8("hello_world, next\nsecond");
    clicks.setCursor(0, 4);
    clicks.selectWord();
    if (clicks.selectionUtf8() != "hello_world") return 56;
    clicks.setCursor(0, 11);
    clicks.selectWord();
    if (clicks.selectionUtf8() != ",") return 57;
    clicks.setCursor(0, 3);
    clicks.selectLine();
    if (clicks.selectionUtf8() != "hello_world, next\n") return 58;
    clicks.setCursor(1, 2);
    clicks.selectLine();
    if (clicks.selectionUtf8() != "second") return 59;

    Document words = Document::fromUtf8("hello, world\nnext");
    words.setCursor(0, 0);
    words.moveWordRight();
    if (words.cursorColumn() != 5) return 60;
    words.moveWordRight();
    if (words.cursorColumn() != 7) return 61;
    words.moveWordRight(true);
    if (words.selectionUtf8() != "world") return 62;
    words.moveWordLeft();
    if (words.cursorColumn() != 7) return 63;

    Document collapse = Document::fromUtf8("first\nsecond");
    collapse.setCursor(1, 3); collapse.extendSelectionTo(0, 2); collapse.moveRight();
    if (collapse.hasSelection() || collapse.cursorLine() != 1 || collapse.cursorColumn() != 3) return 64;
    collapse.setCursor(0, 2); collapse.extendSelectionTo(1, 3); collapse.moveLeft();
    if (collapse.hasSelection() || collapse.cursorLine() != 0 || collapse.cursorColumn() != 2) return 65;

    Document boundaryHistory = Document::fromUtf8("a");
    boundaryHistory.setCursor(0, 1); boundaryHistory.insert(U'b'); boundaryHistory.undo();
    boundaryHistory.setCursor(0, 0); boundaryHistory.backspace(); boundaryHistory.outdentLines(); boundaryHistory.redo();
    if (boundaryHistory.toUtf8() != "ab") return 66;
    boundaryHistory.undo(); boundaryHistory.setCursor(0, 1); boundaryHistory.deleteForward(); boundaryHistory.redo();
    if (boundaryHistory.toUtf8() != "ab") return 67;

    Document block = Document::fromUtf8("a\nb\nc\nd");
    block.setCursor(1, 0); block.extendSelectionTo(3, 0); block.moveLineUp();
    if (block.toUtf8() != "b\nc\na\nd" || block.selectionUtf8() != "b\nc\n") return 68;
    block.moveLineDown();
    if (block.toUtf8() != "a\nb\nc\nd" || block.selectionUtf8() != "b\nc\n") return 69;
    block.duplicateLine();
    if (block.toUtf8() != "a\nb\nc\nb\nc\nd" || block.selectionUtf8() != "b\nc\n") return 70;
    block.undo();
    if (block.toUtf8() != "a\nb\nc\nd" || block.selectionUtf8() != "b\nc\n") return 71;

    Document last = Document::fromUtf8("a\nb\nc");
    last.setCursor(1, 0); last.extendSelectionTo(2, 0); last.moveLineDown();
    if (last.toUtf8() != "a\nc\nb" || last.selectionUtf8() != "b") return 72;

    Document smart = Document::fromUtf8("      value");
    smart.setCursor(0, 11); smart.moveSmartHome();
    if (smart.cursorColumn() != 6) return 73;
    smart.moveSmartHome();
    if (smart.cursorColumn() != 0) return 74;
    smart.setCursor(0, 6); smart.moveSmartHome(true);
    if (smart.selectionUtf8() != "      ") return 75;

    Document vertical = Document::fromUtf8("\tx\n    x\nx\n\tx");
    vertical.setCursor(0, 1);
    vertical.moveDown();
    if (vertical.cursorLine() != 1 || vertical.cursorColumn() != 4) return 90;
    vertical.moveDown();
    if (vertical.cursorLine() != 2 || vertical.cursorColumn() != 1) return 91;
    vertical.moveDown();
    if (vertical.cursorLine() != 3 || vertical.cursorColumn() != 1) return 92;
    vertical.moveUp(true);
    if (vertical.cursorLine() != 2 || vertical.cursorColumn() != 1 || !vertical.hasSelection()) return 93;
    vertical.moveUp(true);
    if (vertical.cursorLine() != 1 || vertical.cursorColumn() != 4) return 94;
    vertical.moveLeft();
    vertical.moveUp();
    if (vertical.cursorLine() != 0 || vertical.cursorColumn() != 1) return 95;

    std::string formatError, formatted;
    const std::string windowsText = "\xef\xbb\xbf" "one\r\ntwo\r\n";
    if (!Document::canEditLosslessly(windowsText, formatError)) return 76;
    auto windowsDoc = Document::fromUtf8(windowsText);
    windowsDoc.setCursor(1, 3); windowsDoc.insert(U'!');
    if (!windowsDoc.serializeForFile(windowsText, formatted, formatError) ||
        formatted != "\xef\xbb\xbf" "one\r\ntwo!\r\n") return 77;
    const std::string oldMac = "one\rtwo\r";
    auto oldMacDoc = Document::fromUtf8(oldMac);
    if (!oldMacDoc.serializeForFile(oldMac, formatted, formatError) || formatted != oldMac) return 78;
    if (Document::canEditLosslessly("one\r\ntwo\n", formatError) || formatError.empty()) return 79;
    if (Document::canEditLosslessly(std::string("bad\xff", 4), formatError) || formatError.empty()) return 80;

    auto joined = Document::fromUtf8("first\n  second\n\tthird\nlast");
    joined.setCursor(0, 2); joined.joinLines();
    if (joined.toUtf8() != "first second\n\tthird\nlast" || joined.cursorColumn() != 5) return 96;
    joined.undo();
    if (joined.toUtf8() != "first\n  second\n\tthird\nlast" || joined.cursorColumn() != 2) return 97;
    joined.setCursor(0, 0); joined.extendSelectionTo(2, 3); joined.joinLines();
    if (joined.toUtf8() != "first second third\nlast") return 98;
    joined.undo();
    if (joined.toUtf8() != "first\n  second\n\tthird\nlast" || !joined.hasSelection()) return 99;

    auto wordDeletion = Document::fromUtf8("one two\nthree");
    wordDeletion.setCursor(0, 7); wordDeletion.deleteWord(false);
    if (wordDeletion.toUtf8() != "one \nthree") return 100;
    wordDeletion.undo();
    if (wordDeletion.toUtf8() != "one two\nthree" || wordDeletion.cursorColumn() != 7 || wordDeletion.hasSelection()) return 101;
    wordDeletion.redo();
    if (wordDeletion.toUtf8() != "one \nthree") return 102;
    wordDeletion.setCursor(0, 0); wordDeletion.deleteWord(false); wordDeletion.undo();
    if (wordDeletion.toUtf8() != "one two\nthree") return 103;
    wordDeletion.setCursor(1, 3); wordDeletion.deleteToLineBoundary(false); wordDeletion.undo();
    if (wordDeletion.toUtf8() != "one two\nthree" || wordDeletion.cursorColumn() != 3 || wordDeletion.hasSelection()) return 104;

    auto formattedDocument = Document::fromUtf8("first\nsecond");
    formattedDocument.setCursor(0, 2); formattedDocument.extendSelectionTo(1, 3);
    if (!formattedDocument.replaceAllUtf8("first!\nsecond!")) return 105;
    if (formattedDocument.cursorLine() != 1 || formattedDocument.cursorColumn() != 3 || formattedDocument.hasSelection()) return 106;
    formattedDocument.undo();
    if (formattedDocument.toUtf8() != "first\nsecond" || !formattedDocument.hasSelection() ||
        formattedDocument.selectionStart().column != 2 || formattedDocument.selectionEnd().column != 3) return 107;
    if (formattedDocument.replaceAllUtf8(std::string("bad\xff", 4)) || formattedDocument.toUtf8() != "first\nsecond") return 108;

    auto surrounded = Document::fromUtf8("hello\nworld");
    surrounded.setCursor(1, 3); surrounded.extendSelectionTo(0, 2);
    if (!surrounded.surroundSelection(U'(', U')') || surrounded.toUtf8() != "he(llo\nwor)ld" ||
        surrounded.selectionUtf8() != "llo\nwor" || surrounded.cursorLine() != 0 || surrounded.cursorColumn() != 3) return 109;
    surrounded.undo();
    if (surrounded.toUtf8() != "hello\nworld" || surrounded.selectionUtf8() != "llo\nwor" || surrounded.cursorColumn() != 2) return 110;
    surrounded.redo();
    if (surrounded.toUtf8() != "he(llo\nwor)ld" || surrounded.selectionUtf8() != "llo\nwor") return 111;

    auto smartBackspace = Document::fromUtf8("      value\n\t   item\ntext  ");
    smartBackspace.setCursor(0, 6); smartBackspace.backspace();
    if (smartBackspace.lineUtf8(0) != "    value" || smartBackspace.cursorColumn() != 4) return 112;
    smartBackspace.undo();
    if (smartBackspace.lineUtf8(0) != "      value" || smartBackspace.cursorColumn() != 6) return 113;
    smartBackspace.setCursor(1, 4); smartBackspace.backspace();
    if (smartBackspace.lineUtf8(1) != "\titem") return 114;
    smartBackspace.setCursor(2, 6); smartBackspace.backspace();
    if (smartBackspace.lineUtf8(2) != "text ") return 115;
    auto boundaries = Document::fromUtf8("first\nsecond\nlast");
    boundaries.setCursor(1, 3); boundaries.moveDocumentStart(true);
    if (boundaries.selectionUtf8() != "first\nsec" || boundaries.cursorLine() != 0) return 116;
    boundaries.moveDocumentEnd(true);
    if (boundaries.selectionUtf8() != "ond\nlast" || boundaries.cursorLine() != 2 || boundaries.cursorColumn() != 4) return 117;
    boundaries.moveDocumentStart();
    if (boundaries.hasSelection() || boundaries.cursorLine() != 0 || boundaries.cursorColumn() != 0) return 118;
    auto paragraphs = Document::fromUtf8("first\ncontinued\n\nsecond\nlast");
    paragraphs.setCursor(1, 3); paragraphs.moveParagraphUp();
    if (paragraphs.cursorLine() != 0 || paragraphs.cursorColumn() != 0) return 119;
    paragraphs.moveParagraphDown(true);
    if (paragraphs.cursorLine() != 3 || paragraphs.selectionUtf8() != "first\ncontinued\n\n") return 120;
    paragraphs.moveParagraphDown();
    if (paragraphs.cursorLine() != 4 || paragraphs.cursorColumn() != 4 || paragraphs.hasSelection()) return 121;
    paragraphs.moveParagraphUp();
    if (paragraphs.cursorLine() != 3 || paragraphs.cursorColumn() != 0) return 122;
    auto largeHistory = Document::fromUtf8(std::string(1024U * 1024U, 'a'));
    for (int i = 0; i < 20; ++i) largeHistory.insert(U'x');
    int undoCount = 0;
    for (;;) {
        const auto revision = largeHistory.revision(); largeHistory.undo();
        if (revision == largeHistory.revision()) break;
        ++undoCount;
    }
    if (undoCount < 1 || undoCount >= 20) return 123;
    const auto beforeRedo = largeHistory.toUtf8(); largeHistory.redo();
    if (largeHistory.toUtf8() == beforeRedo) return 124;

    // Compare direct validation with the editor's actual decode/encode behavior,
    // including malformed UTF-8 and all pairs of byte values.
    std::string validationError;
    for (unsigned first = 0; first < 256; ++first) for (unsigned second = 0; second < 256; ++second) {
        const std::string sample{static_cast<char>(first), static_cast<char>(second)};
        if (Document::canEditLosslessly(sample, validationError) != originalValidation(sample)) return 125;
    }
    std::uint32_t randomState = 0x91d3a5c7U;
    for (int sampleIndex = 0; sampleIndex < 10000; ++sampleIndex) {
        std::string sample;
        for (int i = 0; i < 32; ++i) {
            randomState = randomState * 1664525U + 1013904223U;
            sample.push_back(static_cast<char>(randomState >> 24));
        }
        if (Document::canEditLosslessly(sample, validationError) != originalValidation(sample)) return 126;
    }

    return 0;
}
