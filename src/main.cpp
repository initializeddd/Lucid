#include "app/Application.hpp"
#include "platform/app/PlatformApp.hpp"
#include "logging/Log.hpp"
#include "text/font/FontAtlas.hpp"
#include "platform/macos/VulkanEnvironment.hpp"

#include <exception>
#include <string>

int main(int argc, char* argv[]) {
    if (argc > 0 && argv[0]) lucid::text::setExecutablePath(argv[0]);
    lucid::platform::prepareOpenFiles();
    lucid::platform::configureVulkanDriverEnvironment();

    try {
        lucid::ApplicationConfig config;
        if (argc > 1 && argv[1][0] != '-') config.initialPath = argv[1];
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
