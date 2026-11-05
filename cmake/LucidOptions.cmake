include_guard(GLOBAL)

option(LUCID_ENABLE_VALIDATION "Enable Vulkan validation layers" ON)
option(LUCID_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(LUCID_BUILD_TESTS "Build Lucid's test suite" ON)
option(LUCID_ENABLE_WEBVIEW2 "Build the unexposed Windows WebView2 preview adapter for testing" OFF)
option(LUCID_WEBVIEW2_UI_TEST "Compile the unexposed Windows preview UI plumbing for runtime tests" OFF)
set(LUCID_WEBVIEW2_SDK_DIR "" CACHE PATH "Directory containing the pinned WebView2 header, loader and license")

if(LUCID_ENABLE_VALIDATION)
    add_compile_definitions(LUCID_ENABLE_VALIDATION=1)
else()
    add_compile_definitions(LUCID_ENABLE_VALIDATION=0)
endif()

add_compile_definitions($<$<CONFIG:Debug>:LUCID_DEBUG=1>)
