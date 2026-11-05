#include "modules/CMakeModel.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
int main() {
    namespace fs = std::filesystem; using namespace lucid::modules;
    const auto root = fs::temp_directory_path() / ("lucid-cmake-cache-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "build");
    const auto cache = root / "build" / "CMakeCache.txt";
    const auto put = [&](const std::string& value) { std::ofstream f(cache, std::ios::binary | std::ios::trunc); f << value; };
    const auto get = [&]() { std::ifstream f(cache, std::ios::binary); return std::string(std::istreambuf_iterator<char>(f), {}); };
    bool created = false; std::string error; auto project = lucid::workspace::Project::openOrCreate(root, created, error);
    CMakeCacheUpdate update; const std::string original = "// Comment\r\nFEATURE:BOOL=OFF\r\nOTHER:STRING=x\r\n";
    put(original);
    if (!prepareCMakeBoolCacheUpdate(project, "build/CMakeCache.txt", "FEATURE", "OFF", true, update, error) || update.replacement != "// Comment\r\nFEATURE:BOOL=ON\r\nOTHER:STRING=x\r\n") return 1;
    put(original + "# concurrent\r\n");
    if (saveCMakeBoolCacheUpdate(project, update, error) || get() != original + "# concurrent\r\n") return 2;
    put(original);
    if (!prepareCMakeBoolCacheUpdate(project, "build/CMakeCache.txt", "FEATURE", "OFF", true, update, error) || !saveCMakeBoolCacheUpdate(project, update, error) || get() != update.replacement) return 3;
    put(original + "FEATURE:BOOL=OFF\r\n");
    if (prepareCMakeBoolCacheUpdate(project, "build/CMakeCache.txt", "FEATURE", "OFF", true, update, error)) return 4;
    put("FEATURE:STRING=OFF\n");
    if (prepareCMakeBoolCacheUpdate(project, "build/CMakeCache.txt", "FEATURE", "OFF", true, update, error)) return 5;
    put(original);
    if (prepareCMakeBoolCacheUpdate(project, "build/CMakeCache.txt", "FEATURE", "ON", true, update, error) || prepareCMakeBoolCacheUpdate(project, "../CMakeCache.txt", "FEATURE", "OFF", true, update, error)) return 6;
    const std::u8string unicodeDirectory = u8"構築-é";
    const std::string unicodeRelative(unicodeDirectory.begin(), unicodeDirectory.end());
    const auto unicodeCache = root / fs::path(unicodeDirectory) / "CMakeCache.txt";
    fs::create_directories(unicodeCache.parent_path());
    { std::ofstream f(unicodeCache, std::ios::binary); f << original; }
    if (!prepareCMakeBoolCacheUpdate(project, unicodeRelative + "/CMakeCache.txt", "FEATURE", "OFF", true, update, error) ||
        !saveCMakeBoolCacheUpdate(project, update, error)) return 9;
    std::ifstream unicodeInput(unicodeCache, std::ios::binary);
    const std::string unicodeContents((std::istreambuf_iterator<char>(unicodeInput)), {});
    if (unicodeContents != "// Comment\r\nFEATURE:BOOL=ON\r\nOTHER:STRING=x\r\n") return 10;
    const auto reply = root / "bad" / ".cmake/api/v1/reply";
    fs::create_directories(reply);
    { std::ofstream f(reply / "index-1.json"); f << R"({"objects":[{"kind":"codemodel","version":{"major":2},"jsonFile":"foo\u0000bar.json"}]})"; }
    CMakeModel model;
    if (readCMakeModel(root / "bad", model, error) || error.find("Unsafe") == std::string::npos) return 8;
    std::error_code ec; fs::remove(cache, ec); fs::create_symlink(root / "elsewhere", cache, ec);
    if (!ec && prepareCMakeBoolCacheUpdate(project, "build/CMakeCache.txt", "FEATURE", "OFF", true, update, error)) return 7;
    fs::remove_all(root); return 0;
}
