#include "modules/ToolchainProfiles.hpp"
#include <string>
using namespace lucid::modules;
int main() {
    std::vector<ToolchainProfile> profiles; std::string error;
#if defined(_WIN32)
    const auto valid=R"({"profiles":[{"name":"Board","target":"arm-none-eabi","toolchainFile":"C:\\sdk\\toolchain.cmake","sysroot":"C:\\sdk\\root"}]})";
#else
    const auto valid=R"({"profiles":[{"name":"Board","target":"arm-none-eabi","toolchainFile":"/sdk/toolchain.cmake","sysroot":"/sdk/root"}]})";
#endif
    if (!parseToolchainProfiles(valid,profiles,error) || profiles.size()!=1 || profiles[0].targetTriple!="arm-none-eabi" || profiles[0].cmakeToolchainFile.empty()) return 1;
    if (parseToolchainProfiles(R"({"profiles":[{"name":"a","target":"arm;bad","cCompiler":"/cc"}]})",profiles,error) || !profiles.empty()) return 2;
    if (parseToolchainProfiles(R"({"profiles":[{"name":"a","target":"arm-none-eabi","toolchainFile":"relative"}]})",profiles,error)) return 3;
    if (parseToolchainProfiles(R"({"profiles":[{"name":"a","target":"arm","cCompiler":"/cc","extra":"x"}]})",profiles,error)) return 4;
    if (parseToolchainProfiles(R"({"profiles":[{"name":"a","target":"arm"}]})",profiles,error)) return 5;
    if (parseToolchainProfiles(std::string(1024U*1024U+1,'x'),profiles,error)) return 6;
    if (!parseToolchainProfiles(R"({"profiles":[]})",profiles,error) || !profiles.empty()) return 7;
    if (parseToolchainProfiles(R"({"profiles":[{"name":"a\ncorrupt","target":"arm","toolchainFile":"/cc"}]})",profiles,error)) return 8;
    return 0;
}
