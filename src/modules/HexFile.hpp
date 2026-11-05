#pragma once
#include "modules/HexBuffer.hpp"
#include "workspace/Project.hpp"
#include <atomic>
#include <string>
namespace lucid::modules {
class HexFile {
public:
    [[nodiscard]] bool open(const workspace::Project& project, std::string_view path, std::string& error, const std::atomic_bool* cancel = nullptr);
    [[nodiscard]] bool save(workspace::Project& project, std::string& error, const std::atomic_bool* cancel = nullptr);
    [[nodiscard]] HexBuffer& buffer() { return buffer_; }
    [[nodiscard]] const HexBuffer& buffer() const { return buffer_; }
private:
    HexBuffer buffer_; std::string path_, original_; std::filesystem::path root_; bool opened_ = false;
};
}
