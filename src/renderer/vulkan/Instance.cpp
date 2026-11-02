#include "renderer/vulkan/Instance.hpp"

#include "logging/Log.hpp"
#include "renderer/vulkan/VulkanCommon.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace lucid::vk {
namespace {

[[nodiscard]] std::vector<VkExtensionProperties> instanceExtensions() {
    std::uint32_t count = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS) {
        return {};
    }

    std::vector<VkExtensionProperties> extensions(count);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data()) != VK_SUCCESS) {
        return {};
    }
    extensions.resize(count);
    return extensions;
}

[[nodiscard]] std::vector<VkLayerProperties> instanceLayers() {
    std::uint32_t count = 0;
    if (vkEnumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS) {
        return {};
    }

    std::vector<VkLayerProperties> layers(count);
    if (vkEnumerateInstanceLayerProperties(&count, layers.data()) != VK_SUCCESS) {
        return {};
    }
    layers.resize(count);
    return layers;
}

void addUniqueExtension(std::vector<const char*>& extensions, const char* name,
                        const std::vector<VkExtensionProperties>& available) {
    const bool supported =
        std::any_of(available.begin(), available.end(), [name](const VkExtensionProperties& ext) {
            return std::string_view(ext.extensionName) == name;
        });
    if (!supported) {
        return;
    }

    const bool alreadyPresent =
        std::any_of(extensions.begin(), extensions.end(), [name](const char* extension) {
            return std::string_view(extension) == std::string_view(name);
        });
    if (!alreadyPresent) {
        extensions.push_back(name);
    }
}

} // namespace

Instance::Instance(std::string_view appName, std::uint32_t appVersion, bool enableValidation,
                   const std::vector<const char*>& requiredExtensions) {
    const std::vector<VkExtensionProperties> availableExtensions = instanceExtensions();
    const std::vector<VkLayerProperties> availableLayers = instanceLayers();

    const bool validationAvailable =
        std::any_of(availableLayers.begin(), availableLayers.end(),
                    [](const VkLayerProperties& layer) {
                        return std::string_view(layer.layerName) == "VK_LAYER_KHRONOS_validation";
                    });

    m_validationEnabled = enableValidation && validationAvailable;
    if (enableValidation && !validationAvailable) {
        log::warn("Vulkan validation layer is not available; continuing without it.");
    }

    std::vector<const char*> extensions = requiredExtensions;
    addUniqueExtension(extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
                       availableExtensions);

    const bool hasDebugUtils =
        std::any_of(availableExtensions.begin(), availableExtensions.end(),
                    [](const VkExtensionProperties& ext) {
                        return std::string_view(ext.extensionName) ==
                               VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
                    });
    if (m_validationEnabled && hasDebugUtils) {
        addUniqueExtension(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME, availableExtensions);
    }

    std::vector<const char*> layers;
    if (m_validationEnabled) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    const std::string appNameString(appName);
    VkApplicationInfo applicationInfo{};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = appNameString.c_str();
    applicationInfo.applicationVersion = appVersion;
    applicationInfo.pEngineName = "Lucid";
    applicationInfo.engineVersion = appVersion;
    applicationInfo.apiVersion = VK_API_VERSION_1_2;

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugCreateInfo.pfnUserCallback = debugCallback;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &applicationInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.data();

    const bool portabilityEnumeration =
        std::find(extensions.begin(), extensions.end(),
                  VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) != extensions.end();
    if (portabilityEnumeration) {
        createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
    if (m_validationEnabled && hasDebugUtils) {
        createInfo.pNext = &debugCreateInfo;
    }

    LUCID_VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_instance));

    if (m_validationEnabled && hasDebugUtils) {
        auto* createMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
        m_destroyDebugMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));

        if (createMessenger != nullptr && m_destroyDebugMessenger != nullptr) {
            const VkResult result = createMessenger(m_instance, &debugCreateInfo, nullptr,
                                                    &m_debugMessenger);
            if (result != VK_SUCCESS) {
                log::warn("Failed to create the Vulkan debug messenger.");
                m_debugMessenger = VK_NULL_HANDLE;
            }
        }
    }

    log::info("Vulkan instance created (validation " +
              std::string(m_validationEnabled ? "enabled" : "disabled") + ").");
}

Instance::~Instance() {
    if (m_debugMessenger != VK_NULL_HANDLE && m_destroyDebugMessenger != nullptr) {
        m_destroyDebugMessenger(m_instance, m_debugMessenger, nullptr);
    }
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL
Instance::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                        VkDebugUtilsMessageTypeFlagsEXT /*type*/,
                        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
                        void* /*userData*/) {
    const char* message =
        callbackData != nullptr && callbackData->pMessage != nullptr ? callbackData->pMessage
                                                                     : "Unknown validation message";

    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0U) {
        log::error(message);
    } else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0U) {
        log::warn(message);
    }

    return VK_FALSE;
}

} // namespace lucid::vk
