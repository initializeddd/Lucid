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
    if (alice.install(1, "older text", error) || alice.localText() != host.text() || alice.revision() != 2 || error.empty()) return 11;
    if (alice.install(2, "different text", error) || alice.localText() != host.text() || alice.revision() != 2 || error.empty()) return 12;
    if (!alice.install(2, host.text(), error)) return 13;
    Replica conflict; if (!conflict.install(0, "abc", error) || !conflict.setLocal("aXc", error) || !conflict.nextEdit("c1", b, error)) return 7;
    if (conflict.receive(1, "other", "self", "other1", {0, 1, 1, "Y"}, error) || !conflict.conflicted() || conflict.localText() != "aXc") return 8;
    if (conflict.install(1, "aYc", error) || conflict.localText() != "aXc") return 9;
    Replica unicode; if (!unicode.install(0, "aéz", error) || !unicode.setLocal("aêz", error) || !unicode.nextEdit("u1", a, error) || a.start != 1 || a.eraseBytes != 2) return 10;
    Replica simultaneous;
    if (!simultaneous.install(0, "x", error) || !simultaneous.setLocal("Ax", error) || !simultaneous.nextEdit("mine", a, error)) return 14;
    SharedDocument insertHost("x");
    if (!insertHost.join("remote", "Remote") || !insertHost.join("self", "Self") ||
        !insertHost.applyRebased("remote", {0, 0, 0, "B"}, applied, error) ||
        !simultaneous.receive(1, "remote", "self", "theirs", applied, error) || simultaneous.localText() != "BAx" ||
        !insertHost.applyRebased("self", a, applied, error) ||
        !simultaneous.receive(2, "self", "self", "mine", applied, error) || simultaneous.localText() != insertHost.text()) return 15;
    Replica wrongAck;
    if (!wrongAck.install(0, "abc", error) || !wrongAck.setLocal("aXbc", error) || !wrongAck.nextEdit("mine", a, error) ||
        wrongAck.receive(1, "self", "self", "mine", {0, 1, 0, "Y"}, error) ||
        !wrongAck.conflicted() || wrongAck.localText() != "aXbc" || wrongAck.revision() != 0) return 16;
    // Exercise insertions at every pair of positions, including both clients
    // inserting at the same position, in the order accepted by the host.
    for (size_t local = 0; local <= 4; ++local) {
        for (size_t remote = 0; remote <= 4; ++remote) {
            Replica replica;
            SharedDocument server("abcd");
            std::string text = "abcd";
            text.insert(local, "L");
            if (!server.join("self", "Self") || !server.join("remote", "Remote") ||
                !replica.install(0, "abcd", error) || !replica.setLocal(text, error) ||
                !replica.nextEdit("local", a, error) ||
                !server.applyRebased("remote", {0, remote, 0, "R"}, applied, error) ||
                !replica.receive(1, "remote", "self", "remote", applied, error) ||
                !server.applyRebased("self", a, applied, error) ||
                !replica.receive(2, "self", "self", "local", applied, error) ||
                replica.localText() != server.text()) return 17;
        }
    }
    // Typing while an edit is in flight must survive its acknowledgement and
    // be sent as a separate edit afterwards.
    Replica typing;
    SharedDocument typingHost("abc");
    if (!typingHost.join("self", "Self") || !typing.install(0, "abc", error) ||
        !typing.setLocal("aXbc", error) || !typing.nextEdit("first", a, error) ||
        !typing.setLocal("aXbc!", error) || !typingHost.applyRebased("self", a, applied, error) ||
        !typing.receive(1, "self", "self", "first", applied, error) || typing.localText() != "aXbc!" ||
        !typing.nextEdit("second", a, error) || !typingHost.applyRebased("self", a, applied, error) ||
        !typing.receive(2, "self", "self", "second", applied, error) || typing.localText() != typingHost.text()) return 18;
    return 0;
}
