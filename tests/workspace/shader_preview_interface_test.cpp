#include "modules/ShaderPreviewInterface.hpp"
#include <cstdint>
#include <string>
using namespace lucid::modules;
static void put(std::string& s,std::uint32_t v) { for (unsigned i=0;i<4;++i) s.push_back(static_cast<char>(v>>(8*i))); }
int main() {
    std::string bytes; for (auto w : {0x07230203U,0x00010600U,0U,8U,0U,0x00020011U,1U,0x0003000eU,0U,1U,0x0006000fU,4U,1U,0x6e69616dU,0U,2U,0x00030010U,1U,7U,0x00040047U,2U,30U,0U,0x00030016U,3U,32U,0x00040017U,4U,3U,4U,0x00040020U,5U,3U,4U,0x0004003bU,5U,2U,3U}) put(bytes,w);
    std::string error; if (!validatePreviewFragmentInterface(bytes,error)) return 1;
    auto descriptor=bytes; for (auto w : {0x0004003bU,5U,6U,2U}) put(descriptor,w); if (validatePreviewFragmentInterface(descriptor,error)) return 2;
    auto builtin=bytes; for (auto w : {0x00040047U,2U,11U,0U}) put(builtin,w); if (validatePreviewFragmentInterface(builtin,error)) return 3;
    auto extension=bytes; for (auto w : {0x0002000aU,0U}) put(extension,w); if (validatePreviewFragmentInterface(extension,error)) return 4;
    auto output=bytes; output[88]=1; if (validatePreviewFragmentInterface(output,error)) return 5;
    return 0;
}
