#include "ui/search/SearchBar.hpp"

int main() {
    using lucid::ui::SearchBar;

    SearchBar bar;
    if (bar.visible()) {
        return 1;
    }

    bar.append(U'a');
    if (!bar.query().empty()) {
        return 2;
    }

    bar.open();
    if (!bar.visible() || !bar.query().empty()) {
        return 3;
    }

    double time = 0.0;
    auto state = bar.frameState(time);
    if (!state.commandVisible || state.opacity > 0.01F || state.offsetX >= 0.0F ||
        state.scaleX >= 1.0F || state.blur <= 0.0F) {
        return 4;
    }

    for (int frame = 0; frame < 60; ++frame) {
        time += 1.0 / 60.0;
        state = bar.frameState(time);
    }
    if (!state.commandVisible || state.opacity < 0.99F || state.blur > 0.01F) {
        return 5;
    }

    bar.append(U'h');
    bar.append(U'i');
    if (bar.query() != "hi") {
        return 6;
    }

    bar.append(U'\n');
    bar.append(U'\u263A');
    if (bar.query() != "hi\xE2\x98\xBA") {
        return 7;
    }
    bar.backspace();
    if (bar.query() != "hi") return 16;

    bar.backspace();
    if (bar.query() != "h") {
        return 8;
    }

    bar.backspace();
    bar.backspace();
    if (!bar.query().empty()) {
        return 9;
    }

    bar.append(U'o');
    bar.append(U'k');
    bar.close();
    if (!bar.visible() || bar.query() != "ok") {
        return 10;
    }

    for (int frame = 0; frame < 120; ++frame) {
        time += 1.0 / 60.0;
        state = bar.frameState(time);
    }
    if (bar.visible() || state.commandVisible || state.opacity > 0.01F ||
        !bar.query().empty() || state.timeSeconds != time) {
        return 11;
    }

    bar.toggle();
    if (!bar.visible()) {
        return 12;
    }

    bar.toggle();
    if (!bar.visible()) {
        return 13;
    }

    for (int frame = 0; frame < 120; ++frame) {
        time += 1.0 / 60.0;
        state = bar.frameState(time);
    }
    if (bar.visible() || state.commandVisible) {
        return 14;
    }

    bar.open();
    bar.append(U'z');
    for (int frame = 0; frame < 5; ++frame) {
        time += 1.0 / 60.0;
        state = bar.frameState(time);
    }
    bar.close();
    time += 1.0 / 60.0;
    state = bar.frameState(time);
    if (!bar.visible() || bar.query() != "z") {
        return 15;
    }

    bar.open();
    if (!bar.query().empty()) {
        return 16;
    }

    for (std::size_t index = 0; index < SearchBar::kMaxQueryLength + 50; ++index) {
        bar.append(U'x');
    }
    if (bar.query().size() != SearchBar::kMaxQueryLength) {
        return 17;
    }

    bar.setQuery("path/");
    if (bar.appendUtf8("Grüße\tfile\nignored")) return 30;
    if (bar.query() != "path/Grüße file") return 18;
    bar.backspace();
    if (bar.query() != "path/Grüße fil") return 19;
    bar.setQuery(std::string(SearchBar::kMaxQueryLength - 1, 'x'));
    if (bar.appendUtf8("üa")) return 31;
    if (bar.query().size() != SearchBar::kMaxQueryLength - 1) return 20;
    bar.setQuery("");
    bar.appendUtf8(std::string("a\xff" "b", 3));
    if (bar.query() != "a\xef\xbf\xbd" "b") return 21;

    bar.setQuery("Grüße");
    bar.moveLeft();
    bar.moveLeft(true);
    if (bar.query().substr(bar.selectionStart(), bar.selectionEnd() - bar.selectionStart()) != "ß") return 22;
    bar.append(U's');
    if (bar.query() != "Grüse") return 23;
    bar.deleteForward();
    if (bar.query() != "Grüs") return 24;
    bar.moveHome();
    bar.deleteForward();
    if (bar.query() != "rüs") return 25;
    bar.selectAll();
    bar.appendUtf8("rüste");
    if (bar.query() != "rüste" || bar.hasSelection()) return 26;
    bar.moveLeft();
    bar.backspace();
    if (bar.query() != "rüse") return 27;

    bar.setQuery("src/Grüße.cpp");
    bar.moveWordLeft();
    if (bar.query().substr(bar.cursor()) != "cpp") return 28;
    bar.moveWordLeft(true);
    if (bar.query().substr(bar.selectionStart(), bar.selectionEnd() - bar.selectionStart()) != ".") return 29;
    bar.moveWordLeft();
    bar.moveWordLeft();
    if (bar.query().substr(bar.cursor(), 7) != "Grüße") return 30;
    bar.setCursor(7); // A byte inside ü is clamped to its boundary.
    if (bar.cursor() != 6) return 31;
    bar.moveWordRight(true);
    if (!bar.hasSelection()) return 32;

    bar.close();
    if (!bar.visible() || bar.isOpen()) return 33;
    bar.toggle();
    if (!bar.isOpen() || !bar.visible() || !bar.query().empty()) return 34;

    return 0;
}
