#include "modules/Coverage.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
using namespace lucid::modules;
int main(){namespace fs=std::filesystem; const auto root=fs::temp_directory_path()/("lucid-cobertura-branches-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);{std::ofstream f(root/"a.cpp");f<<"x\n";}bool created=false;std::string error;auto project=lucid::workspace::Project::openOrCreate(root,created,error,false,false);if(!error.empty())return 1;
 const auto parse=[&](std::string_view lines){return parseCobertura(project,"<coverage><class filename='a.cpp'><lines>"+std::string(lines)+"</lines></class></coverage>");};
 auto r=parse("<line number='1' hits='1' branch='true' condition-coverage='50% (1/2)'/><line number='2' hits='0' branch='true' condition-coverage='100.0% (2/2)'/><line number='3' hits='1' branch='false'/>");
 if(r.incomplete || r.lines.size()!=3 || r.lines[0].branchesCovered!=1 || r.lines[0].branchesUncovered!=1 || r.lines[1].branchesCovered!=2 || r.lines[1].branchesUncovered!=0 || r.lines[2].branchesCovered || r.lines[2].branchesUnknown) return 2;
 r=parse("<line number='1' hits='1' branch='true' condition-coverage='50% (1/2)'/><line number='1' hits='0' branch='true' condition-coverage='50% (1/2)'/>"); if(r.incomplete || r.lines.size()!=1 || r.lines[0].branchesCovered!=1 || r.lines[0].branchesUncovered!=1 || !r.lines[0].covered) return 3;
 r=parse("<line number='1' hits='1' branch='true' condition-coverage='50% (1/2)'/><line number='1' hits='1' branch='true' condition-coverage='100% (2/2)'/>"); if(!r.incomplete || r.lines.size()!=1 || r.lines[0].branchesCovered || r.lines[0].branchesUncovered || r.lines[0].branchesUnknown!=2) return 4;
 for(const auto* invalid:{"50% (3/2)","101% (1/2)","100.1% (1/2)","50% (0/0)","50% (1/1000001)","nan% (1/2)","0% (2/2)","50% (1/2)junk","50% (-1/2)"}) {r=parse(std::string("<line number='1' hits='1' branch='true' condition-coverage='")+invalid+"'/>");if(!r.incomplete || r.lines.size()!=1 || r.lines[0].branchesCovered || r.lines[0].branchesUncovered) return 5;}
 r=parse("<line number='1' hits='1' branch='true'/><line number='1' hits='1' branch='true' condition-coverage='50% (1/2)'/>");if(!r.incomplete || r.lines[0].branchesCovered || r.lines[0].branchesUncovered || r.lines[0].branchesUnknown!=2) return 6;
 r=parse("<line number='1' hits='1' branch='false' condition-coverage='50% (1/2)'/>");if(!r.incomplete || r.lines[0].branchesCovered) return 7;
 r=parse("<line number='1' hits='1' branch='true' condition-coverage='33% (1/3)'/>");if(r.incomplete || r.lines[0].branchesCovered!=1 || r.lines[0].branchesUncovered!=2)return 9;
 const auto method=parseCobertura(project,"<coverage><class filename='a.cpp'><methods><method name='a'><lines><line number='1' hits='0' branch='true' condition-coverage='50% (1/2)'/><line number='2' hits='1'/></lines></method><method name='b'><lines><line number='2' hits='0'/></lines></method></methods></class></coverage>");
 if(method.incomplete || method.lines.size()!=2 || method.lines[0].covered || method.lines[0].branchesCovered!=1 || method.lines[0].branchesUncovered!=1 || !method.lines[1].covered)return 10;
 const auto mixed=parseCobertura(project,"<coverage><class filename='a.cpp'><methods><method><lines><line number='1' hits='1' branch='true' condition-coverage='50% (1/2)'/><line number='1' hits='1' branch='true' condition-coverage='100% (2/2)'/><line number='2' hits='1'/></lines></method></methods><lines><line number='1' hits='0' branch='false'/></lines></class></coverage>");
 if(mixed.incomplete || mixed.lines.size()!=2 || mixed.lines[0].covered || mixed.lines[0].branchesCovered || !mixed.lines[1].covered)return 11;
 const auto bounded=parseCobertura(project,"<coverage><class filename='a.cpp'><methods><method><lines><line number='1' hits='1'/><line number='2' hits='0'/></lines></method></methods></class></coverage>",nullptr,1);
 if(!bounded.incomplete || bounded.lines.size()!=1)return 12;
 std::atomic_bool cancel{true};r=parseCobertura(project,"<coverage/>",&cancel);if(!r.incomplete)return 8;
 fs::remove_all(root);return 0;
}
