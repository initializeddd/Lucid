include_guard(GLOBAL)

find_package(Threads REQUIRED)

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew /usr/local)

    # Prefer a locally installed LunarG SDK when one is present. The loader
    # still resolves the MoltenVK ICD at runtime, but pointing FindVulkan at
    # the SDK keeps headers, loader and validation layers in sync.
    if(NOT DEFINED ENV{VULKAN_SDK})
        file(GLOB _lucid_vulkan_sdks LIST_DIRECTORIES true "$ENV{HOME}/VulkanSDK/*")
        if(_lucid_vulkan_sdks)
            list(SORT _lucid_vulkan_sdks COMPARE NATURAL ORDER DESCENDING)
            list(GET _lucid_vulkan_sdks 0 _lucid_vulkan_sdk)
            if(EXISTS "${_lucid_vulkan_sdk}/macOS")
                set(ENV{VULKAN_SDK} "${_lucid_vulkan_sdk}/macOS")
            endif()
        endif()
    endif()
endif()

find_package(glfw3 3.3 CONFIG QUIET)

if(NOT TARGET glfw)
    include(FetchContent)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.5.1
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(glfw)
endif()

if(NOT TARGET glfw)
    message(FATAL_ERROR "GLFW was not found and could not be fetched.")
endif()

find_package(Vulkan 1.2 REQUIRED)

message(STATUS "Lucid: GLFW ${glfw3_VERSION}")
message(STATUS "Lucid: Vulkan ${Vulkan_VERSION}")
