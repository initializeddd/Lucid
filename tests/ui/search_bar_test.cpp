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
    if (bar.query() != "hi") {
        return 7;
    }

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

    return 0;
}
