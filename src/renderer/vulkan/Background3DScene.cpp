#include "renderer/vulkan/Background3DScene.hpp"
#include "renderer/vulkan/Background3DPipeline.hpp"
#include "renderer/vulkan/Device.hpp"
#include "renderer/scene/Camera.hpp"
#include "renderer/scene/Mesh.hpp"
#include "editor/json/Json.hpp"
#include "text/font/FontAtlas.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
namespace lucid::vk {
namespace {
std::array<float, 4> sceneColor(std::string_view scene, std::string_view name,
                                std::array<float, 4> color) {
    const auto has = [name](std::string_view part) { return name.find(part) != std::string_view::npos; };
    const auto set = [&color](float r, float g, float b) { color[0] = r; color[1] = g; color[2] = b; };
    if (scene == "forest-fireflies") {
        if (has("campfire-core")) set(1.0F, 0.88F, 0.46F);
        else if (has("campfire-flames") || has("embers")) set(0.95F, 0.38F, 0.12F);
        else if (has("firefly") || has("lantern-glass")) set(0.95F, 0.78F, 0.35F);
        else if (has("pine-foliage")) set(0.10F, 0.28F, 0.16F);
        else if (has("canopy-foliage") || has("individual-leaves") || has("ferns")) set(0.20F, 0.38F, 0.21F);
        else if (has("tree-bark") || has("camp-wood")) set(0.30F, 0.19F, 0.12F);
        else if (has("forest-floor")) set(0.11F, 0.14F, 0.09F);
        else if (has("forest-stones")) set(0.29F, 0.29F, 0.27F);
        else if (has("tent-fabric")) set(0.55F, 0.32F, 0.18F);
        else if (has("tent-entrance")) set(0.09F, 0.07F, 0.06F);
    } else if (scene == "city-transit") {
        if (has("warm-windows") || has("train-interior")) set(0.94F, 0.68F, 0.32F);
        else if (has("sign-glow") || has("aircraft-lights")) set(0.40F, 0.73F, 0.95F);
        else if (has("train-body")) set(0.53F, 0.24F, 0.17F);
        else if (has("graphite-masonry")) set(0.23F, 0.22F, 0.24F);
        else if (has("masonry-trim")) set(0.38F, 0.34F, 0.30F);
        else if (has("rail-steel")) set(0.30F, 0.33F, 0.36F);
    } else if (scene == "ocean-lighthouse") {
        if (has("ocean-water")) set(0.08F, 0.27F, 0.39F);
        else if (has("sea-foam")) set(0.70F, 0.84F, 0.82F);
        else if (has("lighthouse-trim")) set(0.52F, 0.20F, 0.15F);
        else if (has("lighthouse-stone")) set(0.68F, 0.64F, 0.55F);
        else if (has("lantern") || has("beacon")) set(0.96F, 0.77F, 0.38F);
        else if (has("coastal-rock")) set(0.28F, 0.29F, 0.25F);
    } else if (scene == "mountain-parallax") {
        if (has("pine-needles")) set(0.12F, 0.28F, 0.18F);
        else if (has("pine-bark")) set(0.28F, 0.20F, 0.14F);
        else if (has("snow") || has("cloud")) set(0.65F, 0.72F, 0.76F);
        else if (has("foothills")) set(0.22F, 0.31F, 0.27F);
        else if (has("mountain")) set(0.34F, 0.38F, 0.42F);
    } else if (scene == "lunar-horizon" || scene == "orbital-station") {
        if (has("earth-ocean") || has("planet-ocean")) set(0.09F, 0.28F, 0.48F);
        else if (has("earth-land") || has("planet-continents")) set(0.22F, 0.40F, 0.24F);
        else if (has("cloud")) set(0.75F, 0.80F, 0.84F);
        else if (has("solar-cells")) set(0.10F, 0.23F, 0.47F);
        else if (has("windows") || has("starlight") || has("beacons")) set(0.90F, 0.82F, 0.63F);
        else if (has("regolith") || has("crater") || has("basalt")) set(0.31F, 0.30F, 0.28F);
        else if (has("metal") || has("frame")) set(0.48F, 0.51F, 0.54F);
    }
    return color;
}
void subdivideTent(renderer::scene::Mesh& mesh) {
    using renderer::scene::MeshVertex;
    const auto midpoint = [](const MeshVertex& a, const MeshVertex& b) {
        MeshVertex result{};
        for (std::size_t i = 0; i < 3; ++i) {
            result.position[i] = (a.position[i] + b.position[i]) * 0.5F;
            result.normal[i] = (a.normal[i] + b.normal[i]) * 0.5F;
        }
        return result;
    };
    for (int level = 0; level < 3; ++level) {
        renderer::scene::Mesh next;
        next.vertices.reserve(mesh.indices.size() * 4);
        next.indices.reserve(mesh.indices.size() * 4);
        const auto triangle = [&next](const MeshVertex& a, const MeshVertex& b, const MeshVertex& c) {
            for (const auto& vertex : {a, b, c}) {
                next.indices.push_back(static_cast<std::uint32_t>(next.vertices.size()));
                next.vertices.push_back(vertex);
            }
        };
        for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const auto& a = mesh.vertices[mesh.indices[i]];
            const auto& b = mesh.vertices[mesh.indices[i + 1]];
            const auto& c = mesh.vertices[mesh.indices[i + 2]];
            const auto ab = midpoint(a, b), bc = midpoint(b, c), ca = midpoint(c, a);
            triangle(a, ab, ca); triangle(ab, b, bc);
            triangle(ca, bc, c); triangle(ab, bc, ca);
        }
        mesh = std::move(next);
    }
}
}
Background3DScene::Background3DScene(const Device& device, std::string_view sceneName) {
    m_lunar = sceneName == "lunar-horizon";
    m_mountains = sceneName == "mountain-parallax";
    m_lighthouse = sceneName == "ocean-lighthouse";
    m_rainyStreet = sceneName == "rainy-street";
    m_orbit = sceneName == "orbital-station";
    m_forest = sceneName == "forest-fireflies";
    m_drafting = sceneName == "drafting-grid";
    m_retro = sceneName == "retro-landscape";
    if (sceneName.empty() || sceneName.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-") != std::string_view::npos)
        throw std::runtime_error("Invalid 3D background scene name");
    const std::string directory = std::string("backgrounds/") + std::string(sceneName) + "/";
    const auto manifestPath = text::assetPath(directory + "scene.json");
    if (manifestPath.empty()) throw std::runtime_error("3D background manifest missing");
    std::ifstream manifestFile(manifestPath, std::ios::binary);
    const std::string manifest{std::istreambuf_iterator<char>(manifestFile), std::istreambuf_iterator<char>()};
    if (!manifestFile.eof() && manifestFile.fail()) throw std::runtime_error("Could not read 3D background manifest");
    editor::JsonError parseError; const auto json = editor::parseJson(manifest, parseError);
    const auto* parts = json ? json->find("parts") : nullptr;
    if (!parts || parts->type != editor::JsonType::Array || parts->children.empty() || parts->children.size() > 128)
        throw std::runtime_error("Invalid 3D background manifest");
    for (const auto& spec : parts->children) {
        const auto* name = spec.find("mesh"); const auto* group = spec.find("group"); const auto* color = spec.find("color");
        if (!name || name->type != editor::JsonType::String || name->stringValue.empty() ||
            name->stringValue.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-.") != std::string::npos ||
            name->stringValue.find("..") != std::string::npos || !name->stringValue.ends_with(".obj") ||
            !group || group->type != editor::JsonType::String || !color || color->type != editor::JsonType::Array || color->children.size() != 4)
            throw std::runtime_error("Invalid 3D background part");
        std::array<float,4> partColor{};
        for (std::size_t i = 0; i < partColor.size(); ++i) {
            const auto& component = color->children[i];
            if (component.type != editor::JsonType::Number || !std::isfinite(component.numberValue) || component.numberValue < 0 || component.numberValue > 1)
                throw std::runtime_error("Invalid 3D background material color");
            partColor[i] = static_cast<float>(component.numberValue);
        }
        const auto path = text::assetPath(directory + name->stringValue);
        if (path.empty()) throw std::runtime_error("3D background asset missing: " + name->stringValue);
        std::ifstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("Could not read 3D background asset");
        const std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        renderer::scene::Mesh mesh; std::string error;
        if (!renderer::scene::parseObj(source, mesh, error)) throw std::runtime_error("Invalid 3D background asset: " + error);
        Part part; part.color = partColor;
        part.naturalColor = sceneColor(sceneName, name->stringValue, partColor);
        const auto& filename = name->stringValue;
        if (m_lunar && filename.find("lunar-regolith") != std::string::npos)
            part.surface = 22.0F;
        else if (m_lunar && (filename.find("basalt") != std::string::npos || filename.find("crater-rock") != std::string::npos))
            part.surface = 13.0F;
        else if (m_forest && filename.find("forest-stones") != std::string::npos)
            part.surface = 14.0F;
        else if (filename.find("masonry") != std::string::npos || filename.find("lighthouse-stone") != std::string::npos)
            part.surface = 1.0F;
        else if (filename.find("regolith") != std::string::npos || filename.find("mountain-slate") != std::string::npos || filename.find("coastal-rock") != std::string::npos || filename.find("forest-stones") != std::string::npos)
            part.surface = 2.0F;
        else if (filename.find("wet-pavement") != std::string::npos) part.surface = 3.0F;
        else if (filename.find("city-ground") != std::string::npos) part.surface = 17.0F;
        else if (filename.find("pavement-joints") != std::string::npos) part.surface = 15.0F;
        else if (filename.find("pine-foliage") != std::string::npos || filename.find("canopy-foliage") != std::string::npos) part.surface = 4.0F;
        else if (filename.find("forest-floor") != std::string::npos) part.surface = 5.0F;
        else if (m_orbit && filename.find("planet-ocean") != std::string::npos) part.surface = 7.0F;
        else if (filename.find("warm-windows") != std::string::npos) part.surface = 8.0F;
        else if (filename.find("train-interior") != std::string::npos) part.surface = 16.0F;
        else if (filename.find("navigation-beacons") != std::string::npos) part.surface = 18.0F;
        else if (filename.find("sign-glow") != std::string::npos) part.surface = 19.0F;
        else if (filename.find("rain-reflections") != std::string::npos) part.surface = 9.0F;
        else if (m_orbit && filename.find("planet-windows-and-lights") != std::string::npos) part.surface = 10.0F;
        else if (m_lunar && filename.find("starlight") != std::string::npos) part.surface = 10.0F;
        else if (m_drafting && filename.find("grid-lines") != std::string::npos) part.surface = 11.0F;
        else if (m_retro && filename.find("perspective-grid") != std::string::npos) part.surface = 20.0F;
        if (const auto* emission = spec.find("emission")) {
            if (emission->type != editor::JsonType::Number || !std::isfinite(emission->numberValue) || emission->numberValue < 0 || emission->numberValue > 4)
                throw std::runtime_error("Invalid 3D background emission");
            part.emission = static_cast<float>(emission->numberValue);
        }
        if (group->stringValue == "train") part.motion = Part::Motion::Train;
        else if (group->stringValue == "tent" && m_forest) part.motion = Part::Motion::Tent;
        else if (group->stringValue == "aircraft") part.motion = Part::Motion::Aircraft;
        else if (group->stringValue == "meteor") { part.motion = Part::Motion::Meteor; part.surface = 0.0F; part.emission = 1.0F; }
        else if (group->stringValue == "birds") part.motion = Part::Motion::Birds;
        else if (group->stringValue == "sparks") part.motion = Part::Motion::Sparks;
        else if (group->stringValue == "flame") part.motion = Part::Motion::Flame;
        else if (group->stringValue == "satellite") part.motion = Part::Motion::Satellite;
        else if (group->stringValue == "clouds") part.motion = Part::Motion::Clouds;
        else if (group->stringValue == "ocean") part.motion = Part::Motion::Ocean;
        else if (group->stringValue == "beacon") { part.motion = Part::Motion::Beacon; part.color[3] = 0.16F; }
        else if (group->stringValue == "streetlight") { part.surface = 6.0F; part.color[3] = 0.24F; }
        else if (group->stringValue == "rain") part.motion = Part::Motion::Rain;
        else if (group->stringValue == "ripples") { part.motion = Part::Motion::Ripples; part.color[3] = 0.45F; }
        else if (group->stringValue == "station") part.motion = Part::Motion::Station;
        else if (group->stringValue == "fireflies") part.motion = Part::Motion::Fireflies;
        else if (group->stringValue == "compass") part.motion = Part::Motion::Compass;
        else if (group->stringValue == "tracer") part.motion = Part::Motion::Tracer;
        else if (group->stringValue == "retro-grid") part.motion = Part::Motion::RetroGrid;
        else if (group->stringValue == "retro-sun") part.motion = Part::Motion::RetroSun;
        if (group->stringValue != "meteor" && ((m_orbit && filename.find("planet-windows-and-lights") != std::string::npos) || (m_lunar && filename.find("starlight") != std::string::npos))) part.motion = Part::Motion::Stars;
        if (m_orbit && group->stringValue == "planet" && part.motion != Part::Motion::Stars)
            part.motion = filename.find("clouds") != std::string::npos ? Part::Motion::PlanetClouds : Part::Motion::Planet;
        if (m_lighthouse && part.motion == Part::Motion::Ocean) part.surface = 12.0F;
        if (m_lunar && name->stringValue.starts_with("landscape-earth-")) part.motion = Part::Motion::LunarEarth;
        if ((m_forest && (filename.find("pine-foliage") != std::string::npos || filename.find("canopy-foliage") != std::string::npos || filename.find("individual-leaves") != std::string::npos || filename.find("tree-bark") != std::string::npos)) ||
            (m_mountains && (filename.find("pine-needles") != std::string::npos || filename.find("pine-bark") != std::string::npos)) ||
            (m_rainyStreet && filename.find("street-foliage") != std::string::npos)) part.motion = Part::Motion::Vegetation;
        // Stretch only the distant halves of ground meshes; nearby scene
        // objects retain their authored positions and the fade has room to end.
        const bool extendGround = filename.find("city-ground") != std::string::npos ||
            filename.find("forest-floor") != std::string::npos ||
            filename.find("lunar-regolith") != std::string::npos ||
            filename.find("wet-pavement") != std::string::npos ||
            filename.find("pavement-joints") != std::string::npos ||
            (m_lighthouse && part.motion == Part::Motion::Ocean);
        if (extendGround) for (auto& vertex : mesh.vertices)
            vertex.position[2] -= std::max(-vertex.position[2], 0.0F) * 0.5F;
        if (part.motion == Part::Motion::Tent && filename.find("tent-fabric") != std::string::npos) {
            part.surface = 23.0F;
            subdivideTent(mesh);
        }
        part.naturalColor[3] = part.color[3];
        part.indexCount = static_cast<std::uint32_t>(mesh.indices.size());
        part.vertices = std::make_unique<Buffer>(); part.indices = std::make_unique<Buffer>();
        const VkDeviceSize vertexBytes = static_cast<VkDeviceSize>(mesh.vertices.size() * sizeof(renderer::scene::MeshVertex));
        const VkDeviceSize indexBytes = static_cast<VkDeviceSize>(mesh.indices.size() * sizeof(std::uint32_t));
        constexpr auto hostMemory = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        part.vertices->create(device, vertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, hostMemory, true);
        part.indices->create(device, indexBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, hostMemory, true);
        part.vertices->upload(mesh.vertices.data(), vertexBytes); part.indices->upload(mesh.indices.data(), indexBytes);
        m_parts.push_back(std::move(part));
    }
    std::stable_sort(m_parts.begin(), m_parts.end(), [](const Part& a, const Part& b) { return a.color[3] > b.color[3]; });
}
void Background3DScene::draw(VkCommandBuffer command, const Background3DPipeline& pipeline, VkExtent2D extent, float time, float dim, const std::array<float,4>& background, const std::array<float,3>& primary, bool naturalColors) const {
    if (!extent.width || !extent.height) return;
    // Extend the viewport above and below its original bounds while retaining
    // the same center and projected scale. Tall geometry can then enter the
    // scene naturally instead of being cut by a visible horizontal line.
    constexpr float referenceHeight = 0.55F;
    constexpr float viewportHeight = 0.83F;
    const float height = static_cast<float>(extent.height) * viewportHeight;
    VkViewport viewport{0.0F, static_cast<float>(extent.height) * 0.43F, static_cast<float>(extent.width), height, 0.0F, 1.0F};
    VkRect2D scissor{{0, static_cast<std::int32_t>(viewport.y)}, {extent.width, extent.height - static_cast<std::uint32_t>(viewport.y)}};
    vkCmdSetViewport(command, 0, 1, &viewport); vkCmdSetScissor(command, 0, 1, &scissor);
    renderer::scene::Camera camera; camera.position = {-16,15,24}; camera.target = {0,1.2F,0}; camera.verticalFovRadians = 0.48F;
    if (m_lunar) { camera.position = {0,12,21}; camera.target = {0,2.2F,0}; camera.verticalFovRadians = 0.63F; }
    if (m_mountains) { camera.position = {0,13,24}; camera.target = {0,3,-2}; camera.verticalFovRadians = 0.68F; }
    if (m_lighthouse) { camera.position = {-3,9,19}; camera.target = {-3,1.5F,0}; camera.verticalFovRadians = 0.70F; }
    if (m_rainyStreet) { camera.position = {0,8,16}; camera.target = {0,2.8F,-1}; camera.verticalFovRadians = 0.70F; }
    if (m_orbit) { camera.position = {0,12,24}; camera.target = {0,2.2F,0}; camera.verticalFovRadians = 0.70F; }
    if (m_forest) { camera.position = {0,10,24}; camera.target = {0,2.6F,-1}; camera.verticalFovRadians = 0.67F; }
    if (m_drafting) { camera.position = {0,25,8}; camera.target = {0,0,0}; camera.verticalFovRadians = 0.74F; }
    if (m_retro) { camera.position = {0,10,24}; camera.target = {0,0.8F,0}; camera.verticalFovRadians = 0.70F; }
    // Zoom each composition consistently while preserving its perspective.
    camera.verticalFovRadians = 2.0F * std::atan(std::tan(camera.verticalFovRadians * 0.72F * 0.5F) * (viewportHeight / referenceHeight));
    // A slow camera drift adds parallax while the modeled objects animate.
    const float cameraDrift = 0.18F * std::sin(time * 0.08F);
    camera.position[0] += cameraDrift; camera.target[0] += cameraDrift;
    std::array<float,16> matrix{};
    if (!renderer::scene::viewProjection(camera, viewport.width / viewport.height, matrix)) return;
    if (m_forest || m_mountains || m_lighthouse || m_rainyStreet || (!m_lunar && !m_mountains && !m_lighthouse && !m_rainyStreet && !m_orbit && !m_drafting && !m_retro)) {
        // Extend the forest viewport upward without changing the projected
        // position or scale of the existing landscape. This lets the tall
        // foreground canopy enter naturally instead of being clipped flat.
        const float originalY = viewport.y;
        const float originalHeight = viewport.height;
        viewport.y = 0.0F;
        viewport.height = originalY + originalHeight;
        const float scale = originalHeight / viewport.height;
        const float offset = originalY / viewport.height;
        for (int column = 0; column < 4; ++column) {
            const auto i = static_cast<std::size_t>(column * 4);
            matrix[i + 1] = scale * matrix[i + 1] + offset * matrix[i + 3];
        }
        scissor = {{0, 0}, extent};
    }
    renderer::scene::Camera skyCamera; skyCamera.position = {0,12,24}; skyCamera.target = {0,8,0}; skyCamera.verticalFovRadians = 0.90F;
    std::array<float,16> skyMatrix{};
    if (!renderer::scene::viewProjection(skyCamera, static_cast<float>(extent.width) / static_cast<float>(extent.height), skyMatrix)) return;
    const VkViewport skyViewport{0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0, 1};
    const VkRect2D skyScissor{{0,0}, extent};
    for (const auto& part : m_parts) {
        const bool meteor = part.motion == Part::Motion::Meteor;
        const float meteorPhase = std::fmod(std::max(time, 0.0F), 62.831853F);
        if (meteor && meteorPhase > 14.0F) continue;
        const bool stars = part.motion == Part::Motion::Stars || meteor;
        vkCmdSetViewport(command, 0, 1, stars ? &skyViewport : &viewport);
        vkCmdSetScissor(command, 0, 1, stars ? &skyScissor : &scissor);
        auto transform = stars ? skyMatrix : matrix;
        float normalRotation = 0.0F;
        if (meteor) {
            const float x = meteorPhase * 3.6F;
            const float y = -meteorPhase * 0.5F;
            for (int row=0; row<4; ++row) transform[static_cast<std::size_t>(12+row)] += skyMatrix[static_cast<std::size_t>(row)] * x + skyMatrix[static_cast<std::size_t>(4+row)] * y;
        }
        if (m_orbit && part.motion == Part::Motion::Stars) {
            // Move the whole distant field gently, without wrapping stars in
            // the visible viewport or distorting their individual geometry.
            const float x = 1.8F * std::sin(time * 0.04F);
            const float y = 0.6F * std::sin(time * 0.03F);
            for (int row = 0; row < 4; ++row) {
                const auto i = static_cast<std::size_t>(row);
                transform[12+i] += skyMatrix[i] * x + skyMatrix[4+i] * y;
            }
        }
        if (part.motion == Part::Motion::Planet || part.motion == Part::Motion::PlanetClouds) {
            // OBJ coordinates are in Lucid's Y-up space. Rotate each layer
            // around the same globe center; clouds drift a little faster.
            constexpr float centerX = -42.0F, centerZ = -14.0F;
            // Complete whole turns in the renderer's 200-pi-second clock.
            const float angle = time * (part.motion == Part::Motion::PlanetClouds ? 0.04F : 0.03F);
            const float c = std::cos(angle), s = std::sin(angle);
            normalRotation = angle;
            for (int row = 0; row < 4; ++row) {
                const auto i = static_cast<std::size_t>(row);
                transform[i] = matrix[i] * c - matrix[8+i] * s;
                transform[8+i] = matrix[i] * s + matrix[8+i] * c;
                transform[12+i] = matrix[12+i] + matrix[i] * (centerX - c * centerX - s * centerZ)
                    + matrix[8+i] * (centerZ + s * centerX - c * centerZ) + matrix[4+i] * 5.0F;
            }
        }
        if (part.motion == Part::Motion::Tent) {
            // Angle the complete tent, including its seams and guylines,
            // around its footprint instead of rotating the whole campsite.
            constexpr float angle = 0.34F, centerX = 1.0F, centerZ = -0.7F;
            const float c = std::cos(angle), s = std::sin(angle);
            normalRotation = angle;
            for (int row = 0; row < 4; ++row) {
                const auto i = static_cast<std::size_t>(row);
                transform[i] = matrix[i] * c - matrix[8+i] * s;
                transform[8+i] = matrix[i] * s + matrix[8+i] * c;
                transform[12+i] = matrix[12+i] + matrix[i] * (centerX - c * centerX - s * centerZ)
                    + matrix[8+i] * (centerZ + s * centerX - c * centerZ);
            }
        }
        if (part.motion == Part::Motion::Train) {
            // Complete fifteen traversals per animation clock period; the
            // train wraps beyond the longer viaduct, outside the view.
            const float x = std::fmod(std::max(time, 0.0F) * 1.4323945F + 2.0F * std::sin(time * 0.10F), 60.0F) - 30.0F;
            for (int row=0; row<4; ++row) transform[static_cast<std::size_t>(12+row)] += matrix[static_cast<std::size_t>(row)] * x + matrix[static_cast<std::size_t>(8+row)] * 0.55F;
        }
        if (part.motion == Part::Motion::Aircraft) {
            const float x = std::fmod(std::max(time, 0.0F) * 0.95492966F, 60.0F) - 30.0F;
            const float y = 0.12F * std::sin(time * 0.2F);
            for (int row=0; row<4; ++row) transform[static_cast<std::size_t>(12+row)] += matrix[static_cast<std::size_t>(row)] * x + matrix[static_cast<std::size_t>(4+row)] * y;
        }
        if (part.motion == Part::Motion::Birds) {
            const float x = 6.0F * std::sin(time * 0.04F);
            const float y = 0.22F * std::sin(time * 0.12F);
            for (int row=0; row<4; ++row) transform[static_cast<std::size_t>(12+row)] += matrix[static_cast<std::size_t>(row)] * x + matrix[static_cast<std::size_t>(4+row)] * y;
        }
        if (part.motion == Part::Motion::Satellite) {
            // Keep the satellite to the left of the distant planet throughout
            // its orbit, preserving the reference composition as it moves.
            const float x = -5.0F + 2.8F * std::sin(time * 0.08F);
            const float y = 4.8F + 0.3F * std::sin(time * 0.14F);
            const float angle = 0.12F * std::sin(time * 0.12F);
            const float c = std::cos(angle), s = std::sin(angle);
            normalRotation = angle;
            for (int row=0; row<4; ++row) {
                const auto i = static_cast<std::size_t>(row);
                transform[i] = matrix[i] * c - matrix[8+i] * s;
                transform[8+i] = matrix[i] * s + matrix[8+i] * c;
                transform[12+i] += matrix[i] * x + matrix[4+i] * y;
            }
        }
        if (part.motion == Part::Motion::Clouds) {
            const float x = 2.8F * std::sin(time * 0.04F);
            for (int row=0; row<4; ++row) transform[static_cast<std::size_t>(12+row)] += matrix[static_cast<std::size_t>(row)] * x;
        }
        if (part.motion == Part::Motion::Compass) {
            // Rotate about the centre of the authored construction circles.
            const float angle = time * 0.06F;
            const float c = std::cos(angle), s = std::sin(angle);
            normalRotation = angle;
            for (int row=0; row<4; ++row) {
                const auto i = static_cast<std::size_t>(row);
                transform[i] = matrix[i] * c - matrix[8+i] * s;
                transform[8+i] = matrix[i] * s + matrix[8+i] * c;
                transform[12+i] = matrix[12+i] + matrix[i] * 3.0F;
            }
        }
        if (part.motion == Part::Motion::Tracer) {
            // Reverse smoothly at the ends instead of jumping back to start.
            const float x = 5.0F - 5.0F * std::cos(time * 0.20F);
            for (int row=0; row<4; ++row) transform[static_cast<std::size_t>(12+row)] += matrix[static_cast<std::size_t>(row)] * x - matrix[static_cast<std::size_t>(8+row)] * x * 0.6F;
        }
        if (part.motion == Part::Motion::RetroGrid) {
            const float z = std::fmod(std::max(time, 0.0F) * 0.45042257F, 1.0F);
            for (int row=0; row<4; ++row) transform[static_cast<std::size_t>(12+row)] += matrix[static_cast<std::size_t>(8+row)] * z;
        }
        if (part.motion == Part::Motion::RetroSun) {
            const float y = 0.15F * std::sin(time * 0.10F);
            for (int row=0; row<4; ++row) transform[static_cast<std::size_t>(12+row)] += matrix[static_cast<std::size_t>(4+row)] * y;
        }
        if (part.motion == Part::Motion::Beacon) {
            const float angle = 0.30F * std::sin(time * 0.25F);
            normalRotation = angle;
            const float c = std::cos(angle), s = std::sin(angle);
            for (int row=0; row<4; ++row) {
                const auto i = static_cast<std::size_t>(row);
                transform[i] = matrix[i] * c - matrix[8+i] * s;
                transform[8+i] = matrix[i] * s + matrix[8+i] * c;
                transform[12+i] = matrix[12+i] + matrix[i] * 6.4F + matrix[4+i] * 4.42F;
            }
        }
        if (part.motion == Part::Motion::Station) {
            const float angle = time * 0.12F;
            constexpr float tilt = 0.38F;
            normalRotation = angle;
            const float c = std::cos(angle), s = std::sin(angle);
            const float ct = std::cos(tilt), st = std::sin(tilt);
            for (int row=0; row<4; ++row) {
                const auto i = static_cast<std::size_t>(row);
                const float tiltedX = matrix[i] * ct + matrix[4+i] * st;
                transform[i] = tiltedX * c - matrix[8+i] * s;
                transform[4+i] = -matrix[i] * st + matrix[4+i] * ct;
                transform[8+i] = tiltedX * s + matrix[8+i] * c;
                transform[12+i] = matrix[12+i] + matrix[i] * (4.5F + 0.28F * std::sin(time * 0.04F)) + matrix[4+i] * (5.0F + 0.15F * std::sin(time * 0.06F));
            }
        }
        const float animationKind = part.motion == Part::Motion::Ocean ? 1.0F : part.motion == Part::Motion::Rain ? 2.0F : part.motion == Part::Motion::Fireflies ? 3.0F : part.motion == Part::Motion::LunarEarth ? 4.0F : part.motion == Part::Motion::Beacon ? 5.0F : part.motion == Part::Motion::Vegetation ? 6.0F : part.motion == Part::Motion::RetroSun ? 7.0F : (part.motion == Part::Motion::Stars || meteor) ? 8.0F : part.motion == Part::Motion::Flame ? 9.0F : part.motion == Part::Motion::Aircraft ? 10.0F : part.motion == Part::Motion::Birds ? 11.0F : part.motion == Part::Motion::Ripples ? 12.0F : part.motion == Part::Motion::Sparks ? 13.0F : part.motion == Part::Motion::Station ? 14.0F : part.motion == Part::Motion::Tent ? 15.0F : 0.0F;
        float strength = 0.42F - 0.22F * std::clamp(dim, 0.0F, 1.0F);
        if (m_forest && part.motion != Part::Motion::Fireflies && part.motion != Part::Motion::Flame && part.motion != Part::Motion::Sparks) strength *= 0.70F;
        if (meteor) strength *= 1.4F;
        if (m_forest && (part.motion == Part::Motion::Fireflies || part.motion == Part::Motion::Flame || part.motion == Part::Motion::Sparks)) strength *= 1.35F;
        const std::array<float,4> animation{animationKind, time, strength, normalRotation};
        // Pack the primary color into unused push-constant components, keeping
        // the total within Vulkan's guaranteed 128-byte minimum.
        auto backgroundAndPrimary = background;
        backgroundAndPrimary[3] = naturalColors ? -1.0F : primary[0];
        pipeline.draw(command, part.vertices->handle(), part.indices->handle(), part.indexCount, transform, naturalColors ? part.naturalColor : part.color, animation, backgroundAndPrimary, {part.emission, part.surface, primary[1], primary[2]});
    }
}
}
