#include "modules/BoardIdentification.hpp"
int main() {
    using namespace lucid::modules;
    auto mapping=parseBoardMappings(R"({"boards":[{"name":"Board A","vendorId":"1234","productId":"aBcd","profile":"boards/a.cfg"}]})");
    if (!mapping.error.empty() || mapping.entries.size()!=1) return 1;
    auto r=identifyBoard({0x1234,0xabcd},mapping); if(r.state!=BoardIdentity::State::Match || r.name!="Board A" || r.profile!="boards/a.cfg") return 2;
    if(identifyBoard({0x1234,1},mapping).state!=BoardIdentity::State::Unknown) return 3;
    mapping.entries.push_back(mapping.entries[0]); if(identifyBoard({0x1234,0xabcd},mapping).state!=BoardIdentity::State::Ambiguous) return 4;
    if(parseBoardMappings(R"({"boards":[{"name":"A","vendorId":"1234","productId":"abcd","profile":"../x"}]})").error.empty()) return 5;
    if(parseBoardMappings(R"({"boards":[{"name":"A","vendorId":"0","productId":"abcd","profile":"x"}]})").error.empty()) return 6;
    if(parseBoardMappings(std::string(256*1024+1,' ')).error.empty()) return 7;
    return 0;
}
