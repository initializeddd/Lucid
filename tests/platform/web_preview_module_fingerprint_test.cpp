#include "modules/ModuleFingerprint.hpp"
int main() {
    lucid::modules::Module module; module.id="ab"; module.name="c"; module.valid=true; module.enabled=true; module.project=true; module.network=true; module.commands.push_back({"Preview","",false,"web-preview"});
    const auto first=lucid::modules::moduleFingerprint(module); if (first.size()!=64 || first!=lucid::modules::moduleFingerprint(module)) return 1;
    auto changed=module; changed.id="a"; changed.name="bc"; if (first==lucid::modules::moduleFingerprint(changed)) return 2;
    changed=module; changed.network=false; if (first==lucid::modules::moduleFingerprint(changed)) return 3;
    changed=module; changed.commands[0].action="other"; if (first==lucid::modules::moduleFingerprint(changed)) return 4;
    changed=module; changed.enabled=false; if (first==lucid::modules::moduleFingerprint(changed)) return 5;
    return 0;
}
