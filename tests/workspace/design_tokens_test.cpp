#include "modules/DesignTokens.hpp"
#include <string>
using namespace lucid::modules;
int main() {
    DesignTokenReport report; std::string error;
    if (!extractJsonDesignTokens(R"({"color":{"brand":{"$value":"#123456","$type":"color"}},"space":{"small":4},"active":true,"complex":{"$value":{"a":1}},"list":[1,2]})", report, error)) return 1;
    if (report.tokens.size() != 3 || report.tokens[0].name != "color.brand" || report.tokens[0].value != "#123456" || report.tokens[0].type != "color" || report.warnings.size() != 2) return 2;
    if (extractJsonDesignTokens("{", report, error) || !report.tokens.empty() || error.empty()) return 3;
    const std::string css = ":root {\n --brand: #abc;\n --gap: calc(1px + 2px);\n --brand: red;\n --quoted: \"a;b\";\n /* --comment: red; */\n}\n";
    if (!extractCssDesignTokens(css, report, error) || report.tokens.size() != 3 || report.tokens[0].name != "--brand" || report.tokens[1].value != "calc(1px + 2px)" || report.tokens[0].line != 2 || report.warnings.size() < 2) return 4;
    if (!extractCssDesignTokens("/*\n --fake: 1;\n*/\n:root { --inline: 1; }\n", report, error) || !report.tokens.empty() || report.warnings.empty()) return 5;
    if (extractCssDesignTokens(std::string(4U * 1024U * 1024U + 1, 'x'), report, error) || !report.tokens.empty()) return 6;
    if (!extractJsonDesignTokens(R"({"a.b":1,"valid":null})", report, error) || !report.tokens.empty() || report.warnings.size() != 2) return 7;
    CssTokenUsageReport usages;
    if (!findCssTokenUsages("/* var(--fake) */\n.x { color: var( --brand, var(--fallback)); content: 'var(--no)'; x: myvar(--no); }", usages, error) ||
        usages.usages.size() != 2 || usages.usages[0].name != "--brand" || usages.usages[0].line != 2 || usages.usages[0].column != 18 || usages.usages[1].name != "--fallback") return 8;
    if (findCssTokenUsages(std::string(4U * 1024U * 1024U + 1, 'x'), usages, error) || !usages.usages.empty() || error.empty()) return 9;
    if (!findCssTokenUsages("évar(--false); var(--actual)",usages,error) || usages.usages.size()!=1 || usages.usages[0].name!="--actual") return 15;
    if (extractJsonDesignTokens(R"({"color":{"brand":"red","brand":"blue"}})", report, error) || !report.tokens.empty() || error.empty()) return 10;
    const std::string json = R"({"palette":{"$type":"color","brand":{"$value":"#fff"}},"huge":9007199254740993})";
    if (!extractJsonDesignTokens(json,report,error) || report.tokens.size()!=2 || report.tokens[0].type!="color" || json.substr(report.tokens[0].byteOffset,6)!="\"#fff\"" || report.tokens[1].value!="9007199254740993") return 11;
    const std::string hugeKey(4100,'x');
    if (!extractJsonDesignTokens("{\""+hugeKey+"\":{\"x\":1},\"ok\":2}",report,error) || report.tokens.size()!=1 || report.tokens[0].name!="ok" || report.warnings.empty()) return 12;
    if (!extractCssDesignTokens(css,report,error) || css.substr(report.tokens[0].byteOffset,7)!="--brand") return 13;
    std::string warnings; for (int i=0;i<1002;++i) warnings += ":root { /* comment */\n";
    if (!extractCssDesignTokens(warnings,report,error) || !report.warningsTruncated) return 14;
    const std::string multiline = ".x { color: var(\r\n  --brand\n  , var(\n\t--fallback\n)); }\r.y { color: var(\f --third\n); }";
    if (!findCssTokenUsages(multiline, usages, error) || usages.usages.size() != 3 ||
        usages.usages[0].name != "--brand" || usages.usages[0].line != 2 || usages.usages[0].column != 3 ||
        usages.usages[1].name != "--fallback" || usages.usages[1].line != 4 || usages.usages[1].column != 2 ||
        usages.usages[2].name != "--third" || usages.usages[2].line != 6 || usages.usages[2].column != 19 ||
        multiline.substr(usages.usages[0].byteOffset, 7) != "--brand" || multiline.substr(usages.usages[1].byteOffset, 10) != "--fallback") return 16;
    const std::string crCss = ":root {\r  --brand: red;\r  color: var(\r --brand\r );\r}\r";
    if (!extractCssDesignTokens(crCss, report, error) || report.tokens.size() != 1 || report.tokens[0].line != 2 || crCss.substr(report.tokens[0].byteOffset, 7) != "--brand") return 17;
    if (!findCssTokenUsages(crCss, usages, error) || usages.usages.size() != 1 || usages.usages[0].line != 4 || usages.usages[0].column != 2) return 18;
    return 0;
}
