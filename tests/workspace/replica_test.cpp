#include "workspace/collaboration/Replica.hpp"
using namespace lucid::workspace::collaboration;
int main() {
    Replica alice, bob; std::string error; Edit a, b, applied;
    if (!alice.install(0, "hello world", error) || !bob.install(0, "hello world", error)) return 1;
    if (!alice.setLocal("Hi hello world", error) || !bob.setLocal("hello Lucid", error) ||
        !alice.nextEdit("a1", a, error) || !bob.nextEdit("b1", b, error)) return 2;
    SharedDocument host("hello world"); if (!host.join("a", "Alice") || !host.join("b", "Bob")) return 3;
    if (!host.applyRebased("a", a, applied, error) || !alice.receive(1, "a", "a", "a1", applied, error) || !bob.receive(1, "a", "b", "a1", applied, error)) return 4;
    if (bob.localText() != "Hi hello Lucid" || !host.applyRebased("b", b, applied, error) ||
        !alice.receive(2, "b", "a", "b1", applied, error) || !bob.receive(2, "b", "b", "b1", applied, error)) return 5;
    if (alice.localText() != host.text() || bob.localText() != host.text() || bob.inFlight()) return 6;
    Replica conflict; if (!conflict.install(0, "abc", error) || !conflict.setLocal("aXc", error) || !conflict.nextEdit("c1", b, error)) return 7;
    if (conflict.receive(1, "other", "self", "other1", {0, 1, 1, "Y"}, error) || !conflict.conflicted() || conflict.localText() != "aXc") return 8;
    if (conflict.install(1, "aYc", error) || conflict.localText() != "aXc") return 9;
    Replica unicode; if (!unicode.install(0, "aéz", error) || !unicode.setLocal("aêz", error) || !unicode.nextEdit("u1", a, error) || a.start != 1 || a.eraseBytes != 2) return 10;
    return 0;
}
