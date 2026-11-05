#include "ui/VisualReviewPreview.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <utility>

namespace lucid::ui {
namespace {
constexpr std::size_t kMaxPixels = 16U * 1024U * 1024U;
bool valid(const modules::VisualImage& image) {
    return image.width && image.height && image.width <= kMaxPixels / image.height &&
           image.rgba.size() == image.width * image.height * 4;
}
bool finiteRect(VisualPreviewRect r) {
    return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.width) &&
           std::isfinite(r.height) && r.width > 0 && r.height > 0;
}
}

bool makeVisualReviewPreview(const modules::VisualImage& baseline,
    const modules::VisualImage& candidate, const modules::VisualComparison& comparison,
    std::size_t maxWidth, std::size_t maxHeight, VisualReviewPreview& preview,
    std::string& error) {
    preview = {}; error.clear();
    if (!valid(baseline) || !valid(candidate) || !valid(comparison.diff) ||
        baseline.width != candidate.width || baseline.height != candidate.height ||
        baseline.width != comparison.diff.width || baseline.height != comparison.diff.height ||
        !maxWidth || !maxHeight || maxWidth > 1024 || maxHeight > 1024) {
        error = "Invalid visual comparison or preview size."; return false;
    }
    const auto scale = std::min({1.0, static_cast<double>(maxWidth) / baseline.width,
                                static_cast<double>(maxHeight) / baseline.height});
    const auto width = std::max<std::size_t>(1, static_cast<std::size_t>(baseline.width * scale));
    const auto height = std::max<std::size_t>(1, static_cast<std::size_t>(baseline.height * scale));
    VisualReviewPreview output;
    for (auto* image : {&output.baseline, &output.candidate, &output.difference}) {
        image->width = width; image->height = height; image->rgba.resize(width * height * 4);
    }
    for (std::size_t y = 0; y < height; ++y) for (std::size_t x = 0; x < width; ++x) {
        const auto x0 = static_cast<std::size_t>(static_cast<std::uint64_t>(x) * baseline.width / width);
        const auto x1 = static_cast<std::size_t>(static_cast<std::uint64_t>(x + 1) * baseline.width / width);
        const auto y0 = static_cast<std::size_t>(static_cast<std::uint64_t>(y) * baseline.height / height);
        const auto y1 = static_cast<std::size_t>(static_cast<std::uint64_t>(y + 1) * baseline.height / height);
        const auto count = (x1 - x0) * (y1 - y0);
        std::uint64_t sums[2][4]{}; bool changed = false;
        for (auto sy = y0; sy < y1; ++sy) for (auto sx = x0; sx < x1; ++sx) {
            const auto index = (sy * baseline.width + sx) * 4;
            for (std::size_t which = 0; which < 2; ++which) {
                const auto& image = which ? candidate : baseline;
                const auto alpha = image.rgba[index + 3];
                for (std::size_t c = 0; c < 3; ++c) sums[which][c] += image.rgba[index + c] * alpha;
                sums[which][3] += alpha;
            }
            changed = changed || comparison.diff.rgba[index + 3] != 0;
        }
        const auto checker = static_cast<std::uint64_t>(((x / 8 + y / 8) % 2) ? 192 : 224);
        const auto destination = (y * width + x) * 4;
        for (std::size_t which = 0; which < 2; ++which) {
            auto& rgba = which ? output.candidate.rgba : output.baseline.rgba;
            for (std::size_t c = 0; c < 3; ++c)
                rgba[destination + c] = static_cast<std::uint8_t>((sums[which][c] + checker * (255ULL * count - sums[which][3])) / (255ULL * count));
            rgba[destination + 3] = 255;
        }
        for (std::size_t c = 0; c < 3; ++c) {
            const auto value = output.candidate.rgba[destination + c];
            output.difference.rgba[destination + c] = changed
                ? static_cast<std::uint8_t>((value * 35U + (c == 0 ? 255U : 0U) * 65U) / 100U) : value;
        }
        output.difference.rgba[destination + 3] = 255;
    }
    static std::atomic<std::uint64_t> nextGeneration{1};
    output.generation = nextGeneration.fetch_add(1, std::memory_order_relaxed);
    preview = std::move(output); return true;
}

VisualPreviewRect fitVisualPreview(VisualPreviewRect area, std::size_t imageWidth, std::size_t imageHeight) {
    if (!finiteRect(area) || !imageWidth || !imageHeight || imageWidth > kMaxPixels / imageHeight) return {};
    const auto scale = std::min(area.width / static_cast<double>(imageWidth), area.height / static_cast<double>(imageHeight));
    const auto width = imageWidth * scale, height = imageHeight * scale;
    VisualPreviewRect result{area.x + (area.width - width) / 2, area.y + (area.height - height) / 2, width, height};
    return finiteRect(result) ? result : VisualPreviewRect{};
}

bool visualPreviewSourcePoint(VisualPreviewRect fitted, std::size_t imageWidth,
    std::size_t imageHeight, double x, double y, VisualPreviewPoint& point) {
    point = {};
    if (!finiteRect(fitted) || !imageWidth || !imageHeight || imageWidth > kMaxPixels / imageHeight || !std::isfinite(x) || !std::isfinite(y) ||
        x < fitted.x || y < fitted.y || x - fitted.x >= fitted.width || y - fitted.y >= fitted.height) return false;
    point.x = std::min(imageWidth - 1, static_cast<std::size_t>((x - fitted.x) / fitted.width * imageWidth));
    point.y = std::min(imageHeight - 1, static_cast<std::size_t>((y - fitted.y) / fitted.height * imageHeight));
    return true;
}
}
