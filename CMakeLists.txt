cmake_minimum_required(VERSION 3.24)

project(Lucid
    VERSION 0.3.0
    DESCRIPTION "A minimal, GPU-accelerated code editor."
    LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJC OBJCXX)
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        set(LUCID_MACOS_MIN_VERSION "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    else()
        set(LUCID_MACOS_MIN_VERSION "14.0")
    endif()
endif()

if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(LucidOptions)
include(CompilerWarnings)
include(Dependencies)
include(Shaders)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(_config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_config} "${CMAKE_BINARY_DIR}/bin")
endforeach()

include(GNUInstallDirs)
add_subdirectory(src)

if(LUCID_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
