#include "workspace/collaboration/SharedDocument.hpp"
using namespace lucid::workspace::collaboration;
int main() {
    SharedDocument doc("aéz"); std::string error;
    if (!doc.valid() || !doc.join("alice", "Alice") || !doc.join("bob", "Bob") || doc.join("alice", "Other")) return 1;
    if (!doc.moveCursor("bob", {3, 4}) || doc.moveCursor("bob", {2, 3})) return 2;
    if (!doc.apply("alice", {0, 1, 2, "🙂"}, error)) return 3;
    if (doc.text() != "a🙂z" || doc.revision() != 1 || doc.participants().at("bob").cursor.anchor != 5 || doc.participants().at("bob").cursor.caret != 6) return 4;
    if (doc.apply("bob", {0, 0, 1, ""}, error) || error.empty() || doc.text() != "a🙂z") return 5;
    if (doc.apply("bob", {1, 2, 1, ""}, error) || doc.apply("bob", {1, 0, 0, "\xff"}, error)) return 6;
    if (!doc.apply("bob", {1, 0, 1, ""}, error) || doc.history().size() != 2 || doc.history().back().participantName != "Bob") return 7;
    doc.leave("bob"); if (doc.moveCursor("bob", {0, 0}) || doc.apply("bob", {2, 0, 0, "x"}, error)) return 8;
    SharedDocument invalid("\xff"); if (invalid.valid() || invalid.join("x", "X")) return 9;
    SharedDocument concurrent("hello world");
    if (!concurrent.join("a", "Alice") || !concurrent.join("b", "Bob")) return 10;
    Edit applied;
    if (!concurrent.apply("a", {0, 0, 0, "Hi "}, error) ||
        !concurrent.applyRebased("b", {0, 6, 5, "Lucid"}, applied, error) ||
        concurrent.text() != "Hi hello Lucid" || applied.start != 9) return 11;
    SharedDocument overlap("abcdef");
    if (!overlap.join("a", "Alice") || !overlap.join("b", "Bob") ||
        !overlap.apply("a", {0, 2, 2, "XY"}, error)) return 12;
    if (overlap.applyRebased("b", {0, 3, 2, "Z"}, applied, error) || overlap.text() != "abXYef" || error.empty()) return 13;
    SharedDocument inserts("ab");
    if (!inserts.join("a", "Alice") || !inserts.join("b", "Bob") ||
        !inserts.apply("a", {0, 1, 0, "X"}, error) ||
        !inserts.applyRebased("b", {0, 1, 0, "Y"}, applied, error) || inserts.text() != "aXYb") return 14;
    SharedDocument bounded(""); if (!bounded.join("a", "Alice") || !bounded.join("b", "Bob")) return 15;
    for (int i = 0; i < 1025; ++i) if (!bounded.apply("a", {bounded.revision(), 0, 0, "x"}, error)) return 16;
    if (bounded.applyRebased("b", {0, 0, 0, "y"}, applied, error) || error.empty() || bounded.text().size() != 1025) return 17;
    return 0;
}
