#include "modules/ApiRequests.hpp"
#include "workspace/Project.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
using namespace lucid::modules;
int main() {
    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/("lucid-api-export-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root/"out"); bool created=false; std::string error;
    auto project=lucid::workspace::Project::openOrCreate(root,created,error);
    ApiResponse response; response.status=200; response.body=std::string("hello\0binary\xff",13);
    if (!saveApiResponseBody(project,"out/data.bin",response,error)) return 1;
    { std::ifstream f(root/"out/data.bin",std::ios::binary); const std::string bytes((std::istreambuf_iterator<char>(f)),{}); if(bytes!=response.body) return 2; }
    response.body="overwrite"; if(saveApiResponseBody(project,"out/data.bin",response,error)) return 3;
    if(saveApiResponseBody(project,"../escape.bin",response,error) || saveApiResponseBody(project,"missing/file.bin",response,error)) return 4;
    response.error="Response exceeds size limit"; if(saveApiResponseBody(project,"out/partial.bin",response,error)) return 5;
    response.error.clear(); std::atomic_bool cancelled{true}; if(saveApiResponseBody(project,"out/cancel.bin",response,error,&cancelled)) return 6;
    response.body=std::string(4U*1024U*1024U+1,'x'); if(saveApiResponseBody(project,"out/large.bin",response,error)) return 7;
    response.body="x"; std::error_code ec; fs::create_directory_symlink(root/"out",root/"alias",ec);
    if(!ec && saveApiResponseBody(project,"alias/link.bin",response,error)) return 8;
    fs::remove_all(root); return 0;
}
