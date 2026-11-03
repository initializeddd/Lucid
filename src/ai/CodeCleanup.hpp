#pragma once
#include "editor/document/Document.hpp"
#include <string>
#include <string_view>
namespace lucid::ai {
// Build a bounded request for the selected code with nearby file style examples.
[[nodiscard]] bool cleanupRequest(const editor::Document& document, std::string_view file,
                                  std::string& request, std::string& error);
[[nodiscard]] bool cleanupResponse(std::string& response, std::string& error);
}
