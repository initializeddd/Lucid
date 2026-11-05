#include "modules/VisualComparison.hpp"
#include "editor/json/Json.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <set>
namespace lucid::modules {
namespace {
constexpr std::size_t maxPixels = 16U * 1024U * 1024U, maxMasks = 256, maxConfigBytes = 65536;
bool fields(const editor::JsonValue& object, std::initializer_list<std::string_view> allowed) {
    if (object.type != editor::JsonType::Object) return false;
    std::set<std::string> seen;
    for (const auto& field : object.children) {
        if (!seen.insert(field.key).second || std::find(allowed.begin(), allowed.end(), field.key) == allowed.end()) return false;
    }
    return true;
}
bool integer(const editor::JsonValue* value, std::size_t& out) {
    if (!value || value->type != editor::JsonType::Number || !std::isfinite(value->numberValue) || value->numberValue < 0 ||
        std::floor(value->numberValue) != value->numberValue || value->numberValue > static_cast<double>(maxPixels)) return false;
    out = static_cast<std::size_t>(value->numberValue); return true;
}
bool basicConfig(const VisualReviewConfig& config, std::string& error) {
    if (config.masks.size() > maxMasks) { error = "Too many masks."; return false; }
    for (const auto& mask : config.masks) {
        if (!mask.width || !mask.height || mask.x > maxPixels || mask.y > maxPixels || mask.width > maxPixels || mask.height > maxPixels ||
            mask.width > maxPixels - mask.x || mask.height > maxPixels - mask.y) {
            error = "Invalid mask dimensions."; return false;
        }
    }
    return true;
}
}
bool parseVisualReviewConfig(std::string_view json, VisualReviewConfig& config, std::string& error) {
    config = {}; error.clear();
    if (json.size() > maxConfigBytes) { error = "Visual configuration exceeds size limit."; return false; }
    editor::JsonError parseError; auto root = editor::parseJson(json, parseError);
    if (!root || !fields(*root, {"version", "threshold", "masks"})) { error = "Invalid visual configuration object."; return false; }
    std::size_t version = 0, threshold = 0;
    const auto* masks = root->find("masks");
    if (!integer(root->find("version"), version) || version != 1 || !integer(root->find("threshold"), threshold) || threshold > 255 ||
        !masks || masks->type != editor::JsonType::Array || masks->children.size() > maxMasks) {
        error = "Invalid visual configuration version, threshold, or masks."; return false;
    }
    VisualReviewConfig result; result.threshold = static_cast<std::uint8_t>(threshold);
    for (const auto& item : masks->children) {
        VisualMask mask;
        if (!fields(item, {"x", "y", "width", "height"}) || !integer(item.find("x"), mask.x) || !integer(item.find("y"), mask.y) ||
            !integer(item.find("width"), mask.width) || !integer(item.find("height"), mask.height)) {
            error = "Invalid visual mask."; return false;
        }
        result.masks.push_back(mask);
    }
    if (!basicConfig(result, error)) return false;
    config = std::move(result); return true;
}
bool serializeVisualReviewConfig(const VisualReviewConfig& config, std::string& json, std::string& error) {
    json.clear(); error.clear(); if (!basicConfig(config, error)) return false;
    std::string result = "{\"version\":1,\"threshold\":" + std::to_string(config.threshold) + ",\"masks\":[";
    for (std::size_t i = 0; i < config.masks.size(); ++i) {
        const auto& m = config.masks[i]; if (i) result += ',';
        result += "{\"x\":" + std::to_string(m.x) + ",\"y\":" + std::to_string(m.y) + ",\"width\":" + std::to_string(m.width) + ",\"height\":" + std::to_string(m.height) + '}';
    }
    result += "]}"; json = std::move(result); return true;
}
bool validateVisualReviewConfig(const VisualReviewConfig& config, std::size_t width, std::size_t height, std::string& error) {
    error.clear();
    if (!width || !height || width > maxPixels / height) { error = "Invalid or excessive image dimensions."; return false; }
    if (!basicConfig(config, error)) return false;
    for (const auto& mask : config.masks) {
        if (mask.x >= width || mask.y >= height || mask.width > width - mask.x || mask.height > height - mask.y) {
            error = "Mask must be wholly inside the image."; return false;
        }
    }
    return true;
}
bool compareVisualImages(const VisualImage& baseline, const VisualImage& actual,
                         std::uint8_t threshold, const std::vector<VisualMask>& masks,
                         VisualComparison& result, std::string& error, const std::atomic_bool* cancel) {
    result = {}; error.clear();
    if (cancel && cancel->load()) { error = "Visual comparison cancelled."; return false; }
    const auto width=std::max(baseline.width,actual.width), height=std::max(baseline.height,actual.height);
    if (!baseline.width || !baseline.height || !actual.width || !actual.height ||
        baseline.width > maxPixels / baseline.height || actual.width > maxPixels / actual.height || width > maxPixels / height) { error = "Images have invalid or excessive dimensions."; return false; }
    const auto pixels = width * height;
    if (baseline.rgba.size() != baseline.width * baseline.height * 4 || actual.rgba.size() != actual.width * actual.height * 4) { error = "Image buffer size does not match dimensions."; return false; }
    if (masks.size() > maxMasks) { error = "Too many masks."; return false; }
    for (const auto& mask : masks) {
        const auto commonWidth=std::min(baseline.width,actual.width), commonHeight=std::min(baseline.height,actual.height);
        if (!mask.width || !mask.height || mask.x >= commonWidth || mask.y >= commonHeight ||
            mask.width > commonWidth - mask.x || mask.height > commonHeight - mask.y) {
            error = "Mask must be nonempty and wholly inside both images."; return false;
        }
    }
    // Sweep rectangle boundaries so overlapping full-image masks remain O(pixels + masks log masks).
    struct Event { std::size_t row, left, right; int delta; };
    std::vector<Event> events; events.reserve(masks.size() * 2);
    for (const auto& mask : masks) {
        events.push_back({mask.y, mask.x, mask.x + mask.width, 1});
        events.push_back({mask.y + mask.height, mask.x, mask.x + mask.width, -1});
    }
    std::sort(events.begin(), events.end(), [](const auto& a, const auto& b) { return a.row < b.row; });
    std::vector<int> boundaries(width + 1, 0); std::size_t event = 0;
    VisualComparison output; output.diff.width = width; output.diff.height = height;
    output.diff.rgba.assign(pixels * 4, 0);
    for (std::size_t y = 0; y < height; ++y) {
        if (cancel && cancel->load()) { error = "Visual comparison cancelled."; return false; }
        while (event < events.size() && events[event].row == y) {
            const auto& e = events[event++]; boundaries[e.left] += e.delta; boundaries[e.right] -= e.delta;
        }
        int active = 0;
        for (std::size_t x = 0; x < width; ++x) {
            active += boundaries[x]; const auto i = y * width + x;
            const bool hasBaseline=x<baseline.width && y<baseline.height, hasActual=x<actual.width && y<actual.height;
            if (!hasBaseline && !hasActual) continue;
            if (active) { ++output.maskedPixels; continue; }
            ++output.comparedPixels; bool changed = hasBaseline != hasActual;
            if (!hasBaseline) ++output.addedPixels; else if (!hasActual) ++output.removedPixels;
            if (!changed) for (std::size_t channel = 0; channel < 4; ++channel) {
                const auto a=(y*baseline.width+x)*4+channel, b=(y*actual.width+x)*4+channel;
                if (std::abs(static_cast<int>(baseline.rgba[a]) - static_cast<int>(actual.rgba[b])) > threshold) { changed = true; break; }
            }
            if (changed) { ++output.changedPixels; output.diff.rgba[i * 4] = 255; output.diff.rgba[i * 4 + 3] = 255; }
        }
    }
    result = std::move(output); return true;
}
}
