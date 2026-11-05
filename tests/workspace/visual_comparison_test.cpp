#include "modules/VisualComparison.hpp"
#include <limits>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <atomic>
#include "workspace/Project.hpp"
using namespace lucid::modules;
int main() {
    VisualImage a{3, 2, std::vector<std::uint8_t>(24, 0)}, b = a;
    b.rgba[0] = 10; b.rgba[4] = 11; b.rgba[11] = 20; b.rgba[12] = 255;
    VisualComparison result; std::string error;
    VisualReviewConfig config; std::string configJson;
    bool configOk = parseVisualReviewConfig(R"({"version":1,"threshold":10,"masks":[{"x":1,"y":0,"width":2,"height":1}]})", config, error)
        && config.threshold == 10 && config.masks.size() == 1 && validateVisualReviewConfig(config, 3, 2, error)
        && serializeVisualReviewConfig(config, configJson, error);
    VisualReviewConfig roundTrip;
    configOk = configOk && parseVisualReviewConfig(configJson, roundTrip, error) && roundTrip.threshold == 10 && roundTrip.masks[0].width == 2;
    configOk = configOk && !validateVisualReviewConfig(config, 2, 2, error);
    for (const auto* invalid : {R"({"version":1,"threshold":256,"masks":[]})", R"({"version":1,"threshold":-1,"masks":[]})",
                                R"({"version":1,"threshold":1.5,"masks":[]})", R"({"version":1,"threshold":0,"threshold":1,"masks":[]})",
                                R"({"version":1,"threshold":0,"masks":[{"x":0,"y":0,"width":0,"height":1}]})",
                                R"({"version":1,"threshold":0,"masks":[{"x":0,"x":1,"y":0,"width":1,"height":1}]})",
                                R"({"version":1,"threshold":0,"masks":[],"ignored":true})"}) {
        configOk = configOk && !parseVisualReviewConfig(invalid, roundTrip, error) && roundTrip.masks.empty() && !error.empty();
    }
    configOk = configOk && !parseVisualReviewConfig(std::string(65537, ' '), roundTrip, error);
    if (!configOk) return 2;
    bool ok = compareVisualImages(a, b, 10, {}, result, error) && result.changedPixels == 3 && result.comparedPixels == 6 && result.maskedPixels == 0;
    ok = ok && result.diff.rgba[3] == 0 && result.diff.rgba[4] == 255 && result.diff.rgba[7] == 255 && result.diff.rgba[11] == 255;
    ok = ok && compareVisualImages(a, b, 10, {{1, 0, 2, 1}, {2, 0, 1, 2}}, result, error) && result.maskedPixels == 3 && result.comparedPixels == 3 && result.changedPixels == 1;
    ok = ok && compareVisualImages(a, b, 255, {}, result, error) && result.changedPixels == 0;
    ok = ok && !compareVisualImages(a, b, 0, {{2, 1, 2, 1}}, result, error) && result.diff.rgba.empty();
    ok = ok && !compareVisualImages(a, b, 0, {{0, 0, 0, 1}}, result, error);
    b.width = 4; ok = ok && !compareVisualImages(a, b, 0, {}, result, error); b = a; b.rgba.pop_back(); ok = ok && !compareVisualImages(a, b, 0, {}, result, error);
    VisualImage tall{2,3,std::vector<std::uint8_t>(24,0)}, wide{3,2,std::vector<std::uint8_t>(24,0)};
    ok = ok && compareVisualImages(tall,wide,255,{},result,error) && result.diff.width==3 && result.diff.height==3 && result.comparedPixels==8 && result.changedPixels==4 && result.addedPixels==2 && result.removedPixels==2 && result.diff.rgba[(2*3+2)*4+3]==0;
    ok = ok && compareVisualImages(tall,wide,0,{{0,0,1,1}},result,error) && result.maskedPixels==1 && result.comparedPixels==7 && result.changedPixels==4;
    ok = ok && !compareVisualImages(tall,wide,0,{{2,0,1,1}},result,error);
    VisualImage huge{std::numeric_limits<std::size_t>::max(), 2, {}}; ok = ok && !compareVisualImages(huge, huge, 0, {}, result, error);
    ok = ok && !compareVisualImages(a, a, 0, std::vector<VisualMask>(257, {0, 0, 1, 1}), result, error);
    std::atomic_bool compareCancel{true};
    ok = ok && !compareVisualImages(a, a, 0, {}, result, error, &compareCancel) && result.diff.rgba.empty() && error.find("cancelled") != std::string::npos;
    const unsigned char red[] = {0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x08,0x06,0x00,0x00,0x00,0x1f,0x15,0xc4,0x89,0x00,0x00,0x00,0x0d,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0xf8,0xcf,0xc0,0xf0,0x1f,0x00,0x05,0x00,0x01,0xff,0x89,0x99,0x3d,0x1d,0x00,0x00,0x00,0x00,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82};
    const unsigned char blue[] = {0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x08,0x06,0x00,0x00,0x00,0x1f,0x15,0xc4,0x89,0x00,0x00,0x00,0x0d,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0x60,0x60,0xf8,0xff,0x1f,0x00,0x03,0x02,0x01,0xff,0xe6,0x77,0x0b,0xae,0x00,0x00,0x00,0x00,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82};
    const unsigned char blueWide[] = {0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x01,0x08,0x06,0x00,0x00,0x00,0xf4,0x22,0x7f,0x8a,0x00,0x00,0x00,0x0e,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0x60,0x60,0xf8,0xff,0x1f,0x84,0x01,0x0d,0xfb,0x03,0xfd,0x87,0x5e,0xd5,0x87,0x00,0x00,0x00,0x00,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82};
    const std::string redBytes(reinterpret_cast<const char*>(red), sizeof(red)), blueBytes(reinterpret_cast<const char*>(blue), sizeof(blue));
    VisualImage decoded; ok = ok && decodeVisualPng(redBytes, decoded, error) && decoded.width == 1 && decoded.height == 1 && decoded.rgba == std::vector<std::uint8_t>({255, 0, 0, 255});
    ok = ok && !decodeVisualPng("not a PNG", decoded, error) && decoded.rgba.empty();
    auto damaged = redBytes; damaged[45] ^= 1;
    ok = ok && !decodeVisualPng(damaged, decoded, error) && decoded.rgba.empty();
    ok = ok && !decodeVisualPng(redBytes.substr(0, redBytes.size() - 1), decoded, error);
    ok = ok && !decodeVisualPng(std::string(16U * 1024U * 1024U + 1, 'x'), decoded, error) && decoded.rgba.empty();
    auto oversized = redBytes; oversized[16] = 0x7f; oversized[17] = static_cast<char>(0xff); oversized[18] = static_cast<char>(0xff); oversized[19] = static_cast<char>(0xff);
    ok = ok && !decodeVisualPng(oversized, decoded, error);
    namespace fs = std::filesystem;
    const auto dir = fs::temp_directory_path() / ("lucid-visual-review-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);
    auto write = [&](const char* name, const std::string& value) { std::ofstream f(dir / name, std::ios::binary | std::ios::trunc); f.write(value.data(), static_cast<std::streamsize>(value.size())); };
    write("baseline.png", redBytes); write("candidate.png", blueBytes);
    bool created = false; auto project = lucid::workspace::Project::openOrCreate(dir, created, error, false, false); VisualReview review;
    const std::u8string unicodeBase = u8"图像/basé.png", unicodeCandidate = u8"图像/candidat-é.png";
    const std::string baseName(unicodeBase.begin(), unicodeBase.end()), candidateName(unicodeCandidate.begin(), unicodeCandidate.end());
    ok = ok && project.writeFile(baseName, redBytes, error, nullptr, true) && project.writeFile(candidateName, blueBytes, error, nullptr, true) &&
        review.open(project, baseName, candidateName, 0, {}, error) && review.comparison().changedPixels == 1 && review.approve(project, error);
    write("review.json", R"({"version":1,"threshold":0,"masks":[{"x":0,"y":0,"width":1,"height":1}]})");
    VisualReviewConfig loaded;
    ok = ok && error.empty() && loadVisualReviewConfig(project, "review.json", loaded, error) && loaded.masks.size() == 1;
    ok = ok && review.openConfigured(project, "baseline.png", "candidate.png", "review.json", error)
        && review.comparison().maskedPixels == 1 && review.comparison().changedPixels == 0;
    ok = ok && !review.openConfigured(project, "baseline.png", "candidate.png", "../review.json", error)
        && !review.approve(project, error);
    write("review.json", R"({"version":1,"threshold":0,"masks":[{"x":1,"y":0,"width":1,"height":1}]})");
    ok = ok && !review.openConfigured(project, "baseline.png", "candidate.png", "review.json", error);
    write("review.json", std::string(65537, 'x'));
    ok = ok && !loadVisualReviewConfig(project, "review.json", loaded, error) && loaded.masks.empty();
    std::atomic_bool configCancel{true};
    ok = ok && !loadVisualReviewConfig(project, "review.json", loaded, error, &configCancel) && error.find("cancelled") != std::string::npos;
    std::error_code configEc; fs::create_symlink(dir / "review.json", dir / "review-link.json", configEc);
    if (!configEc) ok = ok && !loadVisualReviewConfig(project, "review-link.json", loaded, error);
    ok = ok && review.open(project, "baseline.png", "candidate.png", 0, {}, error) && review.comparison().changedPixels == 1;
    std::atomic_bool cancel{true}; ok = ok && review.screenshotsStillCurrent(project,error) && !review.screenshotsStillCurrent(project,error,&cancel) && !review.approve(project, error, &cancel);
    write("candidate.png", redBytes); ok = ok && !review.screenshotsStillCurrent(project,error) && error.find("Candidate changed") != std::string::npos && !review.approve(project, error) && error.find("Candidate changed") != std::string::npos;
    write("candidate.png", blueBytes); write("baseline.png", blueBytes); ok = ok && !review.approve(project, error);
    write("baseline.png", redBytes); ok = ok && review.approve(project, error);
    std::string saved; ok = ok && project.readFile("baseline.png", saved, error) && saved == blueBytes && !review.approve(project, error);
    const std::string wideBytes(reinterpret_cast<const char*>(blueWide), sizeof(blueWide));
    write("candidate.png",wideBytes);
    ok = ok && review.open(project,"baseline.png","candidate.png",0,{},error) && review.comparison().addedPixels==1 && review.comparison().removedPixels==0 && review.comparison().changedPixels==1 && review.approve(project,error) && project.readFile("baseline.png",saved,error) && saved==wideBytes;
    ok = ok && !review.open(project, "../baseline.png", "candidate.png", 0, {}, error);
    std::error_code ec; fs::create_symlink(dir / "candidate.png", dir / "link.png", ec);
    if (!ec) ok = ok && !review.open(project, "baseline.png", "link.png", 0, {}, error);
    ec.clear(); fs::create_hard_link(dir / "baseline.png", dir / "alias.png", ec);
    if (!ec) ok = ok && !review.open(project, "baseline.png", "alias.png", 0, {}, error) && error.find("different") != std::string::npos;
    cancel.store(true);
    ok = ok && !review.open(project, "baseline.png", "candidate.png", 0, {}, error, &cancel) && error.find("cancelled") != std::string::npos;
    fs::remove_all(dir);
    return ok ? 0 : 1;
}
