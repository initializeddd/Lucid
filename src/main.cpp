#include "app/Application.hpp"
#include "logging/Log.hpp"
#include "platform/macos/VulkanEnvironment.hpp"

#include <exception>
#include <string>

int main() {
    lucid::platform::configureVulkanDriverEnvironment();

    try {
        lucid::ApplicationConfig config;
        config.window.width = 1280;
        config.window.height = 820;
        config.window.title = "Lucid";

        lucid::Application application(config);
        application.run();
        return 0;
    } catch (const std::exception& exception) {
        lucid::log::error(std::string("Fatal: ") + exception.what());
        return 1;
    }
}
