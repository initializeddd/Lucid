#include "ui/VisualReviewPreview.hpp"
#include <cmath>
#include <limits>
#include <string>
using namespace lucid;
int main() {
    modules::VisualImage before{2, 2, {255, 0, 0, 255, 0, 255, 0, 255,
                                       0, 0, 255, 255, 0, 0, 0, 0}};
    auto after = before; after.rgba[0] = 0;
    modules::VisualComparison comparison; comparison.diff = {2, 2, {255, 0, 0, 255, 0, 0, 0, 0,
                                                                    0, 0, 0, 0, 0, 0, 0, 0}};
    ui::VisualReviewPreview preview; std::string error;
    if (!ui::makeVisualReviewPreview(before, after, comparison, 1, 1, preview, error)) return 1;
    if (preview.baseline.width != 1 || preview.baseline.height != 1 || preview.baseline.rgba != std::vector<std::uint8_t>({119, 119, 119, 255})) return 2;
    if (preview.candidate.rgba != std::vector<std::uint8_t>({56, 119, 119, 255}) || preview.difference.rgba[0] <= preview.candidate.rgba[0] || preview.difference.rgba[1] >= preview.candidate.rgba[1]) return 3;
    if (!ui::makeVisualReviewPreview(before, after, comparison, 4, 4, preview, error) || preview.baseline.width != 2 || preview.baseline.rgba[12] != 224 || preview.difference.rgba[4] != preview.candidate.rgba[4]) return 4;
    if (ui::makeVisualReviewPreview(before, after, comparison, 0, 1, preview, error) || !preview.baseline.rgba.empty() || error.empty()) return 5;
    after.rgba.pop_back();
    if (ui::makeVisualReviewPreview(before, after, comparison, 2, 2, preview, error)) return 6;
    auto fitted = ui::fitVisualPreview({10, 20, 300, 100}, 200, 100);
    if (fitted.x != 60 || fitted.y != 20 || fitted.width != 200 || fitted.height != 100) return 7;
    ui::VisualPreviewPoint point;
    if (!ui::visualPreviewSourcePoint(fitted, 200, 100, 259.9, 119.9, point) || point.x != 199 || point.y != 99) return 8;
    if (ui::visualPreviewSourcePoint(fitted, 200, 100, 260, 100, point) || ui::visualPreviewSourcePoint(fitted, 200, 100, std::numeric_limits<double>::quiet_NaN(), 100, point)) return 9;
    if (ui::fitVisualPreview({0, 0, -1, 10}, 1, 1).width != 0) return 10;
    modules::VisualImage large{600, 400, std::vector<std::uint8_t>(600 * 400 * 4, 255)};
    modules::VisualComparison largeComparison; largeComparison.diff = {600, 400, std::vector<std::uint8_t>(600 * 400 * 4, 0)};
    largeComparison.diff.rgba[(199 * 600 + 299) * 4 + 3] = 255;
    if (!ui::makeVisualReviewPreview(large, large, largeComparison, 1024, 1024, preview, error) || preview.baseline.width != 600 || preview.baseline.height != 400 || !preview.generation || preview.difference.rgba[(199 * 600 + 299) * 4 + 1] >= 255) return 11;
    const auto generation = preview.generation;
    if (!ui::makeVisualReviewPreview(large, large, largeComparison, 300, 300, preview, error) || preview.baseline.width != 300 || preview.baseline.height != 200 || preview.generation == generation || preview.difference.rgba[(99 * 300 + 149) * 4 + 1] >= 255) return 12;
    if (ui::makeVisualReviewPreview(large, large, largeComparison, 1025, 1024, preview, error)) return 13;
    modules::VisualImage tall{1,2,{255,0,0,255,255,0,0,255}}, wide{2,1,{0,255,0,255,0,255,0,255}};
    modules::VisualComparison unequal; if(!modules::compareVisualImages(tall,wide,0,{},unequal,error)) return 14;
    if(!ui::makeVisualReviewPreview(tall,wide,unequal,2,2,preview,error) || preview.baseline.width!=2 || preview.baseline.height!=2 || preview.baseline.rgba[4]!=224 || preview.candidate.rgba[8]!=224 || preview.difference.rgba[7]!=255) return 15;
    return 0;
}
