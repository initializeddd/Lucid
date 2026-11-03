#include "renderer/scene/Mesh.hpp"
#include "editor/json/Json.hpp"
#include <cmath>
#include <string>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <vector>
int main(int argc, char** argv) {
    using namespace lucid::renderer::scene;
    Mesh mesh; std::string error;
    if (!parseObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf -4 -3 -2 -1\n", mesh, error)) return 1;
    if (mesh.indices.size() != 6 || mesh.vertices.size() != 6 || std::abs(mesh.vertices[0].normal[2] - 1.0F) > 0.001F) return 2;
    if (parseObj("v 0 0 0\nf 1 2 3\n", mesh, error) || mesh.indices.size() != 6) return 3;
    if (parseObj("v 0 0 0\nv 1 0 0\nv 2 0 0\nf 1 2 3\n", mesh, error)) return 4;
    if (!parseObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 -1\nf 1//1 2//1 3//1\n", mesh, error) || mesh.vertices[0].normal[2] != -1.0F) return 5;
    for (int argument = 1; argument < argc; ++argument) {
        const auto directory = std::filesystem::path(argv[argument]);
        std::ifstream manifestFile(directory / "scene.json", std::ios::binary);
        if (!manifestFile) return 9;
        const std::string manifest{std::istreambuf_iterator<char>(manifestFile), std::istreambuf_iterator<char>()};
        lucid::editor::JsonError parseError;
        const auto parsed = lucid::editor::parseJson(manifest, parseError);
        const auto* parts = parsed ? parsed->find("parts") : nullptr;
        if (!parts || parts->type != lucid::editor::JsonType::Array || parts->children.empty()) return 10;
        std::unordered_set<std::string> declared;
        std::unordered_set<std::string> groups;
        for (const auto& part : parts->children) {
            const auto* name = part.find("mesh"); const auto* group = part.find("group"); const auto* color = part.find("color");
            const auto* emission = part.find("emission");
            if (!name || name->type != lucid::editor::JsonType::String || name->stringValue.empty() ||
                name->stringValue.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-.") != std::string::npos ||
                name->stringValue.find("..") != std::string::npos || !name->stringValue.ends_with(".obj") ||
                !group || group->type != lucid::editor::JsonType::String ||
                !color || color->type != lucid::editor::JsonType::Array || color->children.size() != 4 ||
                !emission || emission->type != lucid::editor::JsonType::Number || !std::isfinite(emission->numberValue) || emission->numberValue < 0 || emission->numberValue > 4 ||
                !declared.insert(name->stringValue).second || !std::filesystem::is_regular_file(directory / name->stringValue)) return 11;
            groups.insert(group->stringValue);
            for (const auto& value : color->children)
                if (value.type != lucid::editor::JsonType::Number || !std::isfinite(value.numberValue) || value.numberValue < 0 || value.numberValue > 1) return 12;
        }
        // Regenerating a scene must retain its authored animated elements.
        // A valid OBJ alone does not catch an accidentally omitted animation group.
        const std::unordered_map<std::string, std::vector<std::string>> requiredGroups{
            {"city-transit", {"train", "aircraft"}},
            {"lunar-horizon", {"satellite", "meteor"}},
            {"mountain-parallax", {"clouds", "birds"}},
            {"ocean-lighthouse", {"ocean", "beacon", "birds"}},
            {"rainy-street", {"rain", "ripples"}},
            {"orbital-station", {"station", "meteor"}},
            {"forest-fireflies", {"fireflies", "flame", "sparks"}},
            {"drafting-grid", {"tracer", "compass"}},
            {"retro-landscape", {"retro-grid", "retro-sun"}},
        };
        const auto expected = requiredGroups.find(directory.filename().string());
        if (expected == requiredGroups.end()) return 14;
        for (const auto& group : expected->second) {
            if (!groups.contains(group)) {
                std::cerr << directory << ": missing animation group " << group << '\n';
                return 15;
            }
        }
        std::size_t count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.path().extension() != ".obj") continue;
            std::ifstream file(entry.path(), std::ios::binary);
            if (!file) return 6;
            const std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
            if (!parseObj(source, mesh, error)) { std::cerr << entry.path() << ": " << error << '\n'; return 7; }
            if (!declared.contains(entry.path().filename().string())) return 13;
            ++count;
        }
        if (count < 3 || count != declared.size()) return 8;
    }
    return 0;
}
