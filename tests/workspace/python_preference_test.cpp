#include "modules/PythonPreference.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
using namespace lucid::modules;
int main() {
    namespace fs = std::filesystem; std::error_code ec;
    auto root = fs::canonical(fs::temp_directory_path()) / ("lucid-pref-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto settings = root / "settings", a = root / "prójéct", b = root / "other";
    fs::create_directories(settings); fs::create_directories(a); fs::create_directories(b);
    PythonPreference pa, pb, stale; std::string error;
    if (!loadPythonPreference(settings, a, pa, error) || pa.exists || !loadPythonPreference(settings, b, pb, error) || pa.filename == pb.filename) return 1;
    auto python = fs::path("/tmp") / "python-é";
    if (!savePythonPreference(settings, pa, python, error) || !loadPythonPreference(settings, a, stale, error) || stale.interpreter != python) return 2;
    if (!savePythonPreference(settings, pa, {}, error) || savePythonPreference(settings, stale, python, error)) return 3;
    if (!loadPythonPreference(settings, a, stale, error) || !stale.interpreter.empty()) return 4;
    if (savePythonPreference(settings, pb, fs::path("relative"), error)) return 5;
    { std::ofstream file(settings / pb.filename); file << "{\"project\":\"other-project\",\"interpreter\":\"\"}"; }
    if (savePythonPreference(settings, pb, python, error) || loadPythonPreference(settings, b, pb, error)) return 6;
    fs::remove(settings / pa.filename); fs::create_symlink(root / "outside", settings / pa.filename, ec);
    if (!ec && loadPythonPreference(settings, a, stale, error)) return 7;
    auto alias = root / "settings-link"; fs::create_directory_symlink(settings, alias, ec);
    if (!ec && loadPythonPreference(alias, a, stale, error)) return 8;
    fs::remove(settings / pa.filename, ec);
    if (!loadPythonPreference(settings, a, stale, error)) return 9;
    if (!savePythonPreference(settings, stale, python, error)) return 10;
    { auto malformed = stale.baseline; const auto end = malformed.find_last_of('}'); malformed.insert(end, ",\"interpreter\":\"/tmp/other\""); std::ofstream f(settings / stale.filename, std::ios::binary | std::ios::trunc); f << malformed; }
    PythonPreference invalid; if (loadPythonPreference(settings, a, invalid, error)) return 14;
    { std::ofstream f(settings / stale.filename, std::ios::binary | std::ios::trunc); f << stale.baseline; }
    auto otherSettings = root / "settings2"; fs::create_directory(otherSettings);
    if (savePythonPreference(otherSettings, stale, {}, error)) return 11;
    if (savePythonPreference(settings, stale, fs::path(std::string("/tmp/x\0bad", 10)), error)) return 12;
#if !defined(_WIN32)
    fs::permissions(root, fs::perms::group_write, fs::perm_options::add, ec);
    if (!ec && loadPythonPreference(settings, a, stale, error)) return 13;
    fs::permissions(root, fs::perms::group_write, fs::perm_options::remove, ec);
#endif
    fs::remove_all(root); return 0;
}
