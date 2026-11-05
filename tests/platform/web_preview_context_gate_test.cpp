#include "platform/browser/WebPreviewContextGate.hpp"
using namespace lucid::platform;
int main() {
    WebPreviewContextGate gate; WebPreviewContext context{"project","page.html","module",std::string(64,'a'),1,2}; std::uint64_t token=0;
    if (gate.begin(context,true,false,token) || gate.active()) return 1;
    if (gate.begin(context,false,true,token) || gate.active()) return 2;
    if (!gate.begin(context,true,true,token) || !gate.matches(context,true,token) || gate.matches(context,false,token)) return 3;
    auto changed=context; changed.documentRevision++; if (gate.matches(changed,true,token)) return 4;
    changed=context; changed.moduleFingerprint="new"; if (gate.matches(changed,true,token)) return 5;
    changed=context; changed.uiGeneration++; if (gate.matches(changed,true,token)) return 6;
    gate.cancel(); if (gate.matches(context,true,token)) return 7;
    const auto old=token; if (!gate.begin(context,true,true,token) || token==old || gate.matches(context,true,old)) return 8;
    gate.cancel(); auto invalid=context; invalid.moduleFingerprint="short"; if (gate.begin(invalid,true,true,token)) return 9;
    invalid=context; invalid.moduleFingerprint[0]='z'; if (gate.begin(invalid,true,true,token)) return 10;
    context.project.clear(); if (gate.begin(context,true,true,token)) return 11;
    return 0;
}
