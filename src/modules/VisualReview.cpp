#include "modules/VisualComparison.hpp"
#include "workspace/Project.hpp"
#include <filesystem>
#include <exception>
#include <cstdlib>
#include <memory>
#include <utility>
#include <array>
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
// Cap decoder allocations too: PNG dimensions alone do not limit malicious
// compressed data that expands beyond the expected number of scanlines.
namespace {
constexpr std::size_t kVisualDecodeAllocationLimit = 160U * 1024U * 1024U;
void* visualDecodeMalloc(std::size_t size) { return size <= kVisualDecodeAllocationLimit ? std::malloc(size) : nullptr; }
void* visualDecodeRealloc(void* ptr, std::size_t size) { return size <= kVisualDecodeAllocationLimit ? std::realloc(ptr, size) : nullptr; }
}
#define STBI_MALLOC(size) visualDecodeMalloc(size)
#define STBI_REALLOC(ptr, size) visualDecodeRealloc(ptr, size)
#define STBI_FREE(ptr) std::free(ptr)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace lucid::modules {
namespace {
constexpr std::size_t maxEncoded = 16U * 1024U * 1024U;
constexpr std::size_t maxPixels = 16U * 1024U * 1024U;
// stb_image does not validate PNG chunk CRCs. Reject damaged screenshots rather
// than allowing approval of bytes that other PNG decoders may reject.
bool validPngChunks(std::string_view bytes) {
    static constexpr std::array<unsigned char, 8> signature{137, 80, 78, 71, 13, 10, 26, 10};
    if (bytes.size() < signature.size()) return false;
    for (std::size_t i = 0; i < signature.size(); ++i)
        if (static_cast<unsigned char>(bytes[i]) != signature[i]) return false;
    const auto read32 = [&](std::size_t offset) {
        std::uint32_t value = 0;
        for (std::size_t j = 0; j < 4; ++j) value = (value << 8) | static_cast<unsigned char>(bytes[offset + j]);
        return value;
    };
    static constexpr auto crcTable = [] {
        std::array<std::uint32_t, 256> table{};
        for (std::uint32_t i = 0; i < table.size(); ++i) {
            auto crc = i;
            for (int j = 0; j < 8; ++j) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
            table[i] = crc;
        }
        return table;
    }();
    bool header = false, data = false;
    for (std::size_t offset = 8; offset < bytes.size();) {
        if (bytes.size() - offset < 12) return false;
        const auto length = static_cast<std::size_t>(read32(offset));
        if (length > bytes.size() - offset - 12) return false;
        const auto type = bytes.substr(offset + 4, 4);
        if (!header) { if (type != "IHDR" || length != 13) return false; header = true; }
        else if (type == "IHDR") return false;
        std::uint32_t crc = 0xffffffffU;
        for (std::size_t i = offset + 4; i < offset + 8 + length; ++i)
            crc = (crc >> 8) ^ crcTable[(crc ^ static_cast<unsigned char>(bytes[i])) & 0xffU];
        if ((crc ^ 0xffffffffU) != read32(offset + 8 + length)) return false;
        if (type == "IDAT") data = true;
        offset += length + 12;
        if (type == "IEND") return data && length == 0 && offset == bytes.size();
    }
    return false;
}
bool safePath(const workspace::Project& project, std::string_view name, std::string& error) {
    std::filesystem::path relative;
    try { relative = std::filesystem::path(std::u8string(name.begin(), name.end())); }
    catch (const std::exception&) { error = "Invalid UTF-8 project path"; return false; }
    if (name.empty() || name.find('\0') != std::string_view::npos || relative.is_absolute() || relative.has_root_name()) { error = "Invalid project path"; return false; }
    auto path = project.root();
    for (const auto& component : relative) {
        if (component == ".." || component == "." || component.empty()) { error = "Invalid project path"; return false; }
        path /= component; std::error_code ec; const auto status = std::filesystem::symlink_status(path, ec);
        if (ec || !std::filesystem::exists(status) || std::filesystem::is_symlink(status)) { error = "File is missing or path contains a symbolic link"; return false; }
    }
    std::string resolveError; (void)project.resolve(name, resolveError);
    if (!resolveError.empty()) { error = resolveError; return false; }
    return true;
}
}
bool loadVisualReviewConfig(const workspace::Project& project, std::string_view path,
                            VisualReviewConfig& config, std::string& error, const std::atomic_bool* cancel) {
    config = {}; error.clear();
    if (cancel && cancel->load()) { error = "Visual configuration read cancelled"; return false; }
    std::string contents;
    if (!safePath(project, path, error) || !project.readFileBounded(path, contents, error, 65536, cancel) ||
        !safePath(project, path, error) || !parseVisualReviewConfig(contents, config, error)) {
        config = {};
        if (cancel && cancel->load()) error = "Visual configuration read cancelled";
        return false;
    }
    return true;
}
bool decodeVisualPng(std::string_view bytes, VisualImage& image, std::string& error) {
    image = {}; error.clear();
    if (bytes.empty() || bytes.size() > maxEncoded) { error = "PNG exceeds size limit or is empty"; return false; }
    if (!validPngChunks(bytes)) { error = "PNG has invalid or damaged chunks"; return false; }
    const auto* data = reinterpret_cast<const stbi_uc*>(bytes.data()); const auto size = static_cast<int>(bytes.size());
    int width = 0, height = 0, channels = 0;
    if (!stbi_info_from_memory(data, size, &width, &height, &channels) || width <= 0 || height <= 0 ||
        static_cast<std::size_t>(width) > maxPixels / static_cast<std::size_t>(height)) { error = "Invalid PNG or excessive dimensions"; return false; }
    using Pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;
    Pixels pixels(stbi_load_from_memory(data, size, &width, &height, &channels, 4), stbi_image_free);
    if (!pixels || width <= 0 || height <= 0 || static_cast<std::size_t>(width) > maxPixels / static_cast<std::size_t>(height)) { error = "Could not decode PNG"; return false; }
    image.width = static_cast<std::size_t>(width); image.height = static_cast<std::size_t>(height);
    image.rgba.assign(pixels.get(), pixels.get() + image.width * image.height * 4); return true;
}
bool VisualReview::openConfigured(const workspace::Project& project, std::string_view baseline, std::string_view candidate,
                                  std::string_view configPath, std::string& error, const std::atomic_bool* cancel) {
    // Invalidate an older review before any configuration operation can fail.
    opened_ = false; baselineImage_ = {}; candidateImage_ = {}; comparison_ = {}; baselineBytes_.clear(); candidateBytes_.clear();
    VisualReviewConfig config;
    if (!loadVisualReviewConfig(project, configPath, config, error, cancel)) return false;
    return open(project, baseline, candidate, config.threshold, config.masks, error, cancel);
}
bool VisualReview::open(const workspace::Project& project, std::string_view baseline, std::string_view candidate,
                        std::uint8_t threshold, const std::vector<VisualMask>& masks,
                        std::string& error, const std::atomic_bool* cancel) {
    opened_ = false; baselineImage_ = {}; candidateImage_ = {}; comparison_ = {}; baselineBytes_.clear(); candidateBytes_.clear(); error.clear();
    if (baseline == candidate) { error = "Choose two different screenshots"; return false; }
    if (!safePath(project, baseline, error) || !safePath(project, candidate, error)) return false;
    // Different spellings, case aliases and hard links can refer to the same
    // file; never present that as an independent candidate and baseline.
    std::error_code ec;
    std::string resolveError;
    const auto baselineFile = project.resolve(baseline, resolveError);
    if (baselineFile.empty()) { error = resolveError; return false; }
    const auto candidateFile = project.resolve(candidate, resolveError);
    if (candidateFile.empty()) { error = resolveError; return false; }
    if (std::filesystem::equivalent(baselineFile, candidateFile, ec)) {
        error = "Choose two different screenshots"; return false;
    }
    if (ec) { error = "Could not compare screenshot identities"; return false; }
    if ((cancel && cancel->load()) ||
        !project.readFileBounded(baseline, baselineBytes_, error, maxEncoded, cancel) ||
        !project.readFileBounded(candidate, candidateBytes_, error, maxEncoded, cancel) ||
        !safePath(project, baseline, error) || !safePath(project, candidate, error) ||
        !decodeVisualPng(baselineBytes_, baselineImage_, error) || !decodeVisualPng(candidateBytes_, candidateImage_, error) ||
        !compareVisualImages(baselineImage_, candidateImage_, threshold, masks, comparison_, error, cancel)) {
        if (cancel && cancel->load()) error = "Visual comparison cancelled";
        return false;
    }
    root_ = project.root(); baselinePath_ = baseline; candidatePath_ = candidate; opened_ = true; return true;
}
bool VisualReview::screenshotsStillCurrent(const workspace::Project& project,std::string& error,const std::atomic_bool* cancel) const {
    error.clear(); if(!opened_ || root_!=project.root()) {error="Visual review is no longer open";return false;}
    if(cancel && cancel->load()) {error="Visual review check cancelled";return false;}
    if(!safePath(project,baselinePath_,error) || !safePath(project,candidatePath_,error)) return false;
    std::string current;
    if(!project.readFileBounded(baselinePath_,current,error,maxEncoded,cancel)) return false;
    if(current!=baselineBytes_) {error="Baseline changed; compare again";return false;}
    if(!project.readFileBounded(candidatePath_,current,error,maxEncoded,cancel)) return false;
    if(current!=candidateBytes_) {error="Candidate changed; compare again";return false;}
    if(cancel && cancel->load()) {error="Visual review check cancelled";return false;}
    return true;
}
bool VisualReview::approve(workspace::Project& project, std::string& error, const std::atomic_bool* cancel) {
    error.clear();
    if (!opened_ || root_ != project.root()) { error = "No visual review is open for this project"; return false; }
    if (cancel && cancel->load()) { error = "Approval cancelled"; return false; }
    if (!safePath(project, baselinePath_, error) || !safePath(project, candidatePath_, error)) return false;
    std::string current;
    if (!project.readFileBounded(candidatePath_, current, error, maxEncoded, cancel)) return false;
    if (current != candidateBytes_) { error = "Candidate changed since review; compare again"; return false; }
    if (cancel && cancel->load()) { error = "Approval cancelled"; return false; }
    if (!project.writeFile(baselinePath_, candidateBytes_, error, &baselineBytes_, false, cancel)) return false;
    baselineBytes_ = candidateBytes_; baselineImage_ = candidateImage_; opened_ = false; return true;
}
}
