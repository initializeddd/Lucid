#include "modules/TargetConfigure.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
int main(){using namespace lucid::modules;namespace fs=std::filesystem;
 auto root=fs::temp_directory_path()/("lucid-target-config-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root/"build");std::ofstream(root/"CMakeLists.txt")<<"project(test)\n";
 bool created=false;std::string error;auto project=lucid::workspace::Project::openOrCreate(root,created,error,false,false);
#if defined(_WIN32)
 const auto executable=fs::path(L"C:\\Windows\\System32\\cmd.exe"); const auto compiler=fs::path(L"C:\\sdk\\cc.exe");
#else
 const auto executable=root/"fake-cmake";std::ofstream(executable)<<"#!/bin/sh\nprintf invoked > \"$0.ran\"\nexit 0\n";fs::permissions(executable,fs::perms::owner_all);const auto compiler=fs::path("/sdk/cc");
#endif
 ToolchainProfile p;p.name="Board";p.targetTriple="arm-none-eabi";p.cCompiler=compiler;
 auto plan=prepareTargetConfigure(project,p,"build",executable);if(!plan.error.empty() || plan.arguments.size()!=5 || plan.declaredTarget!="arm-none-eabi") return 1;
 if(runTargetConfigure(project,plan,false).error.empty()) return 2;
 auto altered=plan;altered.arguments.push_back("-DUNREVIEWED=ON");if(runTargetConfigure(project,altered,true).error.empty()) return 7;
 if(prepareTargetConfigure(project,p,"../escape",executable).error.empty()) return 3;
 std::ofstream(root/"CMakeLists.txt",std::ios::trunc)<<"changed\n";if(runTargetConfigure(project,plan,true).error.empty()) return 4;
#if !defined(_WIN32)
 if(fs::exists(executable.string()+".ran")) return 5;
 std::ofstream(root/"CMakeLists.txt",std::ios::trunc)<<"project(test)\n";auto result=runTargetConfigure(project,plan,true);if(!result.error.empty() || result.exitCode!=0 || !fs::exists(executable.string()+".ran")) return 6;
#endif
 fs::remove_all(root);return 0;}
