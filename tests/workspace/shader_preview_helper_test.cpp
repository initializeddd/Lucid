#include "modules/ShaderPreview.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif
using namespace lucid::modules;
int main(int argc,char** argv) {
    if (argc==4) {
#if defined(_WIN32)
        if (_setmode(_fileno(stdout), _O_BINARY)==-1) return 11;
#endif
        const std::string path(argv[1]); if (path.find("slow")!=std::string::npos) std::this_thread::sleep_for(std::chrono::seconds(2));
        if (path.find("bad")!=std::string::npos) { std::cout << "bad"; return 0; }
        std::cout << "P6\n2 2\n255\n"; const char pixels[12]={static_cast<char>(255),0,0,static_cast<char>(255),0,0,static_cast<char>(255),0,0,static_cast<char>(255),0,0}; std::cout.write(pixels,12); return 0;
    }
    const auto helper=argc==3?std::filesystem::absolute(argv[1]):std::filesystem::absolute(argv[0]);
    const auto artifact=argc==3?std::filesystem::absolute(argv[2]):std::filesystem::temp_directory_path()/"fake.spv";
    ShaderPreviewImage image; std::string error;
    if (runShaderPreviewHelper(helper,artifact,2,2,false,image,error) || error.empty() || !image.rgb.empty()) return 1;
    if (!runShaderPreviewHelper(helper,artifact,2,2,true,image,error) || image.rgb.size()!=12 || image.rgb[0]!=255 || image.rgb[1]!=0 || image.rgb[2]!=0) return 2;
    for (std::size_t i=0;i<image.rgb.size();i+=3) if (image.rgb[i]!=255 || image.rgb[i+1]!=0 || image.rgb[i+2]!=0) return 3;
    if (runShaderPreviewHelper(helper,artifact,513,2,true,image,error) || !image.rgb.empty()) return 4;
    std::atomic_bool cancel{true}; if (runShaderPreviewHelper(helper,artifact,2,2,true,image,error,std::chrono::seconds(1),&cancel) || error.find("cancel") == std::string::npos) return 5;
    if (argc!=3) {
        if (runShaderPreviewHelper(helper,std::filesystem::temp_directory_path()/"bad",2,2,true,image,error) || error.find("invalid pixels")==std::string::npos) return 6;
        if (runShaderPreviewHelper(helper,std::filesystem::temp_directory_path()/"slow",2,2,true,image,error,std::chrono::milliseconds(100)) || error.find("timed out")==std::string::npos) return 7;
    }
    std::string shader;
    if (argc==3) { std::ifstream file(artifact,std::ios::binary); shader.assign(std::istreambuf_iterator<char>(file),{}); }
    else for (auto w : {0x07230203U,0x00010600U,0U,8U,0U,0x00020011U,1U,0x0003000eU,0U,1U,0x0006000fU,4U,1U,0x6e69616dU,0U,2U,0x00030010U,1U,7U,0x00040047U,2U,30U,0U,0x00030016U,3U,32U,0x00040017U,4U,3U,4U,0x00040020U,5U,3U,4U,0x0004003bU,5U,2U,3U}) for (unsigned i=0;i<4;++i) shader.push_back(static_cast<char>(w>>(8*i)));
    if (runStagedShaderPreview(helper,shader,2,2,false,image,error) || error.empty()) return 8;
    if (!runStagedShaderPreview(helper,shader,2,2,true,image,error) || image.rgb.size()!=12 || image.rgb[0]!=255) return 9;
    if (runStagedShaderPreview(helper,"bad",2,2,true,image,error) || error.empty() || !image.rgb.empty()) return 10;
    return 0;
}
