#pragma once
#include <string>
#include <string_view>
namespace lucid::credentials {
bool save(std::string_view key, std::string& error);
bool load(std::string& key, std::string& error);
bool remove(std::string& error);
}
