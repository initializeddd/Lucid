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
namespace lucid::vk {
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
        const auto& filename = name->stringValue;
        if (m_lunar && (filename.find("lunar-regolith") != std::string::npos || filename.find("basalt") != std::string::npos || filename.find("crater-rock") != std::string::npos))
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
        if (m_lighthouse && part.motion == Part::Motion::Ocean) part.surface = 12.0F;
        if (m_lunar && name->stringValue.starts_with("landscape-earth-")) part.motion = Part::Motion::LunarEarth;
        if ((m_forest && (filename.find("pine-foliage") != std::string::npos || filename.find("canopy-foliage") != std::string::npos || filename.find("individual-leaves") != std::string::npos || filename.find("tree-bark") != std::string::npos)) ||
            (m_mountains && (filename.find("pine-needles") != std::string::npos || filename.find("pine-bark") != std::string::npos)) ||
            (m_rainyStreet && filename.find("street-foliage") != std::string::npos)) part.motion = Part::Motion::Vegetation;
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
void Background3DScene::draw(VkCommandBuffer command, const Background3DPipeline& pipeline, VkExtent2D extent, float time, float dim, const std::array<float,4>& background, const std::array<float,3>& primary) const {
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
            normalRotation = angle;
            const float c = std::cos(angle), s = std::sin(angle);
            for (int row=0; row<4; ++row) {
                const auto i = static_cast<std::size_t>(row);
                transform[i] = matrix[i] * c - matrix[8+i] * s;
                transform[8+i] = matrix[i] * s + matrix[8+i] * c;
                transform[12+i] = matrix[12+i] + matrix[i] * (4.5F + 0.28F * std::sin(time * 0.04F)) + matrix[4+i] * (5.0F + 0.15F * std::sin(time * 0.06F));
            }
        }
        const float animationKind = part.motion == Part::Motion::Ocean ? 1.0F : part.motion == Part::Motion::Rain ? 2.0F : part.motion == Part::Motion::Fireflies ? 3.0F : part.motion == Part::Motion::LunarEarth ? 4.0F : part.motion == Part::Motion::Beacon ? 5.0F : part.motion == Part::Motion::Vegetation ? 6.0F : part.motion == Part::Motion::RetroSun ? 7.0F : (part.motion == Part::Motion::Stars || meteor) ? 8.0F : part.motion == Part::Motion::Flame ? 9.0F : part.motion == Part::Motion::Aircraft ? 10.0F : part.motion == Part::Motion::Birds ? 11.0F : part.motion == Part::Motion::Ripples ? 12.0F : part.motion == Part::Motion::Sparks ? 13.0F : 0.0F;
        float strength = 0.42F - 0.22F * std::clamp(dim, 0.0F, 1.0F);
        if (m_forest && part.motion != Part::Motion::Fireflies && part.motion != Part::Motion::Flame && part.motion != Part::Motion::Sparks) strength *= 0.70F;
        if (meteor) strength *= 1.4F;
        if (m_forest && (part.motion == Part::Motion::Fireflies || part.motion == Part::Motion::Flame || part.motion == Part::Motion::Sparks)) strength *= 1.35F;
        const std::array<float,4> animation{animationKind, time, strength, normalRotation};
        // Pack the primary color into unused push-constant components, keeping
        // the total within Vulkan's guaranteed 128-byte minimum.
        auto backgroundAndPrimary = background;
        backgroundAndPrimary[3] = primary[0];
        pipeline.draw(command, part.vertices->handle(), part.indices->handle(), part.indexCount, transform, part.color, animation, backgroundAndPrimary, {part.emission, part.surface, primary[1], primary[2]});
    }
}
}
