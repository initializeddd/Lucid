#include "modules/DependencyGoZip.hpp"
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>
using namespace lucid::modules;
namespace {
void u16(std::string& out, std::uint16_t x) { out += static_cast<char>(x); out += static_cast<char>(x >> 8); }
void u32(std::string& out, std::uint32_t x) { u16(out, static_cast<std::uint16_t>(x)); u16(out, static_cast<std::uint16_t>(x >> 16)); }
std::uint32_t crc(std::string_view data) { std::uint32_t c=0xffffffffU; for (unsigned char b:data) { c^=b; for (int i=0;i<8;++i) c=(c>>1)^(0xedb88320U & (0U-(c&1U))); } return c^0xffffffffU; }
std::string zip(const std::vector<std::pair<std::string,std::string>>& files) {
    std::string out, directory;
    for (const auto& [name,body]:files) {
        const auto offset=static_cast<std::uint32_t>(out.size()), checksum=crc(body), size=static_cast<std::uint32_t>(body.size());
        u32(out,0x04034b50); u16(out,20); u16(out,0); u16(out,0); u16(out,0); u16(out,0); u32(out,checksum); u32(out,size); u32(out,size); u16(out,static_cast<std::uint16_t>(name.size())); u16(out,0); out+=name; out+=body;
        u32(directory,0x02014b50); u16(directory,20); u16(directory,20); u16(directory,0); u16(directory,0); u16(directory,0); u16(directory,0); u32(directory,checksum); u32(directory,size); u32(directory,size); u16(directory,static_cast<std::uint16_t>(name.size())); u16(directory,0); u16(directory,0); u16(directory,0); u16(directory,0); u32(directory,0); u32(directory,offset); directory+=name;
    }
    const auto offset=static_cast<std::uint32_t>(out.size()); out+=directory;
    u32(out,0x06054b50); u16(out,0); u16(out,0); u16(out,static_cast<std::uint16_t>(files.size())); u16(out,static_cast<std::uint16_t>(files.size())); u32(out,static_cast<std::uint32_t>(directory.size())); u32(out,offset); u16(out,0); return out;
}
}
int main() {
    const std::string prefix="example.com/mod@v1.2.3/";
    const auto data=zip({{prefix+"b.txt","b"},{prefix+"a.txt","a"}}); std::string hash,error;
    if (!hashGoModuleZip(data,"example.com/mod","v1.2.3",hash,error) || hash!="h1:82A/FsHv6RvOwra/JC3KzovckJhMajGHQzw2BqZgCFk=") return 1;
    if (hashGoModuleZip(zip({{prefix+"a","x"},{prefix+"a","y"}}),"example.com/mod","v1.2.3",hash,error)) return 2;
    if (hashGoModuleZip(zip({{prefix+"../escape","x"}}),"example.com/mod","v1.2.3",hash,error)) return 3;
    if (hashGoModuleZip(data,"example.com/other","v1.2.3",hash,error)) return 4;
    auto corrupt=data; corrupt[30+prefix.size()+5]^=1; if (hashGoModuleZip(corrupt,"example.com/mod","v1.2.3",hash,error)) return 5;
    auto bomb=data; const auto cd=bomb.find("PK\x01\x02"); if (cd==std::string::npos) return 6;
    bomb[cd+24]=1; bomb[cd+25]=0; bomb[cd+26]=0; bomb[cd+27]=1; // >16 MiB claimed expansion
    if (hashGoModuleZip(bomb,"example.com/mod","v1.2.3",hash,error)) return 7;
    if (hashGoModuleZip(data.substr(0,data.size()-1),"example.com/mod","v1.2.3",hash,error)) return 8;
    std::atomic_bool cancelled{true}; if (hashGoModuleZip(data,"example.com/mod","v1.2.3",hash,error,&cancelled)) return 9;
    const auto fixture=std::filesystem::path(__FILE__).parent_path().parent_path()/"fixtures/go-module-uppercase-v3.2.1.zip";
    std::ifstream in(fixture,std::ios::binary); const std::string real((std::istreambuf_iterator<char>(in)),{});
    if (!in || !hashGoModuleZip(real,"github.com/Masterminds/semver/v3","v3.2.1",hash,error) || hash!="h1:RN9w6+7QoMeJVGyfmbcgs28Br8cvmnucEXnY0rYXWg0=") return 10;
    return 0;
}
