#include "modules/SpirvInspection.hpp"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>
using namespace lucid::modules;
static void put(std::string& bytes,std::uint32_t value) { for (unsigned n=0;n<4;++n) bytes.push_back(static_cast<char>(value>>(8*n))); }
int main() {
    std::string bytes; for (auto w : {0x07230203U,0x00010600U,42U,8U,0U,0x0005000fU,5U,1U,0x6e69616dU,0U,0x00060010U,1U,17U,8U,4U,1U}) put(bytes,w);
    SpirvInspection out; std::string error;
    if (!inspectSpirv(bytes,out,error) || out.incomplete || out.entryPoints.size()!=1 || out.entryPoints[0].name!="main" || out.entryPoints[0].executionModel!=5 || !out.entryPoints[0].localSize || out.entryPoints[0].localSize->x!=8 || out.entryPoints[0].localSize->y!=4) return 1;
    auto reversed=bytes; for (std::size_t i=0;i<reversed.size();i+=4) std::reverse(reversed.begin()+static_cast<std::ptrdiff_t>(i),reversed.begin()+static_cast<std::ptrdiff_t>(i+4));
    if (!inspectSpirv(reversed,out,error) || out.entryPoints[0].name!="main" || out.entryPoints[0].localSize->x!=8) return 2;
    auto malformed=bytes; malformed[20]=0; malformed[21]=0; malformed[22]=0; malformed[23]=0;
    if (inspectSpirv(malformed,out,error) || error.empty() || !out.entryPoints.empty()) return 3;
    malformed=bytes; malformed.resize(malformed.size()-4); if (inspectSpirv(malformed,out,error)) return 4;
    malformed=bytes; malformed[28]=8; if (inspectSpirv(malformed,out,error)) return 5;
    std::atomic_bool cancelled{true}; if (inspectSpirv(bytes,out,error,&cancelled) || error.empty() || !out.entryPoints.empty()) return 6;
    std::string unknown=bytes; put(unknown,0x0003014bU); put(unknown,1); put(unknown,38);
    if (!inspectSpirv(unknown,out,error) || !out.incomplete) return 7;
    malformed=bytes; malformed[36]=1; if (inspectSpirv(malformed,out,error)) return 8;
    return 0;
}
