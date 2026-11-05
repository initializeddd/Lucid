#include "modules/ModuleFingerprint.hpp"
#include <picosha2.h>
#include <string_view>
namespace lucid::modules {
std::string moduleFingerprint(const Module& module) {
    picosha2::hash256_one_by_one hash;
    const auto add=[&](std::string_view value) { const auto length=std::to_string(value.size())+":"; hash.process(length.begin(),length.end()); hash.process(value.begin(),value.end()); };
    add(module.id); add(module.name); add(module.version); add(module.description); add(module.error);
    add(module.project?"1":"0"); add(module.network?"1":"0"); add(module.processes?"1":"0"); add(module.settings?"1":"0"); add(module.devices?"1":"0"); add(module.enabled?"1":"0"); add(module.valid?"1":"0");
    add(std::to_string(module.requirements.size())); for (const auto& requirement:module.requirements) add(requirement);
    add(std::to_string(module.commands.size())); for (const auto& command:module.commands) { add(command.title); add(command.insert); add(command.snippet?"1":"0"); add(command.action); }
    hash.finish(); return picosha2::get_hash_hex_string(hash);
}
}
