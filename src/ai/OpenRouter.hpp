#pragma once
#include <atomic>
#include <string>
#include <string_view>
namespace lucid::ai {
[[nodiscard]] std::string openRouterRequest(std::string_view model, std::string_view prompt);
[[nodiscard]] bool parseOpenRouterResponse(std::string_view json, std::string& output, std::string& error);
// Explicit opt-in: sends prompt to OpenRouter. Never logs or persists the key.
[[nodiscard]] bool generateOpenRouter(std::string_view key, std::string_view model, std::string_view prompt,
                                      std::string& output, std::string& error, const std::atomic_bool* cancel = nullptr);
}
