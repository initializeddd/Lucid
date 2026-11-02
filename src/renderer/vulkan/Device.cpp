#include "renderer/vulkan/Device.hpp"

#include "logging/Log.hpp"
#include "renderer/vulkan/Instance.hpp"
#include "renderer/vulkan/Surface.hpp"
#include "renderer/vulkan/VulkanCommon.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace lucid::vk {
namespace {

[[nodiscard]] std::string_view deviceTypeName(VkPhysicalDeviceType type) noexcept {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return "discrete GPU";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return "integrated GPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return "virtual GPU";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return "CPU";
    default:
        return "other";
    }
}

} // namespace

Device::Device(const Instance& instance, const Surface& surface) {
    std::uint32_t deviceCount = 0;
    LUCID_VK_CHECK(vkEnumeratePhysicalDevices(instance.handle(), &deviceCount, nullptr));
    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan-capable physical device was found");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    LUCID_VK_CHECK(
        vkEnumeratePhysicalDevices(instance.handle(), &deviceCount, devices.data()));

    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties bestProperties{};
    QueueFamilyIndices bestFamilies;
    int bestScore = -1;

    for (VkPhysicalDevice candidate : devices) {
        const QueueFamilyIndices families = findQueueFamilies(candidate, surface.handle());
        if (!families.complete() ||
            !supportsExtensions(candidate, {VK_KHR_SWAPCHAIN_EXTENSION_NAME})) {
            continue;
        }

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(candidate, &properties);

        int score = 0;
        switch (properties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            score = 1000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            score = 800;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            score = 400;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            score = 100;
            break;
        default:
            score = 0;
            break;
        }

        if (score > bestScore) {
            bestScore = score;
            bestDevice = candidate;
            bestProperties = properties;
            bestFamilies = families;
        }
    }

    if (bestDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("No Vulkan device with graphics, presentation and swapchain "
                                 "support was found");
    }

    m_physical = bestDevice;
    m_properties = bestProperties;
    m_families = bestFamilies;

    constexpr const char* kPortabilitySubsetExtension = "VK_KHR_portability_subset";

    std::vector<const char*> extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    if (supportsExtension(m_physical, kPortabilitySubsetExtension)) {
        extensions.push_back(kPortabilitySubsetExtension);
    }

    const std::set<std::uint32_t> uniqueFamilies{*m_families.graphics, *m_families.present};
    const float priority = 1.0F;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(uniqueFamilies.size());
    for (const std::uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;
        queueCreateInfos.push_back(queueInfo);
    }

    VkPhysicalDeviceFeatures features{};
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &features;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    LUCID_VK_CHECK(vkCreateDevice(m_physical, &createInfo, nullptr, &m_device));

    vkGetDeviceQueue(m_device, *m_families.graphics, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, *m_families.present, 0, &m_presentQueue);

    log::info("Graphics device: " + std::string(m_properties.deviceName) + " (" +
              std::string(deviceTypeName(m_properties.deviceType)) + ", Vulkan " +
              std::to_string(VK_API_VERSION_MAJOR(m_properties.apiVersion)) + "." +
              std::to_string(VK_API_VERSION_MINOR(m_properties.apiVersion)) + "." +
              std::to_string(VK_API_VERSION_PATCH(m_properties.apiVersion)) + ").");
}

Device::~Device() {
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
    }
}

std::uint32_t Device::findMemoryType(std::uint32_t typeFilter,
                                     VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(m_physical, &memoryProperties);

    for (std::uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index) {
        const bool supported = (typeFilter & (1U << index)) != 0U;
        const bool hasProperties =
            (memoryProperties.memoryTypes[index].propertyFlags & properties) == properties;
        if (supported && hasProperties) {
            return index;
        }
    }

    throw std::runtime_error("No suitable Vulkan memory type was found");
}

void Device::waitIdle() const {
    LUCID_VK_CHECK(vkDeviceWaitIdle(m_device));
}

QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice physical, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    std::uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &familyCount, families.data());

    for (std::uint32_t index = 0; index < familyCount; ++index) {
        if ((families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U &&
            !indices.graphics.has_value()) {
            indices.graphics = index;
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical, index, surface, &presentSupport);
        if (presentSupport == VK_TRUE && !indices.present.has_value()) {
            indices.present = index;
        }

        if (indices.complete()) {
            break;
        }
    }

    return indices;
}

bool Device::supportsExtensions(VkPhysicalDevice physical,
                                const std::vector<const char*>& required) {
    return std::all_of(required.begin(), required.end(),
                       [physical](const char* name) { return supportsExtension(physical, name); });
}

bool Device::supportsExtension(VkPhysicalDevice physical, const char* name) {
    std::uint32_t count = 0;
    if (vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, nullptr) != VK_SUCCESS) {
        return false;
    }

    std::vector<VkExtensionProperties> extensions(count);
    if (vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, extensions.data()) !=
        VK_SUCCESS) {
        return false;
    }

    return std::any_of(extensions.begin(), extensions.end(),
                       [name](const VkExtensionProperties& extension) {
                           return std::string_view(extension.extensionName) ==
                                  std::string_view(name);
                       });
}

} // namespace lucid::vk
