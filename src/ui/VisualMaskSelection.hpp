#pragma once
#include "ui/VisualReviewCrop.hpp"
namespace lucid::ui {
// Fitted overview pane in logical UI coordinates, matching visual review layout.
[[nodiscard]] VisualPreviewRect visualReviewPaneRect(double width, double height, std::size_t pane, std::size_t imageWidth, std::size_t imageHeight);
// Maps a drag inside the fitted image to an inclusive source-pixel mask. Both
// endpoints must lie inside; reverse drags and a one-pixel click are supported.
// `visible` is the source region represented by the fitted image, allowing
// overview and cropped previews. No files or renderer state are touched.
[[nodiscard]] bool visualMaskFromDrag(VisualPreviewRect fitted, std::size_t sourceWidth,
    std::size_t sourceHeight, VisualReviewCrop visible, double startX, double startY,
    double endX, double endY, modules::VisualMask& result);
// Maps the intersection of a mask and visible source region back to the fitted
// image. Returns false for invalid input or no overlap. The rectangle describes
// pixel boundaries, not pixel centers.
[[nodiscard]] bool visualMaskOverlay(VisualPreviewRect fitted, std::size_t sourceWidth,
    std::size_t sourceHeight, VisualReviewCrop visible, const modules::VisualMask& mask,
    VisualPreviewRect& result);
}
