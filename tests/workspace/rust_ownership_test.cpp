#include "modules/RustOwnership.hpp"
using namespace lucid::modules;
int main() {
    const std::string input = R"({"reason":"compiler-artifact"})" "\n" R"JSON({"reason":"compiler-message","message":{"code":{"code":"E0382","explanation":"first line\nsecond line"},"rendered":"error line\n  --> src/main.rs","level":"error","message":"borrow of moved value\nadditional detail","spans":[{"file_name":"src/main.rs","line_start":2,"column_start":3,"line_end":2,"column_end":4,"is_primary":true,"label":"borrowed here","text":[{"text":"  let x = thing;","highlight_start":3,"highlight_end":6}],"suggested_replacement":null,"suggestion_applicability":null}],"children":[{"level":"help","message":"consider cloning","spans":[{"file_name":"src/main.rs","line_start":1,"column_start":4,"line_end":1,"column_end":4,"is_primary":false,"label":null,"suggested_replacement":".clone()","suggestion_applicability":"MaybeIncorrect"}],"children":[]}]}})JSON";
    std::vector<RustOwnershipDiagnostic> result; std::string error;
    bool ok = parseRustOwnershipDiagnostics(input, result, error) && result.size() == 1 && result[0].code == "E0382" && result[0].spans[0].lineStart == 2 && result[0].spans[0].primary && result[0].notes[0].spans[0].hasReplacement && result[0].notes[0].spans[0].applicability == "MaybeIncorrect";
    ok = ok && result[0].explanation == "first line\nsecond line" && result[0].rendered == "error line\n  --> src/main.rs" && result[0].message == "borrow of moved value\nadditional detail" && result[0].spans[0].sourceLines[0].text == "  let x = thing;" && result[0].spans[0].sourceLines[0].highlightStart == 3 && result[0].spans[0].sourceLines[0].highlightEnd == 6;
    ok = ok && parseRustOwnershipDiagnostics(R"({"code":{"code":"E0001"},"message":"x"})", result, error) && result.empty();
    ok = ok && !parseRustOwnershipDiagnostics("{bad}", result, error) && result.empty();
    ok = ok && !parseRustOwnershipDiagnostics(R"({"code":{"code":"E0382"},"level":"error","message":"x","spans":[{"file_name":"x","line_start":0,"column_start":1,"line_end":1,"column_end":1,"is_primary":true}],"children":[]})", result, error) && result.empty();
    ok = ok && !parseRustOwnershipDiagnostics(R"({"reason":"x","reason":"y"})", result, error);
    ok = ok && !parseRustOwnershipDiagnostics(R"({"code":{"code":"E0382"},"level":"error","message":"x","spans":[{"file_name":"x","line_start":1,"column_start":1,"line_end":1,"column_end":1,"is_primary":true,"text":[{"text":"x","highlight_start":2,"highlight_end":1}]}],"children":[]})", result, error);
    ok = ok && !parseRustOwnershipDiagnostics(R"({"code":{"code":"E0382"},"level":"error","message":"x","spans":[{"file_name":"x","line_start":1,"column_start":1,"line_end":1,"column_end":1,"is_primary":true,"text":[{"text":"β","highlight_start":1,"highlight_end":3}]}],"children":[]})", result, error);
    ok = ok && parseRustOwnershipDiagnostics(R"({"code":{"code":"E0382"},"level":"error","message":"x","spans":[{"file_name":"x","line_start":1,"column_start":1,"line_end":1,"column_end":1,"is_primary":true,"text":[{"text":"β","highlight_start":1,"highlight_end":2}]}],"children":[]})", result, error);
    return ok ? 0 : 1;
}
