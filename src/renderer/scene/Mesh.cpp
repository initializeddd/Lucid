#include "renderer/scene/Mesh.hpp"

#include <charconv>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>

namespace lucid::renderer::scene {
namespace {
using Vec = std::array<float, 3>;
constexpr std::size_t kLimit = 1000000;

bool resolve(std::string_view text, std::size_t count, std::size_t& index) {
    int value = 0;
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || end != text.data() + text.size() || value == 0) return false;
    const auto result = value > 0 ? static_cast<long long>(value) - 1 : static_cast<long long>(count) + value;
    if (result < 0 || static_cast<unsigned long long>(result) >= count) return false;
    index = static_cast<std::size_t>(result); return true;
}
bool vec(std::istringstream& stream, Vec& value) {
    if (!(stream >> value[0] >> value[1] >> value[2])) return false;
    return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
}
bool normal(const Vec& a, const Vec& b, const Vec& c, Vec& n) {
    const Vec u{b[0]-a[0], b[1]-a[1], b[2]-a[2]}, v{c[0]-a[0], c[1]-a[1], c[2]-a[2]};
    n = {u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]};
    const float length = std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
    if (!std::isfinite(length) || length < 1e-8F) return false;
    for (float& component : n) component /= length;
    return true;
}
struct Corner { std::size_t position; std::size_t normal; bool hasNormal; };
}

bool parseObj(std::string_view source, Mesh& output, std::string& error) {
    error.clear();
    if (source.size() > 64U * 1024U * 1024U) { error = "OBJ file is too large"; return false; }
    std::istringstream input{std::string(source)};
    std::vector<Vec> positions, normals;
    Mesh mesh;
    std::string line; std::size_t lineNumber = 0;
    auto fail = [&](const char* reason) { error = "OBJ line " + std::to_string(lineNumber) + ": " + reason; return false; };
    while (std::getline(input, line)) {
        ++lineNumber;
        std::istringstream stream(line); std::string tag; stream >> tag;
        if (tag.empty() || tag[0] == '#') continue;
        if (tag == "v" || tag == "vn") {
            Vec v{}; if (!vec(stream, v)) return fail("invalid vector");
            auto& destination = tag == "v" ? positions : normals;
            if (destination.size() >= kLimit) return fail("too many vectors");
            destination.push_back(v);
        } else if (tag == "f") {
            std::vector<Corner> face; std::string token;
            while (stream >> token) {
                if (!token.empty() && token[0] == '#') break;
                const auto slash = token.find('/');
                const auto second = slash == std::string::npos ? std::string::npos : token.find('/', slash + 1);
                Corner corner{};
                if (!resolve(std::string_view(token).substr(0, slash), positions.size(), corner.position)) return fail("invalid position index");
                if (second != std::string::npos && second + 1 < token.size()) {
                    corner.hasNormal = true;
                    if (!resolve(std::string_view(token).substr(second + 1), normals.size(), corner.normal)) return fail("invalid normal index");
                }
                face.push_back(corner);
                if (face.size() > 1024) return fail("face has too many vertices");
            }
            if (face.size() < 3) return fail("face has fewer than three vertices");
            for (std::size_t i = 1; i + 1 < face.size(); ++i) {
                const Corner corners[3] = {face[0], face[i], face[i+1]};
                Vec flat{};
                if (!normal(positions[corners[0].position], positions[corners[1].position], positions[corners[2].position], flat)) return fail("degenerate triangle");
                if (mesh.vertices.size() > kLimit - 3) return fail("mesh is too large");
                for (const auto& corner : corners) {
                    mesh.indices.push_back(static_cast<std::uint32_t>(mesh.vertices.size()));
                    mesh.vertices.push_back({positions[corner.position], corner.hasNormal ? normals[corner.normal] : flat});
                }
            }
        }
    }
    if (mesh.indices.empty()) { error = "OBJ contains no faces"; return false; }
    output = std::move(mesh); return true;
}
} // namespace lucid::renderer::scene
