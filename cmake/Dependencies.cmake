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

# Bundle the inference engine; users do not need a separate model server.
include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
FetchContent_Declare(llama
    URL https://github.com/ggml-org/llama.cpp/archive/4762ad7316dcdec20016ab5985fb46a27902204d.tar.gz
    URL_HASH SHA256=06b6a3a1c2d12becc05d02c701274ab1f6d033399b4a20a166b919289e19233c
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(llama)
# This pinned llama.cpp version uses UTF-8 literals as char strings. MSVC's
# C++20 char8_t mode changes their type, unlike the library's intended C++17.
set_target_properties(llama PROPERTIES CXX_STANDARD 17)

set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
set(CURL_ZLIB OFF CACHE BOOL "" FORCE)
set(CURL_BROTLI OFF CACHE BOOL "" FORCE)
set(CURL_ZSTD OFF CACHE BOOL "" FORCE)
set(USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
set(USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAP ON CACHE BOOL "" FORCE)
set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "" FORCE)
set(BUILD_MISC_DOCS OFF CACHE BOOL "" FORCE)
if(WIN32)
    set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
    set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
elseif(APPLE)
    set(CURL_USE_SECTRANSP ON CACHE BOOL "" FORCE)
    set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
else()
    set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(curl
    URL https://github.com/curl/curl/archive/57495c64871d18905a0941db9196ef90bafe9a29.tar.gz
    URL_HASH SHA256=0c7df3d2730de54d3e8eaa557c04ba60c5b68dab7a3904afd45b8de2bb72dc47
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(curl)
