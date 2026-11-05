#include "modules/LocalizationWorkspace.hpp"
#include <string>
using namespace lucid::modules;
int main() {
    const std::string source = R"({"nested":{"a/b":"Hello {name}","new":"Welcome {name}"},"items":["First","Second"]})";
    const std::string translation = R"({"nested":{"a/b":"Hallo {name}"},"items":["Erste","Zweite"],"untouched":"stay"})";
    LocalizationEditPlan plan; std::string error, output;
    if (!prepareLocalizationEdit(source, translation, "/nested/a~1b", "Hallo {name}!", plan, error) || plan.inserted || !plan.preview.missingPlaceholders.empty()) return 1;
    if (!applyLocalizationEdit(plan, source, translation, output, error) || output.find("Hallo {name}!") == std::string::npos || output.find("\"untouched\":\"stay\"") == std::string::npos) return 2;
    if (applyLocalizationEdit(plan, source + " ", translation, output, error) || !output.empty()) return 3;
    if (applyLocalizationEdit(plan, source, translation + " ", output, error) || !output.empty()) return 4;
    auto tampered = plan; tampered.updatedTranslation = "{}";
    if (applyLocalizationEdit(tampered, source, translation, output, error) || !output.empty()) return 5;
    if (!prepareLocalizationEdit(source, translation, "/nested/new", "Willkommen", plan, error) || !plan.inserted || plan.preview.missingPlaceholders != std::vector<std::string>{"name"}) return 6;
    if (!applyLocalizationEdit(plan, source, translation, output, error) || output.find("\"new\":\"Willkommen\"") == std::string::npos) return 7;
    if (!prepareLocalizationEdit(source, translation, "/items/1", "Zwei\nZeilen", plan, error) || plan.inserted || !applyLocalizationEdit(plan, source, translation, output, error) || output.find("Zwei\\u000aZeilen") == std::string::npos) return 8;
    if (prepareLocalizationEdit(source, R"({"items":[]})", "/items/1", "x", plan, error)) return 9;
    if (prepareLocalizationEdit(source, "{}", "/nested/new", "x", plan, error)) return 10;
    if (prepareLocalizationEdit(source, translation, "/not-source", "x", plan, error)) return 11;
    if (prepareLocalizationEdit(source, translation, "/nested/a~2b", "x", plan, error)) return 12;
    if (prepareLocalizationEdit(source, R"({"nested":{"new":{"child":"x"}}})", "/nested/new", "x", plan, error)) return 13;
    if (prepareLocalizationEdit(source, translation, "/nested/new", std::string("\xff", 1), plan, error)) return 14;
    if (prepareLocalizationEdit(source, R"({"nested":{"new":"a","new":"b"}})", "/nested/new", "x", plan, error)) return 15;
    return 0;
}
