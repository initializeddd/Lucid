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
    RustOwnershipSpan fix; fix.file="src/main.rs"; fix.lineStart=1; fix.lineEnd=1; fix.columnStart=2; fix.columnEnd=3;
    fix.hasReplacement=true; fix.applicability="MachineApplicable"; fix.replacement="a"; fix.sourceLines.push_back({"xβy",2,3});
    auto document=lucid::editor::Document::fromUtf8("xβy\nsecond"); RustSuggestionEdit edit;
    ok = ok && prepareRustSuggestion(fix,document,edit,error) && edit.start.line==0 && edit.start.column==1 && edit.end.column==2 && edit.original=="β" && edit.replacement=="a";
    auto stale=lucid::editor::Document::fromUtf8("xγy\nsecond"); ok=ok && !prepareRustSuggestion(fix,stale,edit,error);
    fix.applicability="MaybeIncorrect"; ok=ok && !prepareRustSuggestion(fix,document,edit,error);
    fix.applicability="MachineApplicable"; fix.sourceLines.clear(); ok=ok && !prepareRustSuggestion(fix,document,edit,error);
    fix.sourceLines.push_back({"xβy",2,3}); fix.columnEnd=20; ok=ok && !prepareRustSuggestion(fix,document,edit,error);
    fix.columnStart=2; fix.columnEnd=2; fix.replacement=".clone()"; ok=ok && prepareRustSuggestion(fix,document,edit,error) && edit.start.column==edit.end.column;
    fix.lineEnd=2; fix.columnEnd=3; fix.sourceLines.push_back({"second",1,3}); ok=ok && prepareRustSuggestion(fix,document,edit,error) && edit.end.line==1 && edit.original=="βy\nse";
    fix.replacement=std::string("\xff",1); ok=ok && !prepareRustSuggestion(fix,document,edit,error);
    return ok ? 0 : 1;
}
