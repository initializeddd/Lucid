#include "modules/CMakeCacheValues.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
int main(){using namespace lucid::modules;namespace fs=std::filesystem;auto root=fs::temp_directory_path()/("lucid-cache-values-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root/"build");auto file=root/"build"/"CMakeCache.txt";
 const std::string original="// help\r\nMODE:STRING=Debug\r\nMODE-STRINGS:INTERNAL=Debug;Release\r\nSDK:PATH=/old\r\n";auto put=[&](const std::string& text){std::ofstream(file,std::ios::binary|std::ios::trunc)<<text;};put(original);bool created=false;std::string error;auto p=lucid::workspace::Project::openOrCreate(root,created,error,false,false);
 std::vector<CMakeCacheValue> options;if(!listCMakeCacheValues(p,"build/CMakeCache.txt",options,error)||options.size()!=2||options[0].choices.size()!=2)return 1;
 CMakeCacheUpdate u;if(!prepareCMakeCacheValueUpdate(p,"build/CMakeCache.txt","MODE","STRING","Debug","Release",u,error)||u.replacement.find("MODE:STRING=Release\r\n")==std::string::npos)return 2;
 if(prepareCMakeCacheValueUpdate(p,"build/CMakeCache.txt","MODE","STRING","Debug","Other",u,error)||prepareCMakeCacheValueUpdate(p,"build/CMakeCache.txt","SDK","PATH","/old","x\ny",u,error))return 3;
 if(!prepareCMakeCacheValueUpdate(p,"build/CMakeCache.txt","SDK","PATH","/old","/new path",u,error))return 4;put(original+"# concurrent\n");if(saveCMakeCacheValueUpdate(p,u,error))return 5;put(original);if(!saveCMakeCacheValueUpdate(p,u,error))return 6;
 put(original+"MODE:STRING=Release\n");if(prepareCMakeCacheValueUpdate(p,"build/CMakeCache.txt","MODE","STRING","Debug","Release",u,error))return 7;
 put("MODE:STRING=Debug\nMODE-STRINGS:INTERNAL=Debug;A\\;B\n");if(listCMakeCacheValues(p,"build/CMakeCache.txt",options,error))return 8;
 fs::remove_all(root);return 0;}
