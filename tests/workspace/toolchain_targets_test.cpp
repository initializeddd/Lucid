#include "modules/ToolchainTargets.hpp"
int main(){using namespace lucid::modules;
#if defined(_WIN32)
 const std::string source=R"({"profiles":[{"name":"One","target":"arm-none-eabi","cCompiler":"C:\\sdk\\cc.exe"}]})"; const std::filesystem::path cc=L"C:\\sdk\\cc.exe";
#else
 const std::string source=R"({"profiles":[{"name":"One","target":"arm-none-eabi","cCompiler":"/sdk/cc"}]})"; const std::filesystem::path cc="/sdk/cc";
#endif
 ToolchainProfile next;next.name="Two";next.targetTriple="riscv32-unknown-elf";next.cCompiler=cc;
 auto d=prepareTargetEdit(source,TargetEditKind::Add,"",next);std::vector<ToolchainProfile> parsed;std::string error;if(!d.error.empty() || d.original!=source || !parseToolchainProfiles(d.updated,parsed,error) || parsed.size()!=2 || parsed[1].name!="Two") return 1;
 d=prepareTargetEdit(d.updated,TargetEditKind::Replace,"Two",next);if(!d.error.empty()) return 2;
 d=prepareTargetEdit(source,TargetEditKind::Remove,"One");if(!d.error.empty() || !parseToolchainProfiles(d.updated,parsed,error) || !parsed.empty()) return 3;
 next.name="One";if(prepareTargetEdit(source,TargetEditKind::Add,"",next).error.empty()) return 4;
 if(prepareTargetEdit(source,TargetEditKind::Remove,"Missing").error.empty()) return 5;
 next.name="New";next.targetTriple="bad;command";if(prepareTargetEdit(source,TargetEditKind::Add,"",next).error.empty()) return 6;
 if(prepareTargetEdit("{broken",TargetEditKind::Remove,"One").error.empty()) return 7;
 next.name=std::string(5000,'x');if(prepareTargetEdit(source,TargetEditKind::Add,"",next).error.empty()) return 8;
 return 0;}
