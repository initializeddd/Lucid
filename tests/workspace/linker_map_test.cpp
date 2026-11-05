#include "modules/LinkerMap.hpp"
#include <string>
using namespace lucid::modules;
int main() {
    const std::string map = "Memory Configuration\n\nName Origin Length Attributes\nFLASH 0x08000000 0x00000100 xr\nRAM 0x20000000 0x00000080 xrw\n*default* 0x0 0xffffffffffffffff\n\nLinker script and memory map\n.text 0x08000000 0x20\n .text.foo 0x08000000 0x10 foo.o\n.overlap 0x08000010 0x20\n.data\n 0x20000000 0x10 load address 0x08000030\n.debug 0x00000000 0x05\n";
    LinkerMap result; std::string error;
    bool ok = parseGnuLinkerMap(map, result, error) && result.regions.size() == 2 && result.sections.size() == 4 && result.regions[0].used == 0x30 && result.regions[1].used == 0x10 && result.incomplete;
    ok = ok && result.sections[2].name == ".data" && result.sections[2].region == 1 && result.sections[2].hasLoadAddress && result.sections[2].loadAddress == 0x08000030 && result.sections[2].loadRegion == 0 && result.regions[0].loadUsed == 0x10 && result.loadIncomplete && result.sections[3].region == -1;
    ok = ok && !parseGnuLinkerMap("Memory Configuration\nName Origin Length Attributes\nX 0xffffffffffffffff 0x2\nLinker script and memory map\n", result, error) && result.regions.empty();
    ok = ok && !parseGnuLinkerMap("Memory Configuration\nName Origin Length Attributes\nX 0x0 0x100\nLinker script and memory map\n.x 0xffffffffffffffff 0x2\n", result, error);
    ok = ok && !parseGnuLinkerMap("not a map", result, error);
    const std::string ambiguous = "Memory Configuration\nName Origin Length Attributes\nA 0x0 0x100\nB 0x0 0x100\nLinker script and memory map\n.x 0x10 0x10\n";
    ok = ok && parseGnuLinkerMap(ambiguous, result, error) && result.incomplete && result.sections[0].region == -1;
    const std::string loadOverlap = "Memory Configuration\nName Origin Length Attributes\nFLASH 0x1000 0x100\nRAM 0x2000 0x100\nLinker script and memory map\n.a 0x2000 0x20 load address 0x1000\n.b 0x2020 0x20 load address 0x1010\n";
    ok = ok && parseGnuLinkerMap(loadOverlap, result, error) && !result.loadIncomplete && result.regions[0].loadUsed == 0x30 && result.regions[1].used == 0x40;
    const std::string badLoad = "Memory Configuration\nName Origin Length Attributes\nX 0x0 0x100\nLinker script and memory map\n.x 0x0 0x10 load address 0xffffffffffffffff\n";
    ok = ok && !parseGnuLinkerMap(badLoad, result, error);
    const std::string named = "Memory Configuration\nName Origin Length Attributes\nFLASH 0x1000 0x100\nLinker script and memory map\ntext 0x1000 0x10\nlong_name\n 0X1010 0x20\n. = 0x1030\n";
    ok = ok && parseGnuLinkerMap(named, result, error) && !result.incomplete && result.sections.size() == 2 && result.sections[0].name == "text" && result.sections[1].name == "long_name" && result.regions[0].used == 0x30;
    const std::string dropped = "Memory Configuration\nName Origin Length Attributes\nFLASH 0x0 0x100\nLinker script and memory map\n.long_name\nignored\n.text 0x0 0x10\n";
    ok = ok && parseGnuLinkerMap(dropped, result, error) && result.incomplete;
    return ok ? 0 : 1;
}
