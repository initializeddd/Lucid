#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <atomic>
#include <vector>
namespace lucid::workspace { class Project; }
namespace lucid::modules {
struct VisualImage {
    std::size_t width = 0, height = 0;
    // Straight, unpremultiplied RGBA8 in row-major order.
    std::vector<std::uint8_t> rgba;
};
struct VisualMask { std::size_t x = 0, y = 0, width = 0, height = 0; };
struct VisualReviewConfig { std::uint8_t threshold = 0; std::vector<VisualMask> masks; };
// Portable JSON configuration: {"version":1,"threshold":0..255,
// "masks":[{"x":0,"y":0,"width":1,"height":1}]}. Parsing does not
// read or write files. Call validateVisualReviewConfig with image dimensions
// before using a configuration, since masks can only be validated against an image.
[[nodiscard]] bool parseVisualReviewConfig(std::string_view json, VisualReviewConfig& config, std::string& error);
[[nodiscard]] bool serializeVisualReviewConfig(const VisualReviewConfig& config, std::string& json, std::string& error);
[[nodiscard]] bool validateVisualReviewConfig(const VisualReviewConfig& config, std::size_t width, std::size_t height, std::string& error);
// Reads one project-relative config snapshot, at most 64 KiB, without writing.
// Rejects symbolic-link components. As with other project reads, it cannot
// prevent a separate process from changing the path between checks and open.
[[nodiscard]] bool loadVisualReviewConfig(const workspace::Project& project, std::string_view path,
    VisualReviewConfig& config, std::string& error, const std::atomic_bool* cancel = nullptr);
struct VisualComparison {
    std::size_t comparedPixels = 0, maskedPixels = 0, changedPixels = 0;
    // Counts of pixels present on only one side; both are also included in
    // changedPixels and comparedPixels. The canvas is aligned at top left.
    std::size_t addedPixels = 0, removedPixels = 0;
    // Transparent for equal/masked pixels, opaque red for changed pixels.
    VisualImage diff;
};
// Images align at top left. Added and removed pixels always count as changed,
// regardless of threshold or alpha. Each mask must fit inside both images.
// A pixel changes if any RGBA channel differs by strictly more than threshold.
// Transparent pixels' RGB values are compared too; no image decoding or I/O occurs.
[[nodiscard]] bool compareVisualImages(const VisualImage& baseline, const VisualImage& actual,
    std::uint8_t threshold, const std::vector<VisualMask>& masks,
    VisualComparison& result, std::string& error, const std::atomic_bool* cancel = nullptr);

// Decode a PNG snapshot without filesystem access. Compressed input is limited
// to 16 MiB and decoded dimensions to 16 million pixels.
[[nodiscard]] bool decodeVisualPng(std::string_view bytes, VisualImage& image, std::string& error);

// Captures the exact source bytes being reviewed. Approval checks both files
// against these snapshots and writes the reviewed candidate bytes to the baseline.
// These checks cannot prevent another process changing a file between the final
// check and replacement. Approval is an explicit disk write.
class VisualReview {
public:
    [[nodiscard]] bool open(const workspace::Project& project, std::string_view baseline, std::string_view candidate,
                            std::uint8_t threshold, const std::vector<VisualMask>& masks,
                            std::string& error, const std::atomic_bool* cancel = nullptr);
    // Reads the configuration first, then applies it to the compared snapshots.
    // A failure invalidates any prior review and never writes to disk.
    [[nodiscard]] bool openConfigured(const workspace::Project& project, std::string_view baseline, std::string_view candidate,
                                      std::string_view configPath, std::string& error, const std::atomic_bool* cancel = nullptr);
    // Rechecks the reviewed screenshot bytes without writing. External changes
    // may still race the check; callers must revalidate their own UI context.
    [[nodiscard]] bool screenshotsStillCurrent(const workspace::Project& project, std::string& error, const std::atomic_bool* cancel = nullptr) const;
    [[nodiscard]] bool approve(workspace::Project& project, std::string& error, const std::atomic_bool* cancel = nullptr);
    [[nodiscard]] const VisualComparison& comparison() const noexcept { return comparison_; }
    [[nodiscard]] const VisualImage& baselineImage() const noexcept { return baselineImage_; }
    [[nodiscard]] const VisualImage& candidateImage() const noexcept { return candidateImage_; }
private:
    std::filesystem::path root_;
    std::string baselinePath_, candidatePath_, baselineBytes_, candidateBytes_;
    VisualImage baselineImage_, candidateImage_;
    VisualComparison comparison_;
    bool opened_ = false;
};
}
