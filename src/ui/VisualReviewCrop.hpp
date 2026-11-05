#pragma once
#include "ui/VisualReviewPreview.hpp"
#include <atomic>
namespace lucid::ui {
struct VisualReviewCrop { std::size_t x=0, y=0, width=0, height=0; };
// Extracts exactly one source pixel per output pixel from the compared images.
// Crop dimensions are capped at 1024x1024; no scaling, disk, network or GPU is
// involved. Transparent pixels are composited over the same checkerboard used
// by the overview; difference markers preserve the reviewed comparison's mask
// and threshold. Returns no partial result on errors or cancellation.
[[nodiscard]] bool makeVisualReviewCrop(const modules::VisualImage& baseline,
    const modules::VisualImage& candidate, const modules::VisualComparison& comparison,
    const VisualReviewCrop& crop, VisualReviewPreview& preview, std::string& error,
    const std::atomic_bool* cancel=nullptr);
}
