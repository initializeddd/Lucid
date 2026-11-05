#include "modules/CMakeBuild.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
int main(){using namespace lucid::modules;namespace fs=std::filesystem;
 auto root=fs::temp_directory_path()/("lucid-cmake-build-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));auto build=root/"build",reply=build/".cmake/api/v1/reply";fs::create_directories(reply);std::ofstream(root/"CMakeLists.txt")<<"project(test)\n";
 std::ofstream(reply/"index-1.json")<<R"({"objects":[{"kind":"codemodel","version":{"major":2},"jsonFile":"model.json"}]})";
 std::ofstream(reply/"model.json")<<"{\"paths\":{\"source\":\""<<root.generic_string()<<"\",\"build\":\""<<build.generic_string()<<"\"},\"configurations\":[{\"name\":\"Debug\",\"targets\":[{\"jsonFile\":\"target.json\"}]}]}";
 std::ofstream(reply/"target.json")<<R"({"name":"demo","type":"EXECUTABLE"})";
 bool created=false;std::string error;auto project=lucid::workspace::Project::openOrCreate(root,created,error,false,false);
#if defined(_WIN32)
 auto executable=fs::path(L"C:\\Windows\\System32\\cmd.exe");
#else
 auto executable=root/"fake-cmake";std::ofstream(executable)<<"#!/bin/sh\nprintf invoked > \"$0.ran\"\nexit 0\n";fs::permissions(executable,fs::perms::owner_all);
#endif
 auto plan=prepareCMakeBuild(project,"build","demo","Debug",executable);if(!plan.error.empty() || plan.arguments.size()!=6 || plan.arguments[2]!="--target")return 1;
 if(runCMakeBuild(project,plan,false).error.empty() || prepareCMakeBuild(project,"build","-bad","Debug",executable).error.empty())return 2;
 auto altered=plan;altered.arguments.push_back("--clean-first");if(runCMakeBuild(project,altered,true).error.empty())return 3;
 std::ofstream(root/"CMakeLists.txt",std::ios::trunc)<<"changed\n";if(runCMakeBuild(project,plan,true).error.empty())return 4;
#if !defined(_WIN32)
 if(fs::exists(executable.string()+".ran"))return 5;std::ofstream(root/"CMakeLists.txt",std::ios::trunc)<<"project(test)\n";auto result=runCMakeBuild(project,plan,true);if(!result.error.empty() || result.exitCode || !fs::exists(executable.string()+".ran"))return 6;
#endif
 fs::remove_all(root);return 0;}
