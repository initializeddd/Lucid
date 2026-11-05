#pragma once

#include "modules/VisualComparison.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

namespace lucid::ui {

// Opaque RGBA previews ready for a renderer-owned texture. The difference view
// shows the candidate with a red marker wherever a source pixel changed. A
// marker represents at least one changed pixel, not the proportion changed.
struct VisualReviewPreview {
    modules::VisualImage baseline, candidate, difference;
    std::uint64_t generation = 0;
};

// Box-averages into at most 1024x1024 pixels without enlarging the source. The
// checkerboard represents transparency; no files, network or GPU are accessed.
[[nodiscard]] bool makeVisualReviewPreview(const modules::VisualImage& baseline,
    const modules::VisualImage& candidate, const modules::VisualComparison& comparison,
    std::size_t maxWidth, std::size_t maxHeight, VisualReviewPreview& preview,
    std::string& error);

struct VisualPreviewRect { double x = 0, y = 0, width = 0, height = 0; };
struct VisualPreviewPoint { std::size_t x = 0, y = 0; };

// Fits an image into a destination without changing its aspect ratio.
[[nodiscard]] VisualPreviewRect fitVisualPreview(VisualPreviewRect area,
    std::size_t imageWidth, std::size_t imageHeight);
// Maps a point inside the fitted rectangle to the corresponding source pixel.
// The right and bottom boundaries are exclusive.
[[nodiscard]] bool visualPreviewSourcePoint(VisualPreviewRect fitted,
    std::size_t imageWidth, std::size_t imageHeight, double x, double y,
    VisualPreviewPoint& point);

}
