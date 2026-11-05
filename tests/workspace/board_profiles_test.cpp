#include "modules/BoardProfiles.hpp"
#include "modules/OpenOcdFlash.hpp"
#include <chrono>
#include <cstdlib>
#include <fstream>
int main() {
    using lucid::modules::parseBoardProfile;
    auto r=parseBoardProfile("# board\nsource interface/cmsis-dap.cfg\nsource target/stm32.cfg\nadapter_khz 4000\nfirmware build/app.elf\nreset halt\n");
    if (!r.error.empty() || r.incomplete || r.interfaceConfig!="interface/cmsis-dap.cfg" || r.targetConfig!="target/stm32.cfg" || r.adapterKhz!=4000 || r.firmware!="build/app.elf" || r.warnings.size()!=1) return 1;
    if (parseBoardProfile("firmware ../outside.bin\n").error.empty() || parseBoardProfile("source interface/../../evil.cfg\n").error.empty() || parseBoardProfile("source interface/a.cfg;exec bad\n").error.empty()) return 2;
    if (parseBoardProfile("adapter_khz 0\n").error.empty() || parseBoardProfile("source target/a.cfg\nsource target/b.cfg\n").error.empty()) return 3;
    r=parseBoardProfile("firmware a.bin\n", 5); if (!r.incomplete || !r.firmware.empty()) return 4;
    r=parseBoardProfile("# x\nfirmware a.bin\n",100,1); if (!r.incomplete || !r.firmware.empty()) return 5;
    namespace fs = std::filesystem; using namespace lucid::modules;
    const auto root = fs::temp_directory_path() / ("lucid-flash-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root); { std::ofstream out(root / "firmware.elf", std::ios::binary); out << "reviewed-firmware"; }
    bool created = false; std::string error; auto project = lucid::workspace::Project::openOrCreate(root, created, error, false, false);
    auto profile = parseBoardProfile("source interface/a.cfg\nsource target/b.cfg\nadapter_khz 2000\nfirmware firmware.elf\n");
    FirmwareSnapshot snapshot;
    if (!error.empty() || !prepareFirmware(project, profile, snapshot, error) || snapshot.bytes != "reviewed-firmware" || snapshot.sha256.size() != 64) return 6;
    { std::ofstream out(root / "firmware.elf", std::ios::binary | std::ios::trunc); out << "changed-after-review"; }
    { std::ofstream out(root / "raw.bin",std::ios::binary); out << "rawdata"; }
    const auto rawProfile=parseBoardProfile("source interface/a.cfg\nsource target/b.cfg\nfirmware raw.bin\n"); FirmwareSnapshot raw;
    if (prepareFirmware(project,rawProfile,raw,error) || !prepareFirmware(project,rawProfile,raw,error,nullptr,0x08000000) || !raw.binaryAddress || *raw.binaryAddress!=0x08000000) return 17;
    FirmwareSnapshot rejected; if (prepareFirmware(project,rawProfile,rejected,error,nullptr,0xfffffffd) || prepareFirmware(project,profile,rejected,error,nullptr,0x1000)) return 18;
    auto invalid = snapshot; invalid.sha256[0] = invalid.sha256[0] == '0' ? '1' : '0';
    if (flashFirmware(root / "absent", invalid).error.empty()) return 7;
    if (prepareFirmware(project, parseBoardProfile("source interface/a.cfg\nsource target/b.cfg\nfirmware ../outside.elf\n"), invalid, error)) return 8;
    std::atomic_bool cancel{true}; if (!flashFirmware(root / "absent", snapshot, &cancel).cancelled) return 9;
#if !defined(_WIN32)
    const auto executable = root / "fake-openocd";
    const auto scratch = root / "scratch"; fs::create_directory(scratch);
    const char* previousTmp = std::getenv("TMPDIR"); const bool hadTmp = previousTmp != nullptr;
    const std::string originalTmp = previousTmp ? previousTmp : "";
    if (::setenv("TMPDIR", scratch.c_str(), 1) != 0) return 12;
    { std::ofstream out(executable); out << "#!/bin/sh\nprintf '%s\\n' \"$@\" > \"$0.args\"\nfor d in \"$TMPDIR\"/lucid-firmware-*; do cp \"$d/firmware.elf\" \"$0.reviewed\"; : > \"$d/unexpected\"; done\nexit 0\n"; }
    fs::permissions(executable, fs::perms::owner_all);
    auto run = flashFirmware(executable, snapshot);
    if (hadTmp) ::setenv("TMPDIR", originalTmp.c_str(), 1); else ::unsetenv("TMPDIR");
    if (!run.error.empty() || run.exitCode != 0 || !fs::exists(executable.string() + ".args")) return 10;
    std::ifstream args(executable.string() + ".args"); std::string text((std::istreambuf_iterator<char>(args)), {});
    if (text.find("interface/a.cfg") == std::string::npos || text.find("target/b.cfg") == std::string::npos || text.find("verify reset exit") == std::string::npos) return 11;
    std::ifstream copied(executable.string() + ".reviewed", std::ios::binary); const std::string copiedBytes((std::istreambuf_iterator<char>(copied)), {});
    if (copiedBytes != "reviewed-firmware") return 13;
    bool retained = false;
    for (const auto& entry : fs::directory_iterator(scratch)) {
        if (entry.is_directory() && fs::exists(entry.path() / "unexpected") && !fs::exists(entry.path() / "firmware.elf")) retained = true;
    }
    if (!retained) return 14;
    fs::remove(executable.string() + ".args");
    { std::ofstream out(executable); out << "#!/bin/sh\nprintf '%s\\n' \"$@\" > \"$0.args\"\nexit 0\n"; }
    fs::permissions(executable,fs::perms::owner_all);
    auto rawRun=flashFirmware(executable,raw); if (!rawRun.error.empty() || rawRun.exitCode!=0) return 19;
    std::ifstream rawArgs(executable.string()+".args"); std::string rawText((std::istreambuf_iterator<char>(rawArgs)),{});
    if(rawText.find("firmware.bin")==std::string::npos || rawText.find("verify reset exit 0x8000000")==std::string::npos) return 20;
    fs::remove(executable.string()+".args"); auto tamperedAddress=raw; tamperedAddress.binaryAddress=0x08001000;
    if(flashFirmware(executable,tamperedAddress).error.empty() || fs::exists(executable.string()+".args")) return 21;
    auto wrongExtension = snapshot; wrongExtension.extension = ".hex";
    if (flashFirmware(executable, wrongExtension).error.empty() || fs::exists(executable.string() + ".args")) return 15;
    auto tampered = snapshot; tampered.bytes[0] ^= 1;
    if (flashFirmware(executable, tampered).error.empty() || fs::exists(executable.string() + ".args")) return 16;
#endif
    fs::remove_all(root);
    return 0;
}
