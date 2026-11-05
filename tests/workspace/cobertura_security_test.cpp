#include "modules/Coverage.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    namespace fs = std::filesystem;
    using namespace lucid::modules;
    const auto root = fs::temp_directory_path() / ("lucid-cobertura-audit-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "src");
    { std::ofstream out(root / "src" / "source.cpp"); out << "int main() {}\n"; }
    bool created = false; std::string error;
    const auto project = lucid::workspace::Project::openOrCreate(root, created, error, false, false);
    if (!error.empty()) { fs::remove_all(root); return 1; }
    const auto valid = parseCobertura(project, R"(<coverage><sources><source>&#x73;rc</source></sources><class filename="sou&#114;ce&#46;cpp"><lines><line number="1" hits="2"/></lines></class></coverage>)");
    bool ok = !valid.incomplete && valid.lines.size() == 1 && valid.lines[0].path == "src/source.cpp" && valid.lines[0].covered;
    const std::u8string unicodeName = u8"src/源-é.cpp";
    const std::string relative(unicodeName.begin(), unicodeName.end());
    { std::ofstream out(root / fs::path(unicodeName)); out << "int value;\n"; }
    const auto unicodeXml = parseCobertura(project, "<coverage><class filename=\"" + relative + "\"><lines><line number=\"1\" hits=\"2\"/></lines></class></coverage>");
    const auto unicodeLcov = parseLcov(project, "SF:" + relative + "\nDA:1,1\nend_of_record\n");
    ok = ok && !unicodeXml.incomplete && unicodeXml.lines.size() == 1 && unicodeXml.lines[0].path == relative &&
        !unicodeLcov.incomplete && unicodeLcov.lines.size() == 1 && unicodeLcov.lines[0].path == relative;
    for (const auto* invalid : {
        R"(<coverage><class filename="source&#0;.cpp"/></coverage>)",
        R"(<coverage><class filename="source&#xD800;.cpp"/></coverage>)",
        R"(<coverage><class filename="source&#x110000;.cpp"/></coverage>)",
        R"(<coverage><class filename="source&#xZZ;.cpp"/></coverage>)",
        R"(<coverage><class filename="src/source.cpp"><class filename="src/source.cpp"/></class></coverage>)"}) {
        const auto result = parseCobertura(project, invalid);
        if (!result.incomplete || !result.lines.empty()) ok = false;
    }
    const auto rawControl = parseCobertura(project, std::string("<coverage><!--") + char(1) + "--></coverage>");
    if (!rawControl.incomplete || !rawControl.lines.empty()) ok = false;
    std::string attributes = "<coverage";
    for (int i = 0; i < 257; ++i) attributes += " a" + std::to_string(i) + "='v'";
    attributes += "/>";
    const auto excessive = parseCobertura(project, attributes);
    if (!excessive.incomplete || !excessive.lines.empty()) ok = false;
    fs::remove_all(root);
    return ok ? 0 : 2;
}
