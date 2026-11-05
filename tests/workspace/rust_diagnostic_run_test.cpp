#include "modules/RustDiagnosticRun.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
int main(){using namespace lucid::modules;namespace fs=std::filesystem;
 auto root=fs::temp_directory_path()/("lucid-rust-run-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);std::ofstream(root/"Cargo.toml")<<"[package]\nname='test'\nversion='0.1.0'\n";bool created=false;std::string error;auto project=lucid::workspace::Project::openOrCreate(root,created,error,false,false);
#if defined(_WIN32)
 const auto cargo=fs::path(L"C:\\Windows\\System32\\cmd.exe");
#else
 const auto cargo=root/"fake-cargo";std::ofstream(cargo)<<"#!/bin/sh\nprintf ran > \"$0.ran\"\nprintf '%s\\n' '{\"reason\":\"compiler-message\",\"message\":{\"code\":{\"code\":\"E0382\"},\"level\":\"error\",\"message\":\"moved\",\"spans\":[],\"children\":[]}}'\nexit 1\n";fs::permissions(cargo,fs::perms::owner_all);
#endif
 auto plan=prepareRustDiagnosticRun(project,cargo);if(!plan.error.empty() || plan.arguments.size()!=6 || plan.arguments[2]!="--offline")return 1;
 if(runRustDiagnostic(project,plan,false).error.empty())return 2;
 auto tamper=plan;tamper.arguments.push_back("--help");if(runRustDiagnostic(project,tamper,true).error.empty())return 3;
 std::ofstream(root/"Cargo.toml",std::ios::trunc)<<"changed\n";if(runRustDiagnostic(project,plan,true).error.empty())return 4;
#if !defined(_WIN32)
 if(fs::exists(cargo.string()+".ran"))return 5;
 std::ofstream(root/"Cargo.toml",std::ios::trunc)<<plan.manifestSnapshot;auto result=runRustDiagnostic(project,plan,true);if(!result.error.empty() || result.process.exitCode!=1 || result.incomplete || result.diagnostics.size()!=1 || result.diagnostics[0].code!="E0382")return 6;
#endif
 fs::remove_all(root);return 0;}
